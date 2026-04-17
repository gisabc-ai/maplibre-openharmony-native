#include "tile_loader.h"
#include <curl/curl.h>
#include <cstring>
#include <cstdlib>
#include <atomic>
#include <mutex>
#include <vector>

namespace maplibre {

struct TileLoader::Impl {
    CURLM* multi_;
    std::vector<CURL*> pending_;
    std::mutex mutex_;
    std::string proxyHost_;
    int proxyPort_ = 0;
    bool useProxy_ = false;

    Impl() {
        multi_ = curl_multi_init();
        // 允许同时处理多个请求
        curl_multi_setopt(multi_, CURLMOPT_MAXCONNECTS, 8L);
    }

    ~Impl() {
        cancelAll();
        curl_multi_cleanup(multi_);
    }
};

// --- 下载回调 ---
struct DownloadContext {
    uint8_t* buffer;
    size_t size;
    size_t capacity;
    std::function<void(const uint8_t*, size_t)> callback;
    std::atomic<bool> cancelled{false};
};

static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t realSize = size * nmemb;
    auto* ctx = static_cast<DownloadContext*>(userdata);

    if (ctx->cancelled.load()) return 0;

    // 扩容
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

// --- 瓦片 URL 生成 ---
static std::string makeTileUrl(int x, int y, int z, const std::string& key) {
    // 天地图 vec_w 图层（街道图）
    // 也可用 img_w（卫星图）、ter_w（地形图）
    // y 参数：天地图使用正常 TMS 顺序（y 从下往上递增）
    // 但有些版本需要翻转：OpenStreetMap 用的是 y' = (1 << z) - 1 - y
    // 天地图 WMTS 使用标准 TMS y 坐标（不需要翻转）
    char url[512];
    snprintf(url, sizeof(url),
             "https://t0.tianditu.gov.cn/DataServer?T=vec_w&x=%d&y=%d&l=%d&tk=%s",
             x, y, z, key.c_str());
    return std::string(url);
}

TileLoader::TileLoader()
    : pImpl_(new Impl()) {}

TileLoader::~TileLoader() = default;

void TileLoader::loadTileAsync(int x, int y, int z,
                                const std::string& key,
                                std::function<void(const uint8_t*, size_t)> callback) {
    auto* ctx = new DownloadContext{
        .buffer   = nullptr,
        .size     = 0,
        .capacity = 0,
        .callback = callback,
        .cancelled = {false}
    };

    CURL* easy = curl_easy_init();
    if (!easy) {
        delete ctx;
        callback(nullptr, 0);
        return;
    }

    std::string url = makeTileUrl(x, y, z, key);

    curl_easy_setopt(easy, CURLOPT_URL, url.c_str());
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, ctx);
    curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(easy, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT, 5L);

    // HTTP header 模拟浏览器
    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (OpenHarmony; rv:4.1) MapLibreOH/1.0");
    headers = curl_slist_append(headers, "Accept: image/png,image/jpeg,*/*");
    curl_easy_setopt(easy, CURLOPT_HTTPHEADER, headers);

    // 代理
    if (pImpl_->useProxy_) {
        char proxyStr[256];
        snprintf(proxyStr, sizeof(proxyStr), "%s:%d",
                 pImpl_->proxyHost_.c_str(), pImpl_->proxyPort_);
        curl_easy_setopt(easy, CURLOPT_PROXY, proxyStr);
    }

    curl_multi_add_handle(pImpl_->multi_, easy);

    // 启动后台线程处理这个请求
    // 注意：这里简化处理，实际上建议用 poll/select 驱动 curl_multi
    // 在 OpenHarmony 中应使用 napi_create_async_work
    std::thread([this, easy, ctx, headers]() {
        int running = 0;
        do {
            CURLMcode mc = curl_multi_perform(pImpl_->multi_, &running);
            if (mc == CURLM_OK || mc == CURLM_CALL_MULTI_PERFORM) {
                curl_multi_wait(pImpl_->multi_, nullptr, 0, 100, nullptr);
            } else {
                break;
            }
        } while (running > 0 && !ctx->cancelled.load());

        long httpCode = 0;
        curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &httpCode);

        if (ctx->cancelled.load() || httpCode != 200) {
            if (ctx->buffer) free(ctx->buffer);
            ctx->buffer = nullptr;
            ctx->size = 0;
        }

        // 回调
        ctx->callback(ctx->buffer, ctx->size);

        // 清理
        if (ctx->buffer) free(ctx->buffer);
        delete ctx;
        curl_slist_free_all(headers);
        curl_multi_remove_handle(pImpl_->multi_, easy);
        curl_easy_cleanup(easy);
    }).detach();
}

uint8_t* TileLoader::loadTileSync(int x, int y, int z,
                                  const std::string& key,
                                  size_t* outLen) {
    uint8_t* result = nullptr;
    size_t resultLen = 0;

    std::mutex m;
    std::condition_variable cv;
    bool done = false;

    loadTileAsync(x, y, z, key,
        [&](const uint8_t* data, size_t len) {
            if (data && len > 0) {
                result = static_cast<uint8_t*>(malloc(len));
                if (result) {
                    memcpy(result, data, len);
                    resultLen = len;
                }
            }
            {
                std::lock_guard<std::mutex> lock(m);
                done = true;
            }
            cv.notify_one();
        });

    std::unique_lock<std::mutex> lock(m);
    cv.wait(lock, [&done]() { return done; });

    *outLen = resultLen;
    return result;
}

void TileLoader::freeTileData(uint8_t* data) {
    free(data);
}

void TileLoader::cancelAll() {
    std::lock_guard<std::mutex> lock(pImpl_->mutex_);
    CURLM* m = pImpl_->multi_;
    // 所有正在进行的请求会通过 curl_multi_remove_handle 清理
    curl_multi_wakeup(m);
}

void TileLoader::setProxy(const std::string& host, int port) {
    std::lock_guard<std::mutex> lock(pImpl_->mutex_);
    pImpl_->proxyHost_ = host;
    pImpl_->proxyPort_ = port;
    pImpl_->useProxy_ = !host.empty();
}

} // namespace maplibre
