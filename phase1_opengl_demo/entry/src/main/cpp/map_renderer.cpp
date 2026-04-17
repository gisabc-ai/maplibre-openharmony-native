#include "map_renderer.h"
#include "coordinate.h"
#include "gesture_handler.h"
#include <android/log.h>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <string>

#define LOG_TAG "MapRenderer"
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace maplibre {

// ============================================================
// LRUTextureCache
// ============================================================
LRUTextureCache::LRUTextureCache(size_t maxEntries) : maxEntries_(maxEntries) {}

LRUTextureCache::~LRUTextureCache() { clear(); }

GLuint LRUTextureCache::find(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(key);
    if (it == cache_.end()) return 0;

    // 提升到最前
    auto& node = it->second;
    if (node != lruList_.begin()) {
        lruList_.splice(lruList_.begin(), lruList_, node);
    }
    return node->second;
}

void LRUTextureCache::insert(const std::string& key, GLuint tex) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        // 删除旧纹理
        glDeleteTextures(1, &it->second->second);
        it->second->second = tex;
        lruList_.splice(lruList_.begin(), lruList_, it->second);
        return;
    }

    // 淘汰最旧的
    while (lruList_.size() >= maxEntries_) {
        auto back = lruList_.back();
        glDeleteTextures(1, &back.second);
        cache_.erase(back.first);
        lruList_.pop_back();
    }

    lruList_.push_front({key, tex});
    cache_[key] = lruList_.begin();
}

void LRUTextureCache::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        glDeleteTextures(1, &it->second->second);
        lruList_.erase(it->second);
        cache_.erase(it);
    }
}

void LRUTextureCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& kv : lruList_) glDeleteTextures(1, &kv.second);
    lruList_.clear();
    cache_.clear();
}

size_t LRUTextureCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}

// ============================================================
// Shader 源码
// ============================================================
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

static const char FRAG_SHADER[] = R"(
precision mediump float;
varying vec2 v_texCoord;
uniform sampler2D u_texture;

void main() {
    gl_FragColor = texture2D(u_texture, v_texCoord);
}
)";

// ============================================================
// 矩阵工具
// ============================================================
static void identityMatrix(float* m) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

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

void MapRenderer::multiplyMatrix(float* out, const float* a, const float* b) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            float sum = 0;
            for (int k = 0; k < 4; k++) sum += a[i + k*4] * b[k + j*4];
            out[i + j*4] = sum;
        }
    }
}

void MapRenderer::perspectiveMatrix(float* m, float fovY, float aspect, float near, float far) {
    float f = 1.0f / tanf(fovY * 0.5f);
    memset(m, 0, 16 * sizeof(float));
    m[0]  = f / aspect;
    m[5]  = f;
    m[10] = (far + near) / (near - far);
    m[11] = -1.0f;
    m[14] = 2 * far * near / (near - far);
}

void MapRenderer::lookAtMatrix(float* m, float eyeX, float eyeY, float eyeZ,
                               float centerX, float centerY, float centerZ,
                               float upX, float upY, float upZ) {
    float fx = centerX - eyeX, fy = centerY - eyeY, fz = centerZ - eyeZ;
    float flen = sqrtf(fx*fx + fy*fy + fz*fz);
    fx /= flen; fy /= flen; fz /= flen;

    float sx = fy*upZ - fz*upY, sy = fz*upX - fx*upZ, sz = fx*upY - fy*upX;
    float slen = sqrtf(sx*sx + sy*sy + sz*sz);
    sx /= slen; sy /= slen; sz /= slen;

    float ux = sy*fz - sz*fy, uy = sz*fx - sx*fz, uz = sx*fy - sy*fx;

    m[0] = sx;  m[1] = ux;  m[2] = -fx; m[3] = 0;
    m[4] = sy;  m[5] = uy;  m[6] = -fy; m[7] = 0;
    m[8] = sz;  m[9] = uz;  m[10] = -fz; m[11] = 0;
    m[12] = -(sx*eyeX + sy*eyeY + sz*eyeZ);
    m[13] = -(ux*eyeX + uy*eyeY + uz*eyeZ);
    m[14] = -(-fx*eyeX + -fy*eyeY + -fz*eyeZ);
    m[15] = 1.0f;
}

