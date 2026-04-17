#include "coordinate.h"
#include <cmath>
#include <cstddef>

namespace maplibre {

// Web Mercator 球体半径 (米)
static constexpr double MERCATOR_RADIUS = 20037508.342789244;

// 限制纬度范围（Web Mercator 有效范围）
static double clampLat(double lat) {
    if (lat >  85.0511) return  85.0511;
    if (lat < -85.0511) return -85.0511;
    return lat;
}

// 角度转弧度
static double degToRad(double deg) {
    return deg * M_PI / 180.0;
}

// 弧度转角度
static double radToDeg(double rad) {
    return rad * 180.0 / M_PI;
}

void latLonToTile(double lat, double lon, int zoom, int* outX, int* outY) {
    lat = clampLat(lat);

    // Web Mercator 投影
    double x = (lon + 180.0) / 360.0;
    double sinLat = sin(degToRad(lat));
    double y = 0.5 - log((1.0 + sinLat) / (1.0 - sinLat)) / (4.0 * M_PI);

    double n = static_cast<double>(1 << zoom);
    *outX = static_cast<int>(floor(x * n));
    *outY = static_cast<int>(floor(y * n));
}

double tileToLon(int x, int zoom) {
    double n = static_cast<double>(1 << zoom);
    return static_cast<double>(x) / n * 360.0 - 180.0;
}

double tileToLat(int y, int zoom) {
    double n = static_cast<double>(1 << zoom);
    double y2 = static_cast<double>(y) / n;
    double latRad = atan(sinh(M_PI * (1.0 - 2.0 * y2)));
    return radToDeg(latRad);
}

double latLonToMercatorPixel(double lat, double lon, int zoom) {
    lat = clampLat(lat);

    // 经度 -> X
    double x = (lon + 180.0) / 360.0;

    // 纬度 -> Y (Web Mercator)
    double sinLat = sin(degToRad(lat));
    double y = 0.5 - log((1.0 + sinLat) / (1.0 - sinLat)) / (4.0 * M_PI);

    // 像素数
    double n = static_cast<double>(1 << zoom) * 256.0;
    // Y 是从上往下算（瓦片系统），但 Mercator Y 从下往上
    // 这里返回的 px/py 是以左上角为原点
    double px = x * n;
    double py = y * n;

    // 返回平方和（仅用于计算，不单独使用）
    return px + py * n; // 不这么用
}

void latLonToGL(double lat, double lon, int zoom,
                int screenW, int screenH,
                float* outX, float* outY) {
    lat = clampLat(lat);

    // 先转瓦片坐标
    int tileX, tileY;
    latLonToTile(lat, lon, zoom, &tileX, &tileY);

    // 瓦片像素起始位置
    double n = static_cast<double>(1 << zoom);
    double px = (static_cast<double>(tileX) + 0.5) * 256.0 * 360.0 / n / 360.0;

    // 转 NDC
    // NDC: 屏幕中心为原点，右上为正
    // 像素: 左上为原点，右下为正
    double centerPx = (lon + 180.0) / 360.0 * n * 256.0;
    double sinLat = sin(degToRad(lat));
    double centerPy = (0.5 - log((1.0 + sinLat) / (1.0 - sinLat)) / (4.0 * M_PI)) * n * 256.0;

    // 屏幕像素坐标
    double screenX = centerPx - screenW * 0.5;
    double screenY = centerPy - screenH * 0.5;

    // 转 NDC
    *outX = static_cast<float>(screenX / (screenW * 0.5));
    *outY = static_cast<float>(-screenY / (screenH * 0.5)); // Y翻转
}

void tileToGL(int tileX, int tileY, int zoom,
              int screenW, int screenH,
              float offsetX, float offsetY,
              float* outX, float* outY,
              float* outW, float* outH) {
    // 一个瓦片在 NDC 空间的尺寸
    // 256 像素对应屏幕尺寸
    double n = static_cast<double>(1 << zoom);
    double tileSizePx = 256.0 * 2.0 / n; // 整个地球 = 2.0 NDC 单位

    // 一个瓦片的 NDC 大小
    double tileW = 256.0 / (screenW * 0.5);
    double tileH = 256.0 / (screenH * 0.5);

    // 中心瓦片的 NDC 位置
    double centerTileX = (tileX - n * 0.5) * tileW + 1.0;
    double centerTileY = -(tileY - n * 0.5) * tileH + 1.0;

    // 从瓦片左上角开始（瓦片纹理坐标 v=0 在顶部）
    // 注意 GL Y 向下，NDC Y 向上，所以 tileY 大 -> NDC Y 小
    *outX = centerTileX - 1.0; // 左上角
    *outY = centerTileY - 1.0; // 左上角

    *outW = static_cast<float>(tileW);
    *outH = static_cast<float>(tileH);
}

void mercatorPixelToLatLon(double px, double py, int zoom,
                           double* outLat, double* outLon) {
    double n = static_cast<double>(1 << zoom) * 256.0;
    double x = px / n;
    double y = py / n;

    *outLon = x * 360.0 - 180.0;

    double y2 = 1.0 - 2.0 * y;
    double latRad = atan(sinh(M_PI * y2));
    *outLat = radToDeg(latRad);
}

void getResolution(int zoom, double* outLonRes, double* outLatRes) {
    double n = static_cast<double>(1 << zoom) * 256.0;
    *outLonRes = 360.0 / n;
    // 纬度分辨率随纬度变化，此处返回赤道处值
    *outLatRes = 360.0 / n;
}

} // namespace maplibre
