import { BuiltinPass, GLEXComponent } from '@yunshen1933/ohos_glex';

class EffectOption {
  key: BuiltinPass;
  label: string;
  desc: string;

  constructor(key: BuiltinPass, label: string, desc: string) {
    this.key = key;
    this.label = label;
    this.desc = desc;
  }
}

const EFFECT_OPTIONS: EffectOption[] = [
    new EffectOption('demo', '星空', '星空 + 流星 · OpenGL ES 3.0'),
    new EffectOption('attack', '斩击', '割草斩击 · 触摸触发 · OpenGL ES 3.0'),
];

@Entry
@Component
struct Index {
  @State fpsText: string = 'FPS: --';
  @State selectedPass: BuiltinPass = EFFECT_OPTIONS[0].key;

  build() {
    Column() {
      // 选择特效
      Row({ space: 12 }) {
        ForEach(EFFECT_OPTIONS, (item: EffectOption) => {
          Button(item.label)
            .height(32)
            .padding({ left: 14, right: 14 })
            .backgroundColor(this.selectedPass === item.key ? '#3B82F6' : '#1C1E2A')
            .fontColor('#FFFFFF')
            .onClick(() => {
              this.selectedPass = item.key;
            })
        }, (item: EffectOption) => item.key)
      }
      .margin({ top: 32, bottom: 12 })

      // 标题
      Text('GLEX 渲染框架演示')
        .fontSize(20)
        .fontWeight(FontWeight.Bold)
        .fontColor('#FFFFFF')
        .margin({ bottom: 8 })

      // 说明文字
      Text(EFFECT_OPTIONS.find((item: EffectOption) => item.key === this.selectedPass)?.desc ?? '')
        .fontSize(14)
        .fontColor('#88FFFFFF')
        .margin({ bottom: 6 })

      Text('基于 RenderPass + RenderPipeline 架构')
        .fontSize(12)
        .fontColor('#55FFFFFF')
        .margin({ bottom: 16 })

      // OpenGL 渲染区域
      GLEXComponent({
        targetFPS: 60,
        clearColor: [0.02, 0.03, 0.10, 1.0],
        autoStart: true,
        builtinPass: this.selectedPass,
      })
        .width('100%')
        .height('70%')
        .borderRadius(12)
        .clip(true)
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#0A0B1A')
    .padding({ left: 16, right: 16 })
  }
}