// ============================================================
// MapRenderer
// ============================================================
MapRenderer::MapRenderer()
    : display_(EGL_NO_DISPLAY), context_(EGL_NO_CONTEXT), surface_(EGL_NO_SURFACE), config_(nullptr)
    , shaderProgram_(0), vbo_(0), ibo_(0), gridVbo_(0), gridIbo_(0)
    , locPosition_(-1), locTexCoord_(-1), locMVPMatrix_(-1), locTexture_(-1)
    , textureCache_(64)
    , centerLon_(116.4), centerLat_(39.9), zoom_(10)
    , screenWidth_(1920), screenHeight_(1080)
    , offsetX_(0.0f), offsetY_(0.0f)
    , scale_(1.0f), rotation_(0.0f), tilt_(0.0f)
    , gestureZoomScale_(1.0f)
    , gestureHandler_(nullptr)
    , initialized_(false)
{
    identityMatrix(modelMatrix_);
    identityMatrix(viewMatrix_);
    identityMatrix(projMatrix_);
    identityMatrix(mvpMatrix_);
}

MapRenderer::~MapRenderer() { cleanup(); }

bool MapRenderer::initialize(void* nativeWindow) {
    if (initialized_) return true;
    if (!initEGL(nativeWindow)) { LOGE("EGL init failed"); return false; }
    if (!initGLES()) { LOGE("GLES init failed"); cleanup(); return false; }
    initialized_ = true;
    LOGI("MapRenderer initialized (enhanced)");
    return true;
}

bool MapRenderer::initEGL(void* nativeWindow) {
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY) { LOGE("eglGetDisplay failed"); return false; }

    EGLint major, minor;
    if (!eglInitialize(display_, &major, &minor)) { LOGE("eglInitialize failed"); return false; }
    LOGI("EGL %d.%d", major, minor);

    EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE
    };
    EGLint numConfigs = 0;
    if (!eglChooseConfig(display_, configAttribs, &config_, 1, &numConfigs) || numConfigs == 0) {
        LOGE("eglChooseConfig failed"); return false;
    }

    EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    context_ = eglCreateContext(display_, config_, EGL_NO_CONTEXT, contextAttribs);
    if (context_ == EGL_NO_CONTEXT) { LOGE("eglCreateContext failed"); return false; }

    surface_ = eglCreateWindowSurface(display_, config_,
        static_cast<EGLNativeWindowType>(nativeWindow), nullptr);
    if (surface_ == EGL_NO_SURFACE) { LOGE("eglCreateWindowSurface failed"); return false; }

    if (!eglMakeCurrent(display_, surface_, surface_, context_)) { LOGE("eglMakeCurrent failed"); return false; }
    return true;
}

bool MapRenderer::initGLES() {
    GLuint vertShader = 0, fragShader = 0;
    if (!compileShader(GL_VERTEX_SHADER, VERT_SHADER, &vertShader)) return false;
    if (!compileShader(GL_FRAGMENT_SHADER, FRAG_SHADER, &fragShader)) {
        glDeleteShader(vertShader); return false;
    }
    if (!linkProgram(vertShader, fragShader, &shaderProgram_)) {
        glDeleteShader(vertShader); glDeleteShader(fragShader); return false;
    }
    glDeleteShader(vertShader); glDeleteShader(fragShader);

    locPosition_   = glGetAttribLocation(shaderProgram_, "a_position");
    locTexCoord_   = glGetAttribLocation(shaderProgram_, "a_texCoord");
    locMVPMatrix_  = glGetUniformLocation(shaderProgram_, "u_mvpMatrix");
    locTexture_    = glGetUniformLocation(shaderProgram_, "u_texture");

    // 全屏四边形顶点: x, y, u, v
    const float quadVerts[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 0.0f,
    };
    const uint16_t quadIndices[] = { 0, 1, 2, 0, 2, 3 };

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

    glGenBuffers(1, &ibo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices), quadIndices, GL_STATIC_DRAW);

    // 网格线顶点（用于调试瓦片边界）
    float gridVerts[8*2]; // 4 corners × (x,y)
    const float gridH = 2.0f / (1 << 10); // 一块瓦片的高度（NDC）
    // 预分配简化：实际根据可见瓦片动态生成
    glGenBuffers(1, &gridVbo_);
    glGenBuffers(1, &gridIbo_);

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
        glDeleteShader(shader); return false;
    }
    *outShader = shader; return true;
}

