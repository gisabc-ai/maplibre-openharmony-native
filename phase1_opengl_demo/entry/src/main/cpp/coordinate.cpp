#include "coordinate.h"
#include <cmath>
#include <cstddef>

namespace maplibre {

static double clampLat(double lat) {
    if (lat >  MERCATOR_MAX_LAT) return  MERCATOR_MAX_LAT;
    if (lat < -MERCATOR_MAX_LAT) return -MERCATOR_MAX_LAT;
    return lat;
}

static double degToRad(double d) { return d * M_PI / 180.0; }
static double radToDeg(double r) { return r * 180.0 / M_PI; }

void latLonToTile(double lat, double lon, int zoom, int* outX, int* outY) {
    lat = clampLat(lat);
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

double lonToMercatorPixelX(double lon, int zoom) {
    double n = static_cast<double>(1 << zoom) * 256.0;
    return (lon + 180.0) / 360.0 * n;
}

double latToMercatorPixelY(double lat, int zoom) {
    lat = clampLat(lat);
    double sinLat = sin(degToRad(lat));
    double y = 0.5 - log((1.0 + sinLat) / (1.0 - sinLat)) / (4.0 * M_PI);
    double n = static_cast<double>(1 << zoom) * 256.0;
    return y * n;
}

void mercatorPixelToLatLon(double px, double py, int zoom,
                           double* outLat, double* outLon) {
    double n = static_cast<double>(1 << zoom) * 256.0;
    double x = px / n;
    double y = py / n;
    *outLon = x * 360.0 - 180.0;
    double y2 = 1.0 - 2.0 * y;
    *outLat = radToDeg(atan(sinh(M_PI * y2)));
}

void screenToLatLon(int screenX, int screenY,
                    double centerLon, double centerLat, int zoom,
                    int screenW, int screenH,
                    float offsetX, float offsetY, float scale,
                    double* outLat, double* outLon) {
    // 中心点 Mercator 像素
    double centerPx = lonToMercatorPixelX(centerLon, zoom);
    double centerPy = latToMercatorPixelY(centerLat, zoom);

    // 屏幕中心
    double halfW = screenW * 0.5;
    double halfH = screenH * 0.5;

    // 当前缩放对应的像素/屏幕比例
    double totalScale = scale;
    double pxPerScreenX = 256.0 * totalScale;
    double pxPerScreenY = 256.0 * totalScale;

    // 屏幕左上角 Mercator 像素
    double topLeftPx = centerPx - halfW * pxPerScreenX / totalScale + offsetX * (1 << zoom) * 128.0;
    double topLeftPy = centerPy - halfH * pxPerScreenY / totalScale - offsetY * (1 << zoom) * 128.0;

    // 目标点像素
    double targetPx = topLeftPx + screenX * pxPerScreenX / totalScale;
    double targetPy = topLeftPy + screenY * pxPerScreenY / totalScale;

    mercatorPixelToLatLon(targetPx, targetPy, zoom, outLat, outLon);
}

void latLonToScreen(double lat, double lon,
                    double centerLon, double centerLat, int zoom,
                    int screenW, int screenH,
                    float offsetX, float offsetY, float scale,
                    int* outX, int* outY) {
    double totalScale = scale;
    double centerPx = lonToMercatorPixelX(centerLon, zoom);
    double centerPy = latToMercatorPixelY(centerLat, zoom);
    double halfW = screenW * 0.5;
    double halfH = screenH * 0.5;

    double pxPerScreenX = 256.0 * totalScale;
    double pxPerScreenY = 256.0 * totalScale;

    double topLeftPx = centerPx - halfW * pxPerScreenX / totalScale + offsetX * (1 << zoom) * 128.0;
    double topLeftPy = centerPy - halfH * pxPerScreenY / totalScale - offsetY * (1 << zoom) * 128.0;

    double targetPx = lonToMercatorPixelX(lon, zoom);
    double targetPy = latToMercatorPixelY(lat, zoom);

    *outX = static_cast<int>((targetPx - topLeftPx) / pxPerScreenX * totalScale);
    *outY = static_cast<int>((targetPy - topLeftPy) / pxPerScreenY * totalScale);
}

double zoomToResolution(int zoom) {
    // 赤道处：每个像素对应多少经度
    double n = static_cast<double>(1 << zoom) * 256.0;
    return 360.0 / n;
}

double zoomToPixelScale(int zoom) {
    // 一个像素对应多少 Mercator 单位（米）
    double n = static_cast<double>(1 << zoom) * 256.0;
    return 2.0 * MERCATOR_RADIUS / n;
}

void tileToGL(int tileX, int tileY, int zoom,
              int screenW, int screenH,
              float offsetX, float offsetY,
              float* outX, float* outY,
              float* outW, float* outH) {
    double n = static_cast<double>(1 << zoom);
    double tileW = 2.0 / n;
    double tileH = 2.0 / n;

    // 中心瓦片的 NDC 位置
    double centerTileX = (tileX + 0.5) * tileW - 1.0;
    double centerTileY = 1.0 - (tileY + 0.5) * tileH;

    *outX = centerTileX - tileW * 0.5f;
    *outY = centerTileY - tileH * 0.5f;
    *outW = static_cast<float>(tileW);
    *outH = static_cast<float>(tileH);
}

void ndcToScreen(float ndcX, float ndcY, int screenW, int screenH,
                 int* outX, int* outY) {
    *outX = static_cast<int>((ndcX + 1.0f) * 0.5f * screenW);
    *outY = static_cast<int>((1.0f - ndcY) * 0.5f * screenH);
}

double haversineDistance(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000.0;
    double phi1 = degToRad(lat1), phi2 = degToRad(lat2);
    double dphi = degToRad(lat2 - lat1);
    double dlam = degToRad(lon2 - lon1);
    double a = sin(dphi * 0.5) * sin(dphi * 0.5) +
              cos(phi1) * cos(phi2) * sin(dlam * 0.5) * sin(dlam * 0.5);
    return R * 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
}

} // namespace maplibre
