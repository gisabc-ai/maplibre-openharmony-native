# MapLibre OpenHarmony Native — 构建指南

> Phase 1: OpenGL ES 最小可运行 Demo

## 目录

1. [环境要求](#环境要求)
2. [项目结构](#项目结构)
3. [天地图 Key 申请](#天地图-key-申请)
4. [DevEco Studio 导入](#deveco-studio-导入)
5. [NDK 编译](#ndk-编译)
6. [真机运行](#真机运行)
7. [已知限制](#已知限制)

---

## 环境要求

| 项目 | 版本 |
|------|------|
| OpenHarmony SDK | 4.1+ |
| DevEco Studio | 4.0+ |
| Node.js | 16+ |
| Java | JDK 11+ |
| Android NDK | r23c+ |

---

## 项目结构

```
maplibre-openharmony-native/
├── phase1_opengl_demo/
│   ├── entry/                    # ArkTS 主模块
│   │   ├── build.gradle
│   │   └── src/main/
│   │       ├── ets/
│   │       │   ├── MainAbility/
│   │       │   │   └── Index.ets
│   │       │   └── pages/
│   │       │       └── Index.ets  ← 地图页面
│   │       ├── module.json5
│   │       └── resources/
│   └── native/                   # C++ Native 模块
│       ├── build.gradle
│       └── src/
│           ├── main.cpp          ← NAPI 入口
│           ├── map_renderer.cpp  ← OpenGL ES 渲染器
│           ├── map_renderer.h
│           ├── tile_loader.cpp   ← 瓦片下载器
│           ├── tile_loader.h
│           ├── coordinate.cpp    ← Mercator 坐标转换
│           ├── coordinate.h
│           ├── shaders/
│           │   ├── tile.vert
│           │   └── tile.frag
│           └── CMakeLists.txt
└── PHASE_PLAN.md
```

---

## 天地图 Key 申请

1. 访问 [天地图控制台](https://console.tianditu.gov.cn/)
2. 注册账号并登录
3. 创建应用，获取 **Web 端 Key**
4. 在 `Index.ets` 中找到：

```typescript
@State private tiandituKey: string = '请输入天地图Key';
```

替换为你的 Key：

```typescript
@State private tiandituKey: string = '你的天地图Web端Key';
```

---

## DevEco Studio 导入

### 方法一：直接打开（推荐）

1. 打开 DevEco Studio
2. 选择 **File → Open**
3. 导航到 `~/workspace/maplibre-openharmony-native/phase1_opengl_demo/`
4. 选择 `entry` 文件夹
5. 点击 **OK**

### 方法二：命令行导入

```bash
cd ~/workspace/maplibre-openharmony-native/phase1_opengl_demo/entry
# 使用 DevEco 的 headless 构建工具（如果有）
```

### 配置步骤

1. **配置 OpenHarmony SDK**
   - 打开 DevEco Studio 设置
   - 进入 **SDK Manager**
   - 确保 OpenHarmony API 4.1+ 已安装
   - 确保 NDK 已安装（r23c 或更高）

2. **配置签名**
   - 进入 **Project Structure → Signing**
   - 使用自动生成的调试签名
   - 或导入自有签名证书

3. **Sync 项目**
   - 点击右上角 **Sync Now**
   - 等待 Gradle Sync 完成

4. **构建 Native 模块**
   - DevEco 会自动执行 CMake 构建
   - 如需手动构建：
     ```bash
     cd native/src
     mkdir build && cd build
     cmake .. -DOH_SDK_ROOT=$OH_SDK_ROOT
     make
     ```

---

## NDK 编译

### CMake 配置说明

```cmake
# 关键依赖
find_library(log-lib    log)       # Android/OpenHarmony 日志
find_library(EGL-lib   EGL)       # OpenGL ES 平台绑定
find_library(GLESv2-lib GLESv2)   # OpenGL ES 2.0 API
find_library(curl-lib  curl)      # HTTP 瓦片下载（可选）

target_link_libraries(maplibre_ohos
    ${log-lib}
    ${EGL-lib}
    ${GLESv2-lib}
    ${curl-lib}
)
```

### 常见编译错误

| 错误 | 原因 | 解决方法 |
|------|------|----------|
| `EGL not found` | NDK 未配置 sysroot | 设置 `CMAKE_SYSROOT` |
| `GLESv2: undefined` | 未链接 GLES 库 | 确保 `find_library` 成功 |
| `curl not found` | 缺少 libcurl | 下载 curl 源码并编译到 NDK |
| `stbi not found` | 缺少图片解码 | 使用 stb_image.h（内嵌）|

### 手动编译 Native 模块

```bash
export OH_SDK_ROOT=/path/to/oh-sdk
cd ~/workspace/maplibre-openharmony-native/phase1_opengl_demo/native/src
mkdir -p build && cd build

cmake .. \
  -DOH_SDK_ROOT=$OH_SDK_ROOT \
  -DCMAKE_TOOLCHAIN_FILE=$OH_SDK_ROOT/build/cmake/ohos.toolchain.cmake \
  -DOHOS_ARCH=arm64-v8a

make -j$(nproc)

# 输出: libmaplibre_ohos.so
```

---

## 真机运行

1. **连接设备**
   ```bash
   hdc shell                   # 确认设备已连接
   ```

2. **安装应用**
   ```bash
   hdc install entry/build/outputs/hap/*.hap
   ```

3. **运行**
   - 方式一：Deveco 点击 **Run** 按钮
   - 方式二：
     ```bash
     hdc shell bm install -r /sdcard/xxx.hap
     hdc shell aa start -a MainAbility -b com.maplibre.ohos
     ```

4. **查看日志**
   ```bash
   hdc shell "logcat | grep MapRenderer"
   ```

---

## 已知限制

### Phase 1

- ✅ OpenGL ES 2.0 初始化（EGL + GL Context）
- ✅ 全屏四边形渲染
- ✅ 瓦片下载（curl + 天地图）
- ✅ 纹理加载（占位色块，无真实 PNG 解码）
- ✅ 坐标转换（Mercator）
- ✅ 手势 Pan（平移）
- ⚠️ 缩放（框架已就绪，缩放中心逻辑待完善）
- ⚠️ PNG/JPEG 解码（需接入 stb_image 或 libpng）
- ⚠️ 瓦片 LRU 缓存
- ⚠️ 层级调度

### 图片解码

Phase 1 使用占位纹理（纯色色块）。真实瓦片图片需要图片解码库：

**推荐：stb_image.h（单文件头文件库）**
- 下载：`https://raw.githubusercontent.com/nothings/stb/master/stb_image.h`
- 使用：将头文件加入源码，在 `loadTileTexture()` 中调用：
  ```cpp
  #define STB_IMAGE_IMPLEMENTATION
  #include "stb_image.h"
  
  int w, h, channels;
  stbi_set_flip_vertically_on_load(1);
  uint8_t* pixels = stbi_load_from_memory(data, len, &w, &h, &channels, 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  stbi_image_free(pixels);
  ```

---

## 下一步

参见 [PHASE_PLAN.md](./PHASE_PLAN.md) 查看 Phase 2 规划。

---

## 许可证

本项目代码遵循 MIT 许可证。