bool MapRenderer::linkProgram(GLuint vert, GLuint frag, GLuint* outProg) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        LOGE("Program link error: %s", log);
        glDeleteProgram(prog); return false;
    }
    *outProg = prog; return true;
}

void MapRenderer::setCenter(double lon, double lat, int zoom) {
    centerLon_ = lon; centerLat_ = lat; zoom_ = zoom;
    offsetX_ = 0.0f; offsetY_ = 0.0f;
    scale_ = 1.0f;
    gestureZoomScale_ = 1.0f;
    updateMVP();
}

void MapRenderer::setScreenSize(int width, int height) {
    screenWidth_ = width; screenHeight_ = height;
    updateMVP();
}

void MapRenderer::pan(int dx, int dy) {
    float scale = 2.0f / (screenWidth_ * (1 << zoom_) * scale_ * gestureZoomScale_);
    offsetX_ += dx * scale;
    offsetY_ -= dy * scale;
    updateMVP();
}

void MapRenderer::zoom(int delta, int mouseX, int mouseY) {
    int oldZoom = zoom_;
    zoom_ = std::max(1, std::min(19, zoom_ + delta));
    if (zoom_ != oldZoom) {
        scale_ = 1.0f;
        gestureZoomScale_ = 1.0f;
    }
    updateMVP();
}

void MapRenderer::zoomBy(float scaleFactor, int centerX, int centerY) {
    float oldScale = scale_ * gestureZoomScale_;
    float newScale = oldScale * scaleFactor;
    // 限制缩放范围
    float maxScale = static_cast<float>(1 << (zoom_ + 2));
    newScale = std::max(0.5f, std::min(newScale, maxScale));
    float combined = newScale / oldScale;
    gestureZoomScale_ *= combined;
    updateMVP();
}

void MapRenderer::updateMVP() {
    // 正交投影
    orthoMatrix(projMatrix_, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);

    // 缩放 + 平移
    identityMatrix(modelMatrix_);
    modelMatrix_[0]  = scale_ * gestureZoomScale_;
    modelMatrix_[5]  = scale_ * gestureZoomScale_;
    modelMatrix_[12] = offsetX_;
    modelMatrix_[13] = offsetY_;

    // View = Identity（正交投影不需要复杂view）
    identityMatrix(viewMatrix_);

    // MVP = Proj * View * Model
    float tmp[16];
    multiplyMatrix(tmp, viewMatrix_, modelMatrix_);
    multiplyMatrix(mvpMatrix_, projMatrix_, tmp);
}

std::string MapRenderer::tileKey(int x, int y, int z) const {
    char buf[64];
    snprintf(buf, sizeof(buf), "%d/%d/%d", z, x, y);
    return std::string(buf);
}

GLuint MapRenderer::loadTileTexture(int x, int y, int z, const uint8_t* data, size_t len) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Placeholder 彩色纹理（实际应使用 stb_image 解码）
    uint8_t placeholder[256 * 256 * 4];
    for (int i = 0; i < 256 * 256; i++) {
        placeholder[i*4+0] = (x * 37 + y * 17 + z * 31) % 256;
        placeholder[i*4+1] = (x * 13 + y * 41 + z * 19) % 256;
        placeholder[i*4+2] = 200;
        placeholder[i*4+3] = 255;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, placeholder);
    glGenerateMipmap(GL_TEXTURE_2D);
    return tex;
}

void MapRenderer::loadTile(int x, int y, int z, const uint8_t* data, size_t len) {
    if (!initialized_ || !data || len == 0) return;
    std::string key = tileKey(x, y, z);
    textureCache_.remove(key);
    GLuint tex = loadTileTexture(x, y, z, data, len);
    if (tex != 0) {
        textureCache_.insert(key, tex);
        LOGI("Loaded tile texture: %s", key.c_str());
    }
}

