// MainAbility/Index.ets — 主入口页面
//
// 应用启动后显示的主页面，直接跳转到地图页面。

import router from '@ohos.router';

@Entry
@Component
struct MainIndex {
  aboutToAppear() {
    // 启动后直接跳转到地图页面
    setTimeout(() => {
      router.push({ url: 'pages/Index' });
    }, 500);
  }

  build() {
    Column() {
      Text('OpenHarmony MapLibre')
        .fontSize(24)
        .fontWeight(FontWeight.Bold)
        .fontColor('#1976D2')
        .textAlign(TextAlign.Center)
        .width('100%')
        .padding({ top: 80 })

      Text('Native C++ + OpenGL ES 渲染')
        .fontSize(16)
        .fontColor('#666666')
        .textAlign(TextAlign.Center)
        .width('100%')
        .padding({ top: 16 })

      LoadingProgress()
        .width(48)
        .height(48)
        .margin({ top: 40 })
        .color('#1976D2')

      Text('正在加载地图...')
        .fontSize(14)
        .fontColor('#999999')
        .textAlign(TextAlign.Center)
        .width('100%')
        .padding({ top: 16 })
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#FAFAFA')
  }
}
