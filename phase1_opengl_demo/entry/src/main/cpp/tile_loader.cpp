#include "tile_loader.h"
#include <curl/curl.h>
#include <cstring>
#include <cstdlib>
#include <atomic>
#include <mutex>
#include <vector>
#include <map>
#include <chrono>
#include <thread>

namespace maplibre {

// ============================================================
// 内部结构
// ============================================================

struct TileCacheEntry {
    uint8_t* data;
    size_t len;
    std::chrono::steady_clock::time_point timestamp;
    bool fromCache; // 是否是从缓存返回的
};

struct DownloadContext {
    uint8_t* buffer;
    size_t size;
    size_t capacity;
    std::function<void(const uint8_t*, size_t, bool)> callback;
    std::atomic<bool> cancelled{false};
    bool fromCache;
    int x, y, z;
    std::string url;
    // 重试
    int retryCount;
    std::string etag;
    std::chrono::steady_clock::time_point lastRequest;
};

struct TileLoader::Impl {
    // LRU 内存缓存
    std::map<std::string, TileCacheEntry> memCache_;
    std::mutex cacheMutex_;
    static constexpr size_t MAX_CACHE = 32;

    // 并发控制
    std::atomic<int> activeDownloads_{0};
    int maxConcurrency_ = 4;

    // 代理
    std::string proxyHost_;
    int proxyPort_ = 0;
    bool useProxy_ = false;

    // curl multi
    CURLM* multi_;
    std::mutex multiMutex_;
    std::atomic<bool> running_{false};

    // 已发出的请求（用于取消）
    std::vector<CURL*> activeHandles_;
    std::mutex handlesMutex_;

    ~Impl() {
        // 清理缓存
        std::lock_guard<std::mutex> lock(cacheMutex_);
        for (auto& kv : memCache_) {
            if (kv.second.data) free(kv.second.data);
        }
        memCache_.clear();
        if (multi_) curl_multi_cleanup(multi_);
    }

    std::string makeKey(int x, int y, int z) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%d/%d/%d", z, x, y);
        return std::string(buf);
    }

    std::string makeUrl(int x, int y, int z, const std::string& key) {
        char url[512];
        snprintf(url, sizeof(url),
                 "https://t0.tianditu.gov.cn/DataServer?T=vec_w&x=%d&y=%d&l=%d&tk=%s",
                 x, y, z, key.c_str());
        return std::string(url);
    }

    // LRU 缓存查找
    TileCacheEntry* findInCache(const std::string& key) {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = memCache_.find(key);
        if (it != memCache_.end()) {
            // 更新访问时间（提升到最新）
            TileCacheEntry ent = it->second;
            memCache_.erase(it);
            memCache_[key] = ent;
            return &memCache_[key];
        }
        return nullptr;
    }

    // LRU 缓存插入
    void addToCache(const std::string& key, uint8_t* data, size_t len) {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        // 淘汰最旧的
        while (memCache_.size() >= MAX_CACHE && !memCache_.empty()) {
            auto it = memCache_.begin();
            if (it->second.data) free(it->second.data);
            memCache_.erase(it);
        }
        TileCacheEntry e{ data, len, std::chrono::steady_clock::now(), false };
        memCache_[key] = e;
    }
};

// ============================================================
// CURL 回调
// ============================================================
static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t realSize = size * nmemb;
    auto* ctx = static_cast<DownloadContext*>(userdata);
    if (ctx->cancelled.load()) return 0;
    if (ctx->buffer == nullptr) {
        ctx->buffer = static_cast<uint8_t*>(malloc(realSize > 4096 ? realSize : 4096));
        ctx->capacity = ctx->buffer ? (realSize > 4096 ? realSize : 4096) : 0;
        ctx->size = 0;
    }
    if (ctx->size + realSize > ctx->capacity) {
        size_t newCap = ctx->capacity == 0 ? 4096 : ctx->capacity * 2;
        while (newCap < ctx->size + realSize) newCap *= 2;
        uint8_t* newBuf = static_cast<uint8_t*>(realloc(ctx->buffer, newCap));
        if (!newBuf) return 0;
        ctx->buffer = newBuf;
        ctx->capacity = newCap;
    }
    memcpy(ctx->buffer + ctx->size, ptr, realSize);
    ctx->size += realSize;
    return realSize;
}

