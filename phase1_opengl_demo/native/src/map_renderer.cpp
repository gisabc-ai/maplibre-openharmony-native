#include "map_renderer.h"
#include "coordinate.h"
#include <android/log.h>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <string>

#define LOG_TAG "MapRenderer"
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace maplibre {

// --- 顶点着色器源码 ---
static const char VERT_SHADER[] = R"(
attribute vec2 a_position;
attribute vec2 a_texCoord;
varying vec2 v_texCoord;
uniform mat4 u_mvpMatrix;

void main() {
    gl_Position = u_mvpMatrix * vec4(a_position, 0.0, 1.0);
    v_texCoord = a_texCoord;
}
)";

// --- 片元着色器源码 ---
static const char FRAG_SHADER[] = R"(
precision mediump float;
varying vec2 v_texCoord;
uniform sampler2D u_texture;

void main() {
    gl_FragColor = texture2D(u_texture, v_texCoord);
}
)";

// --- 单位矩阵 ---
static void identityMatrix(float* m) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

// --- 正交投影矩阵 ---
static void orthoMatrix(float* m, float l, float r, float b, float t, float n, float f) {
    memset(m, 0, 16 * sizeof(float));
    m[0]  =  2.0f / (r - l);
    m[5]  =  2.0f / (t - b);
    m[10] = -2.0f / (f - n);
    m[12] = -(r + l) / (r - l);
    m[13] = -(t + b) / (t - b);
    m[14] = -(f + n) / (f - n);
    m[15] =  1.0f;
}

// ============================================================
// MapRenderer
// ============================================================

MapRenderer::MapRenderer()
    : display_(EGL_NO_DISPLAY)
    , context_(EGL_NO_CONTEXT)
    , surface_(EGL_NO_SURFACE)
    , config_(nullptr)
    , shaderProgram_(0)
    , vbo_(0)
    , ibo_(0)
    , locPosition_(-1)
    , locTexCoord_(-1)
    , locMVPMatrix_(-1)
    , locTexture_(-1)
    , centerLon_(116.4)
    , centerLat_(39.9)
    , zoom_(10)
    , screenWidth_(1920)
    , screenHeight_(1080)
    , offsetX_(0.0f)
    , offsetY_(0.0f)
    , initialized_(false)
{
    identityMatrix(mvpMatrix_);
}

MapRenderer::~MapRenderer() {
    cleanup();
}

bool MapRenderer::initialize(void* nativeWindow) {
    if (initialized_) {
        LOGI("Already initialized");
        return true;
    }

    if (!initEGL(nativeWindow)) {
        LOGE("EGL init failed");
        return false;
    }

    if (!initGLES()) {
        LOGE("GLES init failed");
        cleanup();
        return false;
    }

    initialized_ = true;
    LOGI("MapRenderer initialized successfully");
    return true;
}

bool MapRenderer::initEGL(void* nativeWindow) {
    // 1. 获取 EGL 显示
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay failed");
        return false;
    }

    // 2. 初始化 EGL
    EGLint major, minor;
    if (!eglInitialize(display_, &major, &minor)) {
        LOGE("eglInitialize failed");
        return false;
    }
    LOGI("EGL version: %d.%d", major, minor);

    // 3. 配置选择
    EGLint configAttribs[] = {
        EGL_SURFACE_TYPE,     EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE,  EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,         8,
        EGL_GREEN_SIZE,       8,
        EGL_BLUE_SIZE,        8,
        EGL_ALPHA_SIZE,       8,
        EGL_DEPTH_SIZE,       16,
        EGL_NONE
    };

    EGLint numConfigs = 0;
    if (!eglChooseConfig(display_, configAttribs, &config_, 1, &numConfigs) || numConfigs == 0) {
        LOGE("eglChooseConfig failed");
        return false;
    }

    // 4. 创建 EGL 上下文
    EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    context_ = eglCreateContext(display_, config_, EGL_NO_CONTEXT, contextAttribs);
    if (context_ == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed");
        return false;
    }

    // 5. 创建 EGLSurface（绑定 NativeWindow）
    surface_ = eglCreateWindowSurface(display_, config_,
                                      static_cast<EGLNativeWindowType>(nativeWindow), nullptr);
    if (surface_ == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface failed: 0x%x", eglGetError());
        return false;
    }

    // 6. 激活上下文
    if (!eglMakeCurrent(display_, surface_, surface_, context_)) {
        LOGE("eglMakeCurrent failed");
        return false;
    }

    LOGI("EGL initialized");
    return true;
}

