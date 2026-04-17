#ifndef TILE_LOADER_H
#define TILE_LOADER_H

#include <string>
#include <functional>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <vector>
#include <chrono>

namespace maplibre {

/**
 * TileLoader — 高可靠瓦片下载器
 * 
 * 特性：
 * - HTTP 缓存头解析（Cache-Control, ETags, Last-Modified）
 * - 3 次指数退避重试
 * - 最多 4 个并发下载
 * - 内存缓存（LRU，最大 32 条）
 * - 支持代理
 */
class TileLoader {
public:
    TileLoader();
    ~TileLoader();

    /**
     * 异步加载瓦片
     * @param x, y, z   瓦片坐标
     * @param key       天地图 API Key
     * @param callback  回调 (data, len, cacheHit)
     */
    void loadTileAsync(int x, int y, int z,
                       const std::string& key,
                       std::function<void(const uint8_t* data, size_t len, bool fromCache)> callback);

    /**
     * 同步加载（阻塞）
     */
    uint8_t* loadTileSync(int x, int y, int z, const std::string& key, size_t* outLen);

    /**
     * 释放数据
     */
    void freeTileData(uint8_t* data);

    /**
     * 取消所有
     */
    void cancelAll();

    /**
     * 设置代理
     */
    void setProxy(const std::string& host, int port);

    /**
     * 获取/设置并发数
     */
    void setMaxConcurrency(int n);
    int getMaxConcurrency() const { return maxConcurrency_; }

private:
    struct Impl;
    Impl* pImpl_;
};

} // namespace maplibre

#endif // TILE_LOADER_H