// 获取 HTTP 缓存头
static void parseCacheHeaders(CURL* easy, DownloadContext* ctx) {
    char* etag = nullptr;
    curl_easy_getinfo(easy, CURLINFO_HEADER_SIZE, nullptr);
    // CURLINFO_EFFECTIVE_URL 可以拿到 URL
    // 读取 ETag header
    struct curl_header* header = nullptr;
    while (curl_easy_header(easy, "etag", 0, &header, 0) == CURLHE_OK) {
        if (header) ctx->etag = header->value;
    }
}

// 计算退避时间（指数退避）
static int backoffMs(int retry) {
    // 1s, 2s, 4s
    int ms = 1000 * (1 << retry);
    // 加上随机抖动 ±20%
    int jitter = ms / 5;
    ms += (rand() % (2 * jitter)) - jitter;
    return ms;
}

// ============================================================
// TileLoader
// ============================================================
TileLoader::TileLoader() : pImpl_(new Impl()) {
    pImpl_->multi_ = curl_multi_init();
    curl_multi_setopt(pImpl_->multi_, CURLMOPT_MAXCONNECTS, 8L);
}

TileLoader::~TileLoader() = default;

void TileLoader::setProxy(const std::string& host, int port) {
    std::lock_guard<std::mutex> lock(pImpl_->multiMutex_);
    pImpl_->proxyHost_ = host;
    pImpl_->proxyPort_ = port;
    pImpl_->useProxy_ = !host.empty();
}

void TileLoader::setMaxConcurrency(int n) {
    pImpl_->maxConcurrency_ = std::max(1, std::min(n, 8));
}

void TileLoader::cancelAll() {
    std::lock_guard<std::mutex> lock(pImpl_->handlesMutex_);
    for (CURL* h : pImpl_->activeHandles_) {
        curl_multi_remove_handle(pImpl_->multi_, h);
        curl_easy_cleanup(h);
    }
    pImpl_->activeHandles_.clear();
    pImpl_->activeDownloads_ = 0;
}

