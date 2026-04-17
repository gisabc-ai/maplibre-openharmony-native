#ifndef COORDINATE_H
#define COORDINATE_H

/**
 * coordinate.h — Web Mercator 坐标转换工具
 * 
 * 实现：
 *   - 经纬度 <-> 瓦片坐标
 *   - 经纬度 <-> OpenGL 标准化设备坐标 (NDC)
 *   - 屏幕像素 <-> OpenGL NDC
 * 
 * 基于 EPSG:3857 (Web Mercator)
 */

namespace maplibre {

/**
 * 经纬度转瓦片坐标（Web Mercator TMS 瓦片系统）
 * 
 * @param lat 纬度 (-85.0511 ~ 85.0511)
 * @param lon 经度 (-180 ~ 180)
 * @param zoom 缩放级别 (0 ~ 19)
 * @param outX 输出：瓦片 X 坐标
 * @param outY 输出：瓦片 Y 坐标
 */
void latLonToTile(double lat, double lon, int zoom, int* outX, int* outY);

/**
 * 瓦片 X 坐标转经度（Web Mercator TMS）
 * 
 * @param x 瓦片 X 坐标
 * @param zoom 缩放级别
 * @return 经度 (-180 ~ 180)
 */
double tileToLon(int x, int zoom);

/**
 * 瓦片 Y 坐标转纬度（Web Mercator TMS）
 * 
 * @param y 瓦片 Y 坐标
 * @param zoom 缩放级别
 * @return 纬度 (-85.0511 ~ 85.0511)
 */
double tileToLat(int y, int zoom);

/**
 * 经纬度转 OpenGL 标准化设备坐标 (NDC)
 * 
 * @param lat 纬度
 * @param lon 经度
 * @param zoom 缩放级别
 * @param screenW 屏幕宽度（像素）
 * @param screenH 屏幕高度（像素）
 * @param outX 输出：NDC X (-1.0 ~ 1.0)
 * @param outY 输出：NDC Y (-1.0 ~ 1.0)
 */
void latLonToGL(double lat, double lon, int zoom,
                 int screenW, int screenH,
                 float* outX, float* outY);

/**
 * 瓦片坐标转 OpenGL NDC（左上角为原点，Y 向下）
 * 
 * @param tileX 瓦片 X 坐标
 * @param tileY 瓦片 Y 坐标
 * @param zoom 缩放级别
 * @param screenW 屏幕宽度
 * @param screenH 屏幕高度
 * @param offsetX 像素级平移偏移
 * @param offsetY 像素级平移偏移
 * @param outX 输出：NDC X
 * @param outY 输出：NDC Y
 * @param outW  输出：NDC 宽度
 * @param outH  输出：NDC 高度
 */
void tileToGL(int tileX, int tileY, int zoom,
              int screenW, int screenH,
              float offsetX, float offsetY,
              float* outX, float* outY,
              float* outW, float* outH);

/**
 * 经纬度到 Web Mercator 像素坐标（以地图左上角为原点）
 * 
 * @param lat 纬度
 * @param lon 经度
 * @param zoom 缩放级别
 * @return 该点的像素坐标（缩放级别对应）
 */
double latLonToMercatorPixel(double lat, double lon, int zoom);

/**
 * Web Mercator 像素坐标到经纬度
 * 
 * @param px 像素 X
 * @param py 像素 Y
 * @param zoom 缩放级别
 * @param outLat 输出纬度
 * @param outLon 输出经度
 */
void mercatorPixelToLatLon(double px, double py, int zoom,
                           double* outLat, double* outLon);

/**
 * 计算指定缩放级别下的一个像素对应多少经度/纬度
 * 
 * @param zoom 缩放级别
 * @param outLonRes 输出：每像素经度分辨率
 * @param outLatRes 输出：每像素纬度分辨率
 */
void getResolution(int zoom, double* outLonRes, double* outLatRes);

} // namespace maplibre

#endif // COORDINATE_H
