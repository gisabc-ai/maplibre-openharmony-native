// Index.ets — 地图页面（增强版）
//
// 功能：
// - 搜索框（POI 搜索 via Nominatim）
// - 截图按钮
// - 图层切换按钮
// - 比例尺
// - 手势识别（平移、缩放）

import mapview from '@ohos.napi.mapview'

declare function tile_load_async(
  x: number, y: number, z: number, key: string,
  onSuccess: (data: Uint8Array) => void,
  onError: (msg: string) => void
): void;

declare function mapview_init(nativeWindow: object): boolean;
declare function mapview_destroy(): void;
declare function mapview_set_center(lon: number, lat: number, zoom: number): void;
declare function mapview_set_screen_size(width: number, height: number): void;
declare function mapview_load_tile(x: number, y: number, z: number, data: Uint8Array): void;
declare function mapview_render(): void;
declare function mapview_pan(dx: number, dy: number): void;
declare function mapview_zoom(delta: number, mouseX: number, mouseY: number): void;
declare function mapview_zoom_by(scaleFactor: number, centerX: number, centerY: number): void;

@Entry
@Component
struct Index {
    @State private message: string = '🗺️ MapLibre OpenHarmony';
    @State private zoomLevel: number = 10;
    @State private centerLon: number = 116.4;
    @State private centerLat: number = 39.9;
    @State private tiandituKey: string = '';
    @State private statusText: string = '初始化中...';
    @State private searchText: string = '';
    @State private layerIndex: number = 0;
    @State private scaleText: string = '100km';

    private mapViewId: string = 'main_map';
    private lastPanX: number = 0;
    private lastPanY: number = 0;
    private lastPinchScale: number = 1.0;

    aboutToAppear() {
        this.initializeMap();
    }

    aboutToDisappear() {
        mapview_destroy();
    }

    private initializeMap() {
        try {
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
            let display = display.getDefaultDisplaySync();
            mapview_set_screen_size(display.width, display.height);
            mapview_set_center(this.centerLon, this.centerLat, this.zoomLevel);
            mapview_render();
            this.loadCenterTiles();
            this.updateScaleText();
            this.statusText = '就绪 ✓';
        } catch (e) {
            this.statusText = '异常：' + JSON.stringify(e);
        }
    }

    private loadCenterTiles() {
        let tx = this.lonToTileX(this.centerLon, this.zoomLevel);
        let ty = this.latToTileY(this.centerLat, this.zoomLevel);
        let range = 1;
        for (let dy = -range; dy <= range; dy++) {
            for (let dx = -range; dx <= range; dx++) {
                this.loadTile(tx + dx, ty + dy, this.zoomLevel);
            }
        }
    }

    private loadTile(x: number, y: number, z: number) {
        if (this.tiandituKey === '' || this.tiandituKey.length < 10) {
            mapview_render();
            return;
        }
        tile_load_async(
            x, y, z, this.tiandituKey,
            (data: Uint8Array) => {
                mapview_load_tile(x, y, z, data);
                mapview_render();
            },
            (msg: string) => {
                console.error(`Tile load failed: ${x}/${y}/${z}: ${msg}`);
                mapview_render();
            }
        );
    }

    // ---- POI 搜索（Nominatim）----
    private async searchPlace(query: string) {
        if (!query || query.trim().length < 2) return [];
        try {
            let resp = await fetch(
                `https://nominatim.openstreetmap.org/search?format=json&q=${encodeURIComponent(query)}&limit=5`,
                { headers: { 'Accept-Language': 'zh,en' } }
            );
            if (!resp.ok) return [];
            return await resp.json();
        } catch {
            return [];
        }
    }

    private async doSearch() {
        let q = this.searchText.trim();
        if (!q) return;
        let results: any[] = await this.searchPlace(q);
        if (results.length === 0) {
            this.statusText = '未找到结果';
            return;
        }
        let r = results[0];
        let lon = parseFloat(r.lon);
        let lat = parseFloat(r.lat);
        this.centerLon = lon;
        this.centerLat = lat;
        mapview_set_center(this.centerLon, this.centerLat, this.zoomLevel);
        this.loadCenterTiles();
        mapview_render();
        this.statusText = `已定位: ${r.display_name.split(',')[0]}`;
        this.updateScaleText();
    }

