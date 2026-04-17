#ifndef COORDINATE_H
#define COORDINATE_H

/**
 * coordinate.h — Web Mercator 坐标转换工具 (增强版)
 * 
 * 实现：
 *   - 经纬度 <-> 瓦片坐标 (TMS)
 *   - 经纬度 <-> Web Mercator 像素坐标
 *   - 经纬度 <-> 屏幕像素坐标（考虑缩放、平移）
 *   - 屏幕像素坐标 -> 经纬度（反向投影）
 *   - 缩放级别与像素精度的关系
 *   - 瓦片 NDC 位置计算
 */

namespace maplibre {

// Web Mercator 球体半径 (米)
static constexpr double MERCATOR_RADIUS = 20037508.342789244;
static constexpr double MERCATOR_MAX_LAT = 85.051128779806604;

/**
 * 经纬度 -> 瓦片坐标（Web Mercator TMS）
 */
void latLonToTile(double lat, double lon, int zoom, int* outX, int* outY);

/**
 * 瓦片坐标 -> 经纬度
 */
double tileToLon(int x, int zoom);
double tileToLat(int y, int zoom);

/**
 * 经纬度 -> Web Mercator 像素坐标（以地图左上角为原点，zoom 级别）
 * @return 像素 X
 */
double lonToMercatorPixelX(double lon, int zoom);
double latToMercatorPixelY(double lat, int zoom);

/**
 * Web Mercator 像素坐标 -> 经纬度
 */
void mercatorPixelToLatLon(double px, double py, int zoom,
                           double* outLat, double* outLon);

/**
 * 屏幕像素坐标 -> 经纬度（考虑当前地图状态）
 * @param screenX, screenY  屏幕像素坐标（以左上角为原点）
 * @param centerLon, centerLat, zoom  当前地图中心和缩放级别
 * @param screenW, screenH          屏幕尺寸
 * @param offsetX, offsetY          当前平移偏移（NDC 单位）
 * @param scale                     当前缩放因子
 * @param outLat, outLon            输出经纬度
 */
void screenToLatLon(int screenX, int screenY,
                   double centerLon, double centerLat, int zoom,
                   int screenW, int screenH,
                   float offsetX, float offsetY, float scale,
                   double* outLat, double* outLon);

/**
 * 经纬度 -> 屏幕像素坐标
 */
void latLonToScreen(double lat, double lon,
                    double centerLon, double centerLat, int zoom,
                    int screenW, int screenH,
                    float offsetX, float offsetY, float scale,
                    int* outX, int* outY);

/**
 * 缩放级别 -> 每像素经度/纬度分辨率（赤道处）
 */
double zoomToResolution(int zoom);

/**
 * 缩放级别 -> 像素比例（一个像素对应多少 Mercator 单位）
 */
double zoomToPixelScale(int zoom);

/**
 * 瓦片坐标 -> OpenGL NDC（左上角为原点，Y 向上）
 */
void tileToGL(int tileX, int tileY, int zoom,
              int screenW, int screenH,
              float offsetX, float offsetY,
              float* outX, float* outY,
              float* outW, float* outH);

/**
 * NDC 坐标 -> 屏幕像素
 */
void ndcToScreen(float ndcX, float ndcY, int screenW, int screenH,
                 int* outX, int* outY);

/**
 * Haversine 距离（米）
 */
double haversineDistance(double lat1, double lon1, double lat2, double lon2);

} // namespace maplibre

#endif // COORDINATE_H
