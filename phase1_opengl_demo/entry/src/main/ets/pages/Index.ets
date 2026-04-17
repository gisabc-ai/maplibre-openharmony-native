// Index.ets — 地图页面
//
// 这是地图展示的主页面，使用 MapView 组件。
// MapView 是在 native 模块中注册的自定义 ArkUI 组件。

import mapview from '@ohos.napi.mapview'

// 瓦片加载器（NAPI bridge）
// 这些函数由 libmaplibre_ohos.so 导出
declare function tile_load_async(
  x: number,
  y: number,
  z: number,
  key: string,
  onSuccess: (data: Uint8Array) => void,
  onError: (msg: string) => void
): void;

declare function mapview_init(nativeWindow: object): boolean;
declare function mapview_destroy(): void;
declare function mapview_set_center(lon: number, lat: number, zoom: number): void;
declare function mapview_set_zoom(zoom: number): void;
declare function mapview_set_screen_size(width: number, height: number): void;
declare function mapview_load_tile(x: number, y: number, z: number, data: Uint8Array): void;
declare function mapview_render(): void;
declare function mapview_pan(dx: number, dy: number): void;
declare function mapview_zoom(delta: number, mouseX: number, mouseY: number): void;

@Entry
@Component
struct Index {
  // 地图状态
  @State private message: string = 'OpenHarmony Map Demo';
  @State private zoomLevel: number = 10;
  @State private centerLon: number = 116.4;
  @State private centerLat: number = 39.9;
  @State private tiandituKey: string = '请输入天地图Key';
  @State private statusText: string = '初始化中...';

  // 内置 MapView 组件（由 native 模块提供）
  // 当 MapView 被创建时，触发 onAppear
  // 组件生命周期由 ArkUI 管理
  private mapViewId: string = 'main_map';

  aboutToAppear() {
    // 初始化地图
    this.initializeMap();
  }

  aboutToDisappear() {
    // 清理地图资源
    mapview_destroy();
  }

  private initializeMap() {
    try {
      // 1. 初始化渲染器（需要 NativeWindow）
      // 注意：在真实设备上，MapView 组件会提供 OHNativeWindow
      // 这里通过 ArkUI 组件环境获取
      let nativeWindow = mapview.getNativeWindow();

      if (!nativeWindow) {
        this.statusText = '错误：无法获取 NativeWindow';
        return;
      }

      let ok = mapview_init(nativeWindow);
      if (!ok) {
        this.statusText = '错误：渲染器初始化失败';
        return;
      }

      // 2. 设置屏幕尺寸
      let display = display.getDefaultDisplaySync();
      mapview_set_screen_size(display.width, display.height);

      // 3. 设置地图中心
      mapview_set_center(this.centerLon, this.centerLat, this.zoomLevel);

      // 4. 触发首帧渲染
      mapview_render();

      // 5. 异步加载中心瓦片
      this.loadCenterTiles();

      this.statusText = '就绪';
    } catch (e) {
      this.statusText = '异常：' + JSON.stringify(e);
    }
  }

  /**
   * 加载可见范围及周边的瓦片
   * 根据当前中心点和缩放级别，计算可见瓦片范围并异步加载
   */
  private loadCenterTiles() {
    // 简化实现：加载中心瓦片 + 周边 8 个
    // 真实实现需要根据屏幕尺寸和缩放级别计算可见瓦片范围
    let tx = this.lonToTileX(this.centerLon, this.zoomLevel);
    let ty = this.latToTileY(this.centerLat, this.zoomLevel);

    let range = 1; // 加载周围 1 圈
    for (let dy = -range; dy <= range; dy++) {
      for (let dx = -range; dx <= range; dx++) {
        this.loadTile(tx + dx, ty + dy, this.zoomLevel);
      }
    }
  }

  /**
   * 异步加载单个瓦片
   */
  private loadTile(x: number, y: number, z: number) {
    if (this.tiandituKey === '请输入天地图Key' || this.tiandituKey.length < 10) {
      // 没有有效 Key，跳过真实加载，渲染占位色块
      // libmapre_ohos 内部已有占位纹理逻辑
      mapview_render();
      return;
    }

    tile_load_async(
      x, y, z, this.tiandituKey,
      (data: Uint8Array) => {
        // 下载成功，加载到渲染器
        mapview_load_tile(x, y, z, data);
        mapview_render();
      },
      (msg: string) => {
        // 下载失败，使用占位纹理
        console.error(`Tile load failed: ${x}/${y}/${z}: ${msg}`);
        mapview_render();
      }
    );
  }

  // ---- 坐标转换辅助 ----

  private lonToTileX(lon: number, zoom: number): number {
    return Math.floor((lon + 180.0) / 360.0 * Math.pow(2, zoom));
  }

  private latToTileY(lat: number, zoom: number): number {
    let rad = lat * Math.PI / 180.0;
    let log = Math.log((1.0 + Math.sin(rad)) / (1.0 - Math.sin(rad))) / 4.0;
    return Math.floor((0.5 - log / Math.PI) * Math.pow(2, zoom));
  }

  // ---- 手势处理 ----

  private handlePan(dx: number, dy: number) {
    mapview_pan(dx, dy);
    mapview_render();
  }

  private handleZoom(delta: number, mouseX: number, mouseY: number) {
    let newZoom = Math.max(1, Math.min(19, this.zoomLevel + delta));
    if (newZoom !== this.zoomLevel) {
      this.zoomLevel = newZoom;
      mapview_set_center(this.centerLon, this.centerLat, this.zoomLevel);
      this.loadCenterTiles();
      mapview_render();
    }
  }

