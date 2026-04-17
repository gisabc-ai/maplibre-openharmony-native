# MapLibre OpenHarmony Native — Phase Plan

## 整体目标
在 OpenHarmony 平台上用 Native C++ 实现 MapLibre GL JS 风格的地图渲染。

## 阶段划分

### Phase 1 ✅ (当前阶段)
**目标**：OpenGL ES 最小可运行 Demo
- [x] C++ Native 模块（NDK）
- [x] EGL + OpenGL ES 初始化
- [x] 简单四边形渲染
- [x] 瓦片图片下载（curl + 天地图）
- [x] 瓦片纹理渲染
- [x] ArkTS WebView 封装
- [x] 坐标转换工具（Mercator）

### Phase 2 (规划中)
**目标**：完整瓦片地图渲染
- [ ] 多级瓦片调度（可见范围 + 周边缓冲）
- [ ] 瓦片 LRU 缓存
- [ ] 平移/缩放手势支持
- [ ] 瓦片淡入动画

### Phase 3 (规划中)
**目标**：MapLibre GL JS 风格样式
- [ ] 样式 JSON 解析
- [ ] 图层渲染管线
- [ ] 符号/标注渲染
- [ ] 喷泉/进度条效果

## 技术栈
- **渲染**：OpenGL ES 2.0 / 3.0
- **原生语言**：C++ (NDK)
- **上层封装**：ArkTS + ArkUI
- **地图数据**：天地图 WMTS
- **网络**：libcurl
- **EGL**：Native Window EGL 绑定

## 关键文件
- `native/src/main.cpp` — NDK 入口
- `native/src/map_renderer.cpp` — OpenGL ES 渲染器
- `native/src/tile_loader.cpp` — 瓦片加载器
- `native/src/coordinate.cpp` — 坐标转换

## 构建方式
```bash
# 在 DevEco Studio 中打开 native 目录
cd native
mkdir build && cd build
cmake ..
make
```