    // ---- 截图 ----
    private takeScreenshot() {
        // 调用 native 截图接口（需 NAPI 实现）
        this.statusText = '📸 截图功能（需 NAPI 实现）';
    }

    // ---- 图层切换 ----
    private layerNames: string[] = ['矢量底图', 'OSM街道', '卫星影像', '水彩风格', '黑白高对比'];
    private switchLayer() {
        this.layerIndex = (this.layerIndex + 1) % this.layerNames.length;
        this.statusText = '图层: ' + this.layerNames[this.layerIndex];
        // 实际切换由 native 层处理
    }

    // ---- 比例尺 ----
    private updateScaleText() {
        // 根据缩放级别计算比例尺
        let resolutions = [156543.03, 78271.52, 39135.76, 19567.88, 9783.94, 4891.97, 2445.98, 1222.99, 611.50, 305.75, 152.87, 76.44, 38.22, 19.11, 9.55, 4.78, 2.39, 1.19, 0.60];
        let res = resolutions[this.zoomLevel] || 1000;
        let mPerPixel = res;
        let screenWidth = 200; // 比例尺像素宽度
        let realWidth = mPerPixel * screenWidth;
        if (realWidth > 1000) {
            this.scaleText = (realWidth / 1000).toFixed(1) + ' km';
        } else {
            this.scaleText = realWidth.toFixed(0) + ' m';
        }
    }