  build() {
    Column() {
      // 顶部状态栏
      Row() {
        Text(this.message)
          .fontSize(20)
          .fontWeight(FontWeight.Bold)
          .fontColor('#333333')

        Blank()

        Text(`缩放: ${this.zoomLevel}`)
          .fontSize(14)
          .fontColor('#666666')

        Text(' | ')
          .fontColor('#cccccc')

        Text(`中心: ${this.centerLon.toFixed(4)}, ${this.centerLat.toFixed(4)}`)
          .fontSize(14)
          .fontColor('#666666')
      }
      .width('100%')
      .padding({ left: 16, right: 16, top: 12, bottom: 8 })
      .backgroundColor('#F5F5F5')

      // 地图区域（使用 MapView 自定义组件）
      // MapView 内部持有 OHNativeWindow，负责 OpenGL ES 渲染
      Stack() {
        MapView({
          mapViewId: this.mapViewId,
          onPan: (dx: number, dy: number) => this.handlePan(dx, dy),
          onZoom: (delta: number, mouseX: number, mouseY: number) =>
                   this.handleZoom(delta, mouseX, mouseY),
        })
        .width('100%')
        .layoutWeight(1)

        // 状态浮层
        Text(this.statusText)
          .fontSize(12)
          .fontColor('#FFFFFF')
          .backgroundColor('#88000000')
          .padding({ left: 8, right: 8, top: 4, bottom: 4 })
          .borderRadius(4)
          .position({ x: 10, y: 10 })
      }
      .width('100%')
      .layoutWeight(1)

      // 底部控制栏
      Row() {
        Button('放大')
          .fontSize(14)
          .height(36)
          .onClick(() => this.handleZoom(1, 0, 0))

        Button('缩小')
          .fontSize(14)
          .height(36)
          .margin({ left: 12 })
          .onClick(() => this.handleZoom(-1, 0, 0))

        Button('北京')
          .fontSize(14)
          .height(36)
          .margin({ left: 12 })
          .onClick(() => {
            this.centerLon = 116.4;
            this.centerLat = 39.9;
            this.zoomLevel = 10;
            mapview_set_center(this.centerLon, this.centerLat, this.zoomLevel);
            this.loadCenterTiles();
            mapview_render();
          })

        Button('上海')
          .fontSize(14)
          .height(36)
          .margin({ left: 12 })
          .onClick(() => {
            this.centerLon = 121.47;
            this.centerLat = 31.23;
            this.zoomLevel = 10;
            mapview_set_center(this.centerLon, this.centerLat, this.zoomLevel);
            this.loadCenterTiles();
            mapview_render();
          })
      }
      .width('100%')
      .padding(12)
      .justifyContent(FlexAlign.Center)
      .backgroundColor('#F5F5F5')
    }
    .width('100%')
    .height('100%')
  }
}

/**
 * MapView — 自定义 ArkUI 组件
 * 
 * 该组件是对 native 渲染器的封装，负责：
 * 1. 获取 OHNativeWindow（OpenHarmony 原生窗口）
 * 2. 处理手势事件（平移、捏合缩放）
 * 3. 向 native 层传递配置参数
 * 
 * 在 OpenHarmony 4.1+ 中，自定义组件通过 Node-API 注册
 */
@Component
export struct MapView {
  mapViewId: string = '';
  onPan?: (dx: number, dy: number) => void;
  onZoom?: (delta: number, mouseX: number, mouseY: number) => void;

  // 手势状态
  private lastPanX: number = 0;
  private lastPanY: number = 0;
  private lastPinchDist: number = 0;

  aboutAppear() {
    // 组件出现时初始化 native 渲染器
  }

  build() {
    Stack() {
      // Native 层渲染输出（通过 OHNativeWindow 绑定到此处）
      nativeSurface({
        id: this.mapViewId
      })
      .width('100%')
      .height('100%')
    }
    .width('100%')
    .height('100%')
    .gesture(
      // 平移手势
      PanGesture()
        .onActionStart((event) => {
          this.lastPanX = event.globalX;
          this.lastPanY = event.globalY;
        })
        .onActionUpdate((event) => {
          let dx = event.globalX - this.lastPanX;
          let dy = event.globalY - this.lastPanY;
          if (this.onPan) {
            this.onPan(dx, dy);
          }
          this.lastPanX = event.globalX;
          this.lastPanY = event.globalY;
        })
    )
    .pinchGesture(
      // 捏合缩放手势
      PinchGesture()
        .onActionStart((event) => {
          this.lastPinchDist = 0;
        })
        .onActionUpdate((event) => {
          if (this.lastPinchDist !== 0) {
            let delta = event.scale > 1 ? 1 : -1;
            if (this.onZoom) {
              this.onZoom(delta, event.globalX, event.globalY);
            }
          }
          this.lastPinchDist = event.scale;
        })
    )
  }
}

/**
 * nativeSurface — 原生窗口占位组件
 * 
 * 这是一个虚拟组件，用于将 OHNativeWindow 绑定到 UI 树。
 * 真实实现由 OpenHarmony 的 native 渲染管道提供。
 */
@Component
struct nativeSurface {
  id: string = '';

  build() {
    // 占位：真实渲染由 native 层完成
    // 该节点在 UI 树中用于确定 surface 的布局边界
  }
}
