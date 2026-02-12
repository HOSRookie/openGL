import { GLEXComponent } from '@yunshen1933/ohos_glex';

@Entry
@Component
struct Index {
  @State fpsText: string = 'FPS: --';

  build() {
    Column() {
      // 标题
      Text('GLEX 渲染框架演示')
        .fontSize(20)
        .fontWeight(FontWeight.Bold)
        .fontColor('#FFFFFF')
        .margin({ top: 48, bottom: 16 })

      // OpenGL 渲染区域
      GLEXComponent({
        targetFPS: 60,
        clearColor: [0.02, 0.03, 0.10, 1.0],
        autoStart: true,
      })
        .width('100%')
        .height('70%')
        .borderRadius(12)
        .clip(true)

      // 说明文字
      Text('星空 + 流星 · OpenGL ES 3.0')
        .fontSize(14)
        .fontColor('#88FFFFFF')
        .margin({ top: 16 })

      Text('基于 RenderPass + RenderPipeline 架构')
        .fontSize(12)
        .fontColor('#55FFFFFF')
        .margin({ top: 8 })
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#0A0B1A')
    .padding({ left: 16, right: 16 })
  }
}