void MapRenderer::renderTile(int x, int y, int z) {
    std::string key = tileKey(x, y, z);
    GLuint tex = textureCache_.find(key);
    if (tex == 0) return;

    // 瓦片 NDC 位置
    float tileX, tileY, tileW, tileH;
    tileToGL(x, y, z, screenWidth_, screenHeight_, offsetX_, offsetY_,
             &tileX, &tileY, &tileW, &tileH);

    // 应用缩放因子
    float scale = scale_ * gestureZoomScale_;
    float cx = (tileX + tileX + tileW) * 0.5f;
    float cy = (tileY + tileY + tileH) * 0.5f;
    tileX = cx + (tileX - cx) / scale;
    tileY = cy + (tileY - cy) / scale;
    tileW /= scale;
    tileH /= scale;

    float vertices[] = {
        tileX,  tileY + tileH,  0.0f, 0.0f,
        tileX + tileW, tileY + tileH,  1.0f, 0.0f,
        tileX + tileW, tileY,          1.0f, 1.0f,
        tileX,  tileY,          0.0f, 1.0f,
    };

    glUseProgram(shaderProgram_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    glEnableVertexAttribArray(locPosition_);
    glVertexAttribPointer(locPosition_, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(locTexCoord_);
    glVertexAttribPointer(locTexCoord_, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glUniformMatrix4fv(locMVPMatrix_, 1, GL_FALSE, mvpMatrix_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(locTexture_, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
}

void MapRenderer::renderTileGrid() {
    // 瓦片网格线（调试用）
    int centerTX, centerTY;
    latLonToTile(centerLat_, centerLon_, zoom_, &centerTX, &centerTY);
    int range = 1;

    glLineWidth(1.0f);
    glDisable(GL_TEXTURE_2D);

    for (int dy = -range; dy <= range; dy++) {
        for (int dx = -range; dx <= range; dx++) {
            int tx = centerTX + dx;
            int ty = centerTY + dy;
            float tileX, tileY, tileW, tileH;
            tileToGL(tx, ty, zoom_, screenWidth_, screenHeight_, offsetX_, offsetY_,
                     &tileX, &tileY, &tileW, &tileH);

            float scale = scale_ * gestureZoomScale_;
            float cx = (tileX + tileX + tileW) * 0.5f;
            float cy = (tileY + tileY + tileH) * 0.5f;
            float rx = tileW / scale * 0.5f;
            float ry = tileH / scale * 0.5f;

            float lineVerts[] = {
                cx - rx, cy - ry, cx + rx, cy - ry,
                cx + rx, cy - ry, cx + rx, cy + ry,
                cx + rx, cy + ry, cx - rx, cy + ry,
                cx - rx, cy + ry, cx - rx, cy - ry,
            };

            glColor4f(0.0f, 0.5f, 1.0f, 0.3f);
            glVertexPointer(2, GL_FLOAT, 0, lineVerts);
            glDrawArrays(GL_LINES, 0, 8);
        }
    }
    glEnable(GL_TEXTURE_2D);
}

void MapRenderer::renderFrame() {
    if (!initialized_) return;

    glViewport(0, 0, screenWidth_, screenHeight_);
    glClearColor(0.92f, 0.95f, 0.98f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    int centerTX, centerTY;
    latLonToTile(centerLat_, centerLon_, zoom_, &centerTX, &centerTY);
    int range = 1;
    for (int dy = -range; dy <= range; dy++) {
        for (int dx = -range; dx <= range; dx++) {
            renderTile(centerTX + dx, centerTY + dy, zoom_);
        }
    }

    // 网格线（可选调试）
    // renderTileGrid();

    eglSwapBuffers(display_, surface_);
}

void MapRenderer::cleanup() {
    if (!initialized_) return;
    textureCache_.clear();
    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (ibo_) { glDeleteBuffers(1, &ibo_); ibo_ = 0; }
    if (gridVbo_) { glDeleteBuffers(1, &gridVbo_); gridVbo_ = 0; }
    if (gridIbo_) { glDeleteBuffers(1, &gridIbo_); gridIbo_ = 0; }
    if (shaderProgram_) { glDeleteProgram(shaderProgram_); shaderProgram_ = 0; }
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) { eglDestroyContext(display_, context_); context_ = EGL_NO_CONTEXT; }
        if (surface_ != EGL_NO_SURFACE) { eglDestroySurface(display_, surface_); surface_ = EGL_NO_SURFACE; }
        eglTerminate(display_); display_ = EGL_NO_DISPLAY;
    }
    initialized_ = false;
    LOGI("MapRenderer cleaned up");
}

} // namespace maplibre