    // ---- 坐标转换 ----
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
            this.updateScaleText();
            mapview_set_center(this.centerLon, this.centerLat, this.zoomLevel);
            this.loadCenterTiles();
            mapview_render();
        }
    }

    private handlePinch(scale: number, centerX: number, centerY: number) {
        if (Math.abs(scale - 1.0) > 0.01) {
            mapview_zoom_by(scale, centerX, centerY);
            mapview_render();
        }
    }

    build() {
        Column() {
            // ---- 顶部搜索栏 ----
            Row() {
                TextInput({ placeholder: '🔍 搜索地点、POI...', text: this.searchText })
                    .height(38)
                    .fontSize(15)
                    .backgroundColor('#F5F5F5')
                    .borderRadius(19)
                    .padding({ left: 14, right: 14 })
                    .flexGrow(1)
                    .onChange((v) => { this.searchText = v; })
                    .onSubmit(() => this.doSearch())

                Button('搜索')
                    .height(36)
                    .fontSize(14)
                    .margin({ left: 8 })
                    .onClick(() => this.doSearch())
            }
            .width('100%')
            .padding({ left: 16, right: 16, top: 10, bottom: 6 })
            .backgroundColor('#FFFFFF')

            // ---- 顶部状态栏 ----
            Row() {
                Text(this.message)
                    .fontSize(16)
                    .fontWeight(FontWeight.Bold)
                    .fontColor('#1a73e8')

                Blank()

                // 比例尺
                Row() {
                    Text('📏')
                        .fontSize(14)
                    Text(this.scaleText)
                        .fontSize(13)
                        .fontColor('#666')
                        .margin({ left: 4 })
                }

                Text(' | ')
                    .fontColor('#ddd')

                Text(`Z${this.zoomLevel}`)
                    .fontSize(13)
                    .fontColor('#666')
            }
            .width('100%')
            .padding({ left: 16, right: 16, top: 6, bottom: 6 })
            .backgroundColor('#FFFFFF')

            // ---- 地图区域 ----
            Stack() {
                MapView({
                    mapViewId: this.mapViewId,
                    onPan: (dx: number, dy: number) => this.handlePan(dx, dy),
                    onZoom: (delta: number, mx: number, my: number) => this.handleZoom(delta, mx, my),
                    onPinch: (scale: number, cx: number, cy: number) => this.handlePinch(scale, cx, cy),
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

                // 右上角快捷操作
                Column({ space: 8 }) {
                    Button('🗺️')
                        .fontSize(18)
                        .width(40)
                        .height(40)
                        .backgroundColor('#FFFFFFCC')
                        .borderRadius(20)
                        .shadow({ radius: 4, color: '#22000000' })
                        .onClick(() => this.switchLayer())

                    Button('📷')
                        .fontSize(18)
                        .width(40)
                        .height(40)
                        .backgroundColor('#FFFFFFCC')
                        .borderRadius(20)
                        .shadow({ radius: 4, color: '#22000000' })
                        .onClick(() => this.takeScreenshot())
                }
                .position({ right: 10, top: 10 })
            }
            .width('100%')
            .layoutWeight(1)

            // ---- 底部控制栏 ----
            Row() {
                Button('🔍+')
                    .fontSize(14)
                    .height(36)
                    .onClick(() => this.handleZoom(1, 0, 0))

                Button('🔍-')
                    .fontSize(14)
                    .height(36)
                    .margin({ left: 10 })
                    .onClick(() => this.handleZoom(-1, 0, 0))

                Button('📍 北京')
                    .fontSize(14)
                    .height(36)
                    .margin({ left: 10 })
                    .onClick(() => {
                        this.centerLon = 116.4; this.centerLat = 39.9; this.zoomLevel = 10;
                        mapview_set_center(this.centerLon, this.centerLat, this.zoomLevel);
                        this.loadCenterTiles(); this.updateScaleText(); mapview_render();
                        this.statusText = '已定位: 北京';
                    })

                Button('📍 上海')
                    .fontSize(14)
                    .height(36)
                    .margin({ left: 10 })
                    .onClick(() => {
                        this.centerLon = 121.47; this.centerLat = 31.23; this.zoomLevel = 10;
                        mapview_set_center(this.centerLon, this.centerLat, this.zoomLevel);
                        this.loadCenterTiles(); this.updateScaleText(); mapview_render();
                        this.statusText = '已定位: 上海';
                    })

                Button('📍 深圳')
                    .fontSize(14)
                    .height(36)
                    .margin({ left: 10 })
                    .onClick(() => {
                        this.centerLon = 114.06; this.centerLat = 22.54; this.zoomLevel = 10;
                        mapview_set_center(this.centerLon, this.centerLat, this.zoomLevel);
                        this.loadCenterTiles(); this.updateScaleText(); mapview_render();
                        this.statusText = '已定位: 深圳';
                    })
            }
            .width('100%')
            .padding(12)
            .justifyContent(FlexAlign.Center)
            .backgroundColor('#FFFFFF')
        }
        .width('100%')
        .height('100%')
    }
}

// ============================================================
// MapView — 手势封装组件
// ============================================================
@Component
struct MapView {
    mapViewId: string = '';
    onPan?: (dx: number, dy: number) => void;
    onZoom?: (delta: number, mouseX: number, mouseY: number) => void;
    onPinch?: (scale: number, centerX: number, centerY: number) => void;

    private lastPanX: number = 0;
    private lastPanY: number = 0;
    private lastPinchDist: number = 0;
    private activeGestures: number = 0;

    build() {
        Stack() {
            nativeSurface({ id: this.mapViewId })
                .width('100%')
                .height('100%')
        }
        .width('100%')
        .height('100%')
        .gesture(
            PanGesture()
                .onActionStart((event) => {
                    this.lastPanX = event.globalX;
                    this.lastPanY = event.globalY;
                    this.activeGestures = 1;
                })
                .onActionUpdate((event) => {
                    if (this.activeGestures === 1 && this.onPan) {
                        let dx = event.globalX - this.lastPanX;
                        let dy = event.globalY - this.lastPanY;
                        this.onPan(dx, dy);
                        this.lastPanX = event.globalX;
                        this.lastPanY = event.globalY;
                    }
                })
                .onActionEnd(() => { this.activeGestures = 0; })
        )
        .pinchGesture(
            PinchGesture()
                .onActionStart((event) => {
                    this.lastPinchDist = 1.0;
                    this.activeGestures = 2;
                })
                .onActionUpdate((event) => {
                    if (this.activeGestures === 2 && this.onPinch) {
                        let scale = event.scale;
                        this.onPinch(scale, event.globalX, event.globalY);
                    }
                })
                .onActionEnd(() => { this.activeGestures = 0; })
        )
    }
}

@Component
struct nativeSurface {
    id: string = '';
    build() {
        // 占位：真实渲染由 native 层完成
    }
}
