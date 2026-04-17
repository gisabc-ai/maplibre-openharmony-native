#ifndef TILE_LOADER_H
#define TILE_LOADER_H

#include <string>
#include <functional>
#include <cstdint>

namespace maplibre {

/**
 * TileLoader — 天地图瓦片异步加载器
 * 
 * 使用 libcurl 异步下载 WMTS 瓦片图片，
 * 支持 PNG/JPEG 格式，下载完成后通过回调返回原始字节。
 * 
 * 使用天地图 WMTS 服务，用户需自行申请 Key：
 *   https://console.tianditu.gov.cn/
 * 
 * 瓦片 URL 模板：
 *   https://t0.tianditu.gov.cn/DataServer?T=vec_w&x={x}&y={y}&l={z}&tk={key}
 */
class TileLoader {
public:
    TileLoader();
    ~TileLoader();

    /**
     * 异步加载瓦片
     * 
     * @param x     瓦片 X 坐标
     * @param y     瓦片 Y 坐标  
     * @param z     缩放级别
     * @param key   天地图 API Key（可共用，也可每个瓦片不同）
     * @param callback 下载完成回调
     *                  - data: PNG/JPEG 原始字节
     *                  - len:  数据长度
     *                  - 若下载失败，data 为 nullptr，len 为 0
     * 
     * 注意：回调在 curl 工作线程中执行，
     *       若要操作 GL 资源，需 post 到主线程。
     */
    void loadTileAsync(int x, int y, int z,
                       const std::string& key,
                       std::function<void(const uint8_t* data, size_t len)> callback);

    /**
     * 同步加载瓦片（阻塞）
     * 
     * @param x   瓦片 X 坐标
     * @param y   瓦片 Y 坐标
     * @param z   缩放级别
     * @param key 天地图 API Key
     * @return  瓦片数据指针（需调用 freeTileData 释放），nullptr 表示失败
     */
    uint8_t* loadTileSync(int x, int y, int z, const std::string& key, size_t* outLen);

    /**
     * 释放瓦片数据（由 loadTileSync 分配）
     */
    void freeTileData(uint8_t* data);

    /**
     * 取消所有待处理的请求
     */
    void cancelAll();

    /**
     * 设置代理（可选）
     */
    void setProxy(const std::string& host, int port);

private:
    // 内部实现
    struct Impl;
    Impl* pImpl_;
};

} // namespace maplibre

#endif // TILE_LOADER_H
