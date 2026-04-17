#ifndef MAP_RENDERER_H
#define MAP_RENDERER_H

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <string>
#include <map>
#include <functional>
#include <list>
#include <mutex>

namespace maplibre {

class GestureHandler;

/**
 * LRU 纹理缓存
 * 线程安全，限制最大条目数，超出时淘汰最久未使用的纹理
 */
class LRUTextureCache {
public:
    LRUTextureCache(size_t maxEntries = 64);
    ~LRUTextureCache();

    // 查找（命中会提升优先级）
    GLuint find(const std::string& key);

    // 插入（如果已存在则更新）
    void insert(const std::string& key, GLuint tex);

    // 删除
    void remove(const std::string& key);

    // 清空所有
    void clear();

    // 获取当前条目数
    size_t size() const;

private:
    struct Entry {
        GLuint tex;
        // 使用双向链表节点
    };
    size_t maxEntries_;
    std::map<std::string, std::list<std::pair<std::string, GLuint>>::iterator> cache_;
    std::list<std::pair<std::string, GLuint>> lruList_; // front=最新, back=最旧
    mutable std::mutex mutex_;
};

/**
 * MapRenderer — OpenGL ES 2.0 地图渲染器
 */
class MapRenderer {
public:
    MapRenderer();
    ~MapRenderer();

    bool initialize(void* nativeWindow);
    void renderFrame();
    void loadTile(int x, int y, int z, const uint8_t* data, size_t len);
    void setCenter(double lon, double lat, int zoom);
    void setScreenSize(int width, int height);
    void pan(int dx, int dy);
    void zoom(int delta, int mouseX, int mouseY);
    void zoomBy(float scaleFactor, int centerX, int centerY);
    void cleanup();
    bool isInitialized() const { return initialized_; }

    // 绑定手势处理器（外部拥有）
    void setGestureHandler(GestureHandler* handler) { gestureHandler_ = handler; }

private:
    bool initEGL(void* nativeWindow);
    bool initGLES();
    bool compileShader(GLenum type, const char* src, GLuint* outShader);
    bool linkProgram(GLuint vertShader, GLuint fragShader, GLuint* outProgram);
    GLuint loadTileTexture(int x, int y, int z, const uint8_t* data, size_t len);
    void renderTile(int x, int y, int z);
    void updateMVP();
    void updateMVPWithGesture();
    std::string tileKey(int x, int y, int z) const;

    // 矩阵计算
    void multiplyMatrix(float* out, const float* a, const float* b);
    void perspectiveMatrix(float* m, float fovY, float aspect, float near, float far);
    void lookAtMatrix(float* m, float eyeX, float eyeY, float eyeZ, float centerX, float centerY, float centerZ, float upX, float upY, float upZ);

    // 瓦片网格渲染
    void renderTileGrid();

    // EGL
    EGLDisplay display_;
    EGLContext context_;
    EGLSurface surface_;
    EGLConfig  config_;

    // GLES
    GLuint shaderProgram_;
    GLuint vbo_;
    GLuint ibo_;
    GLuint gridVbo_;
    GLuint gridIbo_;

    GLint locPosition_;
    GLint locTexCoord_;
    GLint locMVPMatrix_;
    GLint locTexture_;

    // LRU 纹理缓存
    LRUTextureCache textureCache_;

    // 地图状态
    double centerLon_;
    double centerLat_;
    int    zoom_;
    int    screenWidth_;
    int    screenHeight_;
    float  offsetX_;
    float  offsetY_;
    float  scale_;
    float  rotation_;    // 绕 Z 轴旋转（弧度）
    float  tilt_;        // 倾斜角度（弧度）

    // 手势相关
    float gestureZoomScale_;  // 手势引起的缩放（乘法因子）
    GestureHandler* gestureHandler_;

    // MVP 矩阵（列主序）
    float  modelMatrix_[16];
    float  viewMatrix_[16];
    float  projMatrix_[16];
    float  mvpMatrix_[16];

    bool   initialized_;
};

} // namespace maplibre

#endif // MAP_RENDERER_H