bool MapRenderer::initGLES() {
    // 编译顶点着色器
    GLuint vertShader = 0;
    if (!compileShader(GL_VERTEX_SHADER, VERT_SHADER, &vertShader)) {
        LOGE("Vertex shader compile failed");
        return false;
    }

    // 编译片元着色器
    GLuint fragShader = 0;
    if (!compileShader(GL_FRAGMENT_SHADER, FRAG_SHADER, &fragShader)) {
        LOGE("Fragment shader compile failed");
        glDeleteShader(vertShader);
        return false;
    }

    // 链接程序
    if (!linkProgram(vertShader, fragShader, &shaderProgram_)) {
        LOGE("Program link failed");
        glDeleteShader(vertShader);
        glDeleteShader(fragShader);
        return false;
    }

    // 着色器已链接入程序，可删除中间对象
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    // 获取 uniform/attribute 位置
    locPosition_    = glGetAttribLocation(shaderProgram_, "a_position");
    locTexCoord_   = glGetAttribLocation(shaderProgram_, "a_texCoord");
    locMVPMatrix_  = glGetUniformLocation(shaderProgram_, "u_mvpMatrix");
    locTexture_    = glGetUniformLocation(shaderProgram_, "u_texture");

    // 创建全屏四边形顶点缓冲
    // 顶点格式: x, y, u, v (每顶点 4 个 float)
    const float quadVertices[] = {
        // x     y     u     v
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 0.0f,
    };
    const uint16_t quadIndices[] = { 0, 1, 2, 0, 2, 3 };

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glGenBuffers(1, &ibo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices), quadIndices, GL_STATIC_DRAW);

    glEnable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    LOGI("GLES initialized, program=%d", shaderProgram_);
    return true;
}

bool MapRenderer::compileShader(GLenum type, const char* src, GLuint* outShader) {
    GLuint shader = glCreateShader(type);
    if (!shader) return false;

    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        LOGE("Shader compile error: %s", log);
        glDeleteShader(shader);
        return false;
    }

    *outShader = shader;
    return true;
}

bool MapRenderer::linkProgram(GLuint vertShader, GLuint fragShader, GLuint* outProgram) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vertShader);
    glAttachShader(prog, fragShader);
    glLinkProgram(prog);

    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        LOGE("Program link error: %s", log);
        glDeleteProgram(prog);
        return false;
    }

    *outProgram = prog;
    return true;
}

void MapRenderer::setCenter(double lon, double lat, int zoom) {
    centerLon_ = lon;
    centerLat_ = lat;
    zoom_ = zoom;
    offsetX_ = 0.0f;
    offsetY_ = 0.0f;
    updateMVP();
}

void MapRenderer::setScreenSize(int width, int height) {
    screenWidth_ = width;
    screenHeight_ = height;
    updateMVP();
}

void MapRenderer::pan(int dx, int dy) {
    float scale = 2.0f / (screenWidth_ * (1 << zoom_));
    offsetX_ += dx * scale;
    offsetY_ -= dy * scale; // Y 翻转
    updateMVP();
}

void MapRenderer::zoom(int delta, int mouseX, int mouseY) {
    int oldZoom = zoom_;
    zoom_ = std::max(1, std::min(19, zoom_ + delta));
    if (zoom_ != oldZoom) {
        // 缩放中心对齐
        // 实现略：需将鼠标像素转为经纬度，再重新计算 offset
    }
    updateMVP();
}

void MapRenderer::updateMVP() {
    // 正交投影，范围 [-1, 1]
    // 结合 offset 实现平移
    float m[16];
    orthoMatrix(m, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);

    // 应用平移
    m[12] += offsetX_;
    m[13] += offsetY_;

    memcpy(mvpMatrix_, m, sizeof(m));
}

std::string MapRenderer::tileKey(int x, int y, int z) const {
    char buf[64];
    snprintf(buf, sizeof(buf), "%d/%d/%d", z, x, y);
    return std::string(buf);
}

