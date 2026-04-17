#ifndef MAP_RENDERER_H
#define MAP_RENDERER_H

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <string>
#include <map>
#include <functional>

namespace maplibre {

/**
 * MapRenderer — OpenGL ES 2.0 地图渲染器
 * 
 * 负责：
 * 1. EGL 初始化（连接 Native Window）
 * 2. OpenGL ES 上下文管理
 * 3. 瓦片纹理加载与缓存
 * 4. 帧渲染
 * 
 * 使用 OpenHarmony NDK NativeWindow API (API 4.1+)
 */
class MapRenderer {
public:
    MapRenderer();
    ~MapRenderer();

    /**
     * 初始化渲染器
     * @param nativeWindow OHNativeWindow 指针，来自 ArkTS 侧传入
     * @return true 成功，false 失败
     */
    bool initialize(void* nativeWindow);

    /**
     * 渲染一帧
     * 调用时机：每當需要重绘时（手动 or VSync 触发）
     */
    void renderFrame();

    /**
     * 加载瓦片图片为 OpenGL 纹理
     * @param x 瓦片 X 坐标 (Web Mercator tile coordinate)
     * @param y 瓦片 Y 坐标
     * @param z 缩放级别 (zoom level)
     * @param data PNG/JPEG 原始字节数据
     * @param len 数据长度
     */
    void loadTile(int x, int y, int z, const uint8_t* data, size_t len);

    /**
     * 设置地图中心点（经纬度 + 缩放级别）
     * @param lon 经度
     * @param lat 纬度
     * @param zoom 缩放级别
     */
    void setCenter(double lon, double lat, int zoom);

    /**
     * 设置屏幕分辨率（像素）
     * @param width 屏幕宽度
     * @param height 屏幕高度
     */
    void setScreenSize(int width, int height);

    /**
     * 平移地图（像素增量）
     * @param dx X 方向增量
     * @param dy Y 方向增量
     */
    void pan(int dx, int dy);

    /**
     * 缩放等级调整
     * @param delta 缩放增量（正数放大，负数缩小）
     * @param mouseX 鼠标 X（用于确定缩放中心）
     * @param mouseY 鼠标 Y
     */
    void zoom(int delta, int mouseX, int mouseY);

    /**
     * 清理资源（析构或销毁时调用）
     */
    void cleanup();

    /**
     * 是否已初始化
     */
    bool isInitialized() const { return initialized_; }

private:
    // 内部辅助
    bool initEGL(void* nativeWindow);
    bool initGLES();
    bool compileShader(GLenum type, const char* src, GLuint* outShader);
    bool linkProgram(GLuint vertShader, GLuint fragShader, GLuint* outProgram);
    GLuint loadTileTexture(int x, int y, int z, const uint8_t* data, size_t len);
    void renderTile(int x, int y, int z);
    void updateMVP();
    std::string tileKey(int x, int y, int z) const;

    // EGL 资源
    EGLDisplay display_;
    EGLContext context_;
    EGLSurface surface_;
    EGLConfig  config_;

    // GLES 资源
    GLuint shaderProgram_;
    GLuint vbo_;              // 顶点缓冲
    GLuint ibo_;              // 索引缓冲

    // Shader 句柄
    GLint locPosition_;
    GLint locTexCoord_;
    GLint locMVPMatrix_;
    GLint locTexture_;

    // 瓦片纹理缓存 key: "z/x/y"
    std::map<std::string, GLuint> tileTextures_;

    // 地图状态
    double centerLon_;
    double centerLat_;
    int    zoom_;
    int    screenWidth_;
    int    screenHeight_;
    float  offsetX_;   // 像素偏移（平移用）
    float  offsetY_;

    // MVP 矩阵（列主序，16 元素 float 数组）
    float  mvpMatrix_[16];

    bool   initialized_;
};

} // namespace maplibre

#endif // MAP_RENDERER_H