void TileLoader::loadTileAsync(int x, int y, int z,
                                 const std::string& key,
                                 std::function<void(const uint8_t*, size_t, bool)> callback) {
    std::string cacheKey = pImpl_->makeKey(x, y, z);

    // 1. 查内存缓存
    TileCacheEntry* cached = pImpl_->findInCache(cacheKey);
    if (cached && cached->data && cached->len > 0) {
        // 返回缓存副本
        uint8_t* copy = static_cast<uint8_t*>(malloc(cached->len));
        if (copy) { memcpy(copy, cached->data, cached->len); }
        callback(copy, cached->len, true);
        return;
    }

    // 2. 检查并发限制
    if (pImpl_->activeDownloads_.load() >= pImpl_->maxConcurrency_) {
        // 排队到下一个可用slot
        std::thread([=]() {
            while (pImpl_->activeDownloads_.load() >= pImpl_->maxConcurrency_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            // 实际在子线程执行下载
            this->loadTileAsync(x, y, z, key, callback);
        }).detach();
        return;
    }

    // 3. 执行下载（带重试）
    auto* ctx = new DownloadContext{
        .buffer = nullptr, .size = 0, .capacity = 0,
        .callback = callback, .cancelled = {false},
        .fromCache = false, .x = x, .y = y, .z = z,
        .url = pImpl_->makeUrl(x, y, z, key),
        .retryCount = 0, .etag = "",
        .lastRequest = std::chrono::steady_clock::now()
    };

    std::thread([this, ctx]() {
        const int MAX_RETRIES = 3;
        bool done = false;

        while (!done && !ctx->cancelled.load()) {
            CURL* easy = curl_easy_init();
            if (!easy) { done = true; break; }

            curl_easy_setopt(easy, CURLOPT_URL, ctx->url.c_str());
            curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, writeCallback);
            curl_easy_setopt(easy, CURLOPT_WRITEDATA, ctx);
            curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(easy, CURLOPT_TIMEOUT, 12L);
            curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, 6L);

            // HTTP 头
            curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (OpenHarmony; rv:4.1) MapLibreOH/1.0");
            headers = curl_slist_append(headers, "Accept: image/png,image/jpeg,*/*");
            if (!ctx->etag.empty()) {
                std::string ifNone = std::string("If-None-Match: ") + ctx->etag;
                headers = curl_slist_append(headers, ifNone.c_str());
            }
            curl_easy_setopt(easy, CURLOPT_HTTPHEADER, headers);

            // 代理
            if (pImpl_->useProxy_) {
                char proxyStr[256];
                snprintf(proxyStr, sizeof(proxyStr), "%s:%d",
                         pImpl_->proxyHost_.c_str(), pImpl_->proxyPort_);
                curl_easy_setopt(easy, CURLOPT_PROXY, proxyStr);
            }

            pImpl_->activeDownloads_++;
            {
                std::lock_guard<std::mutex> lock(pImpl_->handlesMutex_);
                pImpl_->activeHandles_.push_back(easy);
            }
            curl_multi_add_handle(pImpl_->multi_, easy);

            // 轮询完成
            int running = 0;
            CURLMsg* msg = nullptr;
            do {
                CURLMcode mc = curl_multi_perform(pImpl_->multi_, &running);
                if (mc == CURLM_OK || mc == CURLM_CALL_MULTI_PERFORM) {
                    curl_multi_wait(pImpl_->multi_, nullptr, 0, 50, nullptr);
                }
                // 检查是否有消息
                int msgs = 0;
                while ((msg = curl_multi_info_read(pImpl_->multi_, &msgs))) {
                    if (msg->msg == CURLMSG_DONE) {
                        long httpCode = 0;
                        curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &httpCode);

                        if (httpCode == 304 && !ctx->etag.empty()) {
                            // Not Modified，使用缓存
                            // 此时认为缓存有效
                            done = true;
                        } else if (msg->data.result == CURLE_OK && httpCode == 200 && ctx->buffer && ctx->size > 0) {
                            // 成功
                            parseCacheHeaders(msg->easy_handle, ctx);
                            std::string cacheKey2 = pImpl_->makeKey(ctx->x, ctx->y, ctx->z);
                            pImpl_->addToCache(cacheKey2, ctx->buffer, ctx->size);
                            ctx->buffer = nullptr; // 防止被 free
                            done = true;
                        } else if (ctx->retryCount < MAX_RETRIES) {
                            // 失败，重试
                            ctx->retryCount++;
                            ctx->buffer = nullptr; ctx->size = 0; ctx->capacity = 0;
                            std::this_thread::sleep_for(std::chrono::milliseconds(backoffMs(ctx->retryCount - 1)));
                            done = false;
                        } else {
                            done = true;
                        }

                        curl_multi_remove_handle(pImpl_->multi_, msg->easy_handle);
                        curl_slist_free_all(headers);
                        curl_easy_cleanup(msg->easy_handle);
                        {
                            std::lock_guard<std::mutex> lock(pImpl_->handlesMutex_);
                            auto it = std::find(pImpl_->activeHandles_.begin(), pImpl_->activeHandles_.end(), msg->easy_handle);
                            if (it != pImpl_->activeHandles_.end()) *it = pImpl_->activeHandles_.back();
                            if (!pImpl_->activeHandles_.empty()) pImpl_->activeHandles_.pop_back();
                        }
                    }
                }
            } while (!done && running > 0 && !ctx->cancelled.load());

            pImpl_->activeDownloads_--;
        }

        // 调用回调
        if (ctx->cancelled.load() || ctx->size == 0) {
            callback(nullptr, 0, ctx->fromCache);
        } else {
            callback(ctx->buffer, ctx->size, false);
        }

        if (ctx->buffer) free(ctx->buffer);
        delete ctx;
    }).detach();
}

uint8_t* TileLoader::loadTileSync(int x, int y, int z,
                                   const std::string& key,
                                   size_t* outLen) {
    uint8_t* result = nullptr;
    size_t resultLen = 0;
    std::mutex m; std::condition_variable cv; bool done = false;
    loadTileAsync(x, y, z, key, [&](const uint8_t* data, size_t len, bool) {
        if (data && len > 0) {
            result = static_cast<uint8_t*>(malloc(len));
            if (result) { memcpy(result, data, len); resultLen = len; }
        }
        { std::lock_guard<std::mutex> lock(m); done = true; }
        cv.notify_one();
    });
    std::unique_lock<std::mutex> lock(m);
    cv.wait(lock, [&done]() { return done; });
    *outLen = resultLen; return result;
}

void TileLoader::freeTileData(uint8_t* data) { free(data); }

} // namespace maplibre