GLuint MapRenderer::loadTileTexture(int x, int y, int z,
                                     const uint8_t* data, size_t len) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // 设置纹理参数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 加载 PNG/JPEG 数据到 GPU
    // 实际应使用 stb_image 或 libpng 解码
    // 这里简化：假设 data 已经是解码后的 RGBA 数据
    // 真正的实现需要图片解码库
    // 使用 stb_image.h 加载（内嵌，轻量）
    int w = 256, h = 256, channels = 4;
    // stbi_set_flip_vertically_on_load(1);
    // uint8_t* pixels = stbi_load_from_memory(data, len, &w, &h, &channels, 4);
    // if (!pixels) { glDeleteTextures(1, &tex); return 0; }
    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    // stbi_image_free(pixels);

    // Placeholder: 纯色纹理演示
    uint8_t placeholder[256 * 256 * 4];
    for (int i = 0; i < 256 * 256; i++) {
        placeholder[i*4+0] = (x * 37 + y * 17 + z * 31) % 256; // R
        placeholder[i*4+1] = (x * 13 + y * 41 + z * 19) % 256; // G
        placeholder[i*4+2] = 200;                                // B
        placeholder[i*4+3] = 255;                                // A
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, placeholder);

    glGenerateMipmap(GL_TEXTURE_2D);
    return tex;
}

void MapRenderer::loadTile(int x, int y, int z,
                            const uint8_t* data, size_t len) {
    if (!initialized_ || !data || len == 0) return;

    std::string key = tileKey(x, y, z);

    // 如果已有纹理，先删除
    auto it = tileTextures_.find(key);
    if (it != tileTextures_.end()) {
        glDeleteTextures(1, &it->second);
        tileTextures_.erase(it);
    }

    GLuint tex = loadTileTexture(x, y, z, data, len);
    if (tex != 0) {
        tileTextures_[key] = tex;
        LOGI("Loaded tile texture: %s (tex=%u)", key.c_str(), tex);
    }
}

void MapRenderer::renderTile(int x, int y, int z) {
    std::string key = tileKey(x, y, z);
    auto it = tileTextures_.find(key);
    if (it == tileTextures_.end()) return;

    // 瓦片 NDC 位置
    float tileX, tileY, tileW, tileH;
    tileToGL(x, y, z, screenWidth_, screenHeight_, offsetX_, offsetY_,
             &tileX, &tileY, &tileW, &tileH);

    // 顶点数据：基于 tileX/tileY/tileW/tileH 的四边形
    float vertices[] = {
        // x      y       u     v
        tileX,  tileY + tileH,  0.0f, 0.0f,
        tileX + tileW, tileY + tileH,  1.0f, 0.0f,
        tileX + tileW, tileY,          1.0f, 1.0f,
        tileX,  tileY,          0.0f, 1.0f,
    };

    // 绑定 program
    glUseProgram(shaderProgram_);

    // 上传顶点数据（覆盖 vbo）
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    // 启用顶点属性
    glEnableVertexAttribArray(locPosition_);
    glVertexAttribPointer(locPosition_, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(locTexCoord_);
    glVertexAttribPointer(locTexCoord_, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(float), (void*)(2 * sizeof(float)));

    // MVP 矩阵
    glUniformMatrix4fv(locMVPMatrix_, 1, GL_FALSE, mvpMatrix_);

    // 绑定纹理
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, it->second);
    glUniform1i(locTexture_, 0);

    // 绘制
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
}

void MapRenderer::renderFrame() {
    if (!initialized_) return;

    glViewport(0, 0, screenWidth_, screenHeight_);
    glClearColor(0.92f, 0.95f, 0.98f, 1.0f); // 浅蓝灰色背景（模拟天空）
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 确定可见瓦片范围
    int centerTX, centerTY;
    latLonToTile(centerLat_, centerLon_, zoom_, &centerTX, &centerTY);

    // 以中心瓦片为中心，渲染周围 9 个瓦片 (3x3 网格)
    int range = 1; // 可扩展为更大范围
    for (int dy = -range; dy <= range; dy++) {
        for (int dx = -range; dx <= range; dx++) {
            int tx = centerTX + dx;
            int ty = centerTY + dy;
            renderTile(tx, ty, zoom_);
        }
    }

    eglSwapBuffers(display_, surface_);
}

void MapRenderer::cleanup() {
    if (!initialized_) return;

    // 删除纹理
    for (auto& kv : tileTextures_) {
        glDeleteTextures(1, &kv.second);
    }
    tileTextures_.clear();

    // 删除 GL 资源
    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (ibo_) { glDeleteBuffers(1, &ibo_); ibo_ = 0; }
    if (shaderProgram_) { glDeleteProgram(shaderProgram_); shaderProgram_ = 0; }

    // EGL 清理
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
            context_ = EGL_NO_CONTEXT;
        }
        if (surface_ != EGL_NO_SURFACE) {
            eglDestroySurface(display_, surface_);
            surface_ = EGL_NO_SURFACE;
        }
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }

    initialized_ = false;
    LOGI("MapRenderer cleaned up");
}

} // namespace maplibre
