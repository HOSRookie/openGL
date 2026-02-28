import { createGlexRenderer, GlexNativeInstance, GLInfo } from '@yunshen1933/ohos_glex';

type BuiltinPass = 'demo' | 'attack' | 'none';

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
  @State selectedPass: BuiltinPass = EFFECT_OPTIONS[0].key;

  private xComponentId: string = 'glex_view_' + Date.now().toString();
  private native: GlexNativeInstance = createGlexRenderer();
  private initialized: boolean = false;
  private viewWidth: number = 0;
  private viewHeight: number = 0;
  private touchScaleX: number = 1.0;
  private touchScaleY: number = 1.0;

  private onSelectedPassChanged(): void {
    this.applyBuiltinPass();
  }

  private applyBuiltinPass(): void {
    if (!this.initialized) {
      return;
    }
    const passList: string[] = [];
    if (this.selectedPass === 'demo') {
      passList.push('DemoPass');
    } else if (this.selectedPass === 'attack') {
      passList.push('AttackPass');
    }
    try {
      this.native.setPasses(passList);
      this.reportLastError();
    } catch {
      // ignore
    }
  }

  private reportLastError(): void {
    try {
      const msg: string = this.native.getLastError();
      if (msg && msg.length > 0) {
        console.error('[GLEX] ' + msg);
        this.native.clearLastError();
      }
    } catch {
      // ignore
    }
  }

  private onRenderLoad(): void {
    try {
      this.native.bindXComponent(this.xComponentId);
      this.native.setTargetFPS(60);
      this.native.setBackgroundColor(0.02, 0.03, 0.10, 1.0);
      this.initialized = true;
      this.applyBuiltinPass();
      this.native.startRender();
      this.updateTouchScale();
      this.reportLastError();
    } catch {
      console.error('[GLEX] init failed');
    }
  }

  private onRenderDestroy(): void {
    try {
      this.native.stopRender();
      this.native.destroySurface();
      this.native.unbindXComponent();
    } catch {
      // ignore
    }
    this.initialized = false;
  }

  private handleTouch(event: TouchEvent): void {
    if (!this.initialized || this.selectedPass !== 'attack') {
      return;
    }
    const touches: TouchObject[] = (event.touches && event.touches.length > 0)
      ? event.touches
      : (event.changedTouches ?? []);
    if (!touches || touches.length === 0) {
      return;
    }
    const touch: TouchObject = touches[0];
    const x: number = touch.x * this.touchScaleX;
    const y: number = touch.y * this.touchScaleY;
    if (!Number.isFinite(x) || !Number.isFinite(y)) {
      return;
    }
    const maxX: number = this.viewWidth * this.touchScaleX;
    const maxY: number = this.viewHeight * this.touchScaleY;
    const clampedX: number = Math.min(Math.max(x, 0), maxX > 0 ? maxX : x);
    const clampedY: number = Math.min(Math.max(y, 0), maxY > 0 ? maxY : y);

    let action: number = 3;
    if (event.type === TouchType.Down) {
      action = 0;
    } else if (event.type === TouchType.Move) {
      action = 1;
    } else if (event.type === TouchType.Up) {
      action = 2;
    }

    try {
      this.native.setTouchEvent(clampedX, clampedY, action, touch.id);
      this.reportLastError();
    } catch {
      // ignore
    }
  }

  private updateTouchScale(): void {
    if (!this.initialized || this.viewWidth <= 0 || this.viewHeight <= 0) {
      return;
    }
    try {
      const info: GLInfo = this.native.getGLInfo();
      if (info.width > 0 && info.height > 0) {
        this.touchScaleX = info.width / this.viewWidth;
        this.touchScaleY = info.height / this.viewHeight;
      }
    } catch {
      // ignore
    }
  }

  private toNumber(value: number | string): number {
    if (typeof value === 'number') {
      return value;
    }
    const parsed: number = Number.parseFloat(value);
    return Number.isFinite(parsed) ? parsed : 0;
  }

  build() {
    Column() {
      Row({ space: 12 }) {
        ForEach(EFFECT_OPTIONS, (item: EffectOption) => {
          Button(item.label)
            .height(32)
            .padding({ left: 14, right: 14 })
            .backgroundColor(this.selectedPass === item.key ? '#3B82F6' : '#1C1E2A')
            .fontColor('#FFFFFF')
            .onClick(() => {
              this.selectedPass = item.key;
              this.onSelectedPassChanged();
            });
        }, (item: EffectOption) => item.key);
      }
      .margin({ top: 32, bottom: 12 });

      Text('GLEX 渲染框架演示')
        .fontSize(20)
        .fontWeight(FontWeight.Bold)
        .fontColor('#FFFFFF')
        .margin({ bottom: 8 });

      Text(EFFECT_OPTIONS.find((item: EffectOption) => item.key === this.selectedPass)?.desc ?? '')
        .fontSize(14)
        .fontColor('#88FFFFFF')
        .margin({ bottom: 6 });

      Text('基于 RenderPass + RenderPipeline 架构')
        .fontSize(12)
        .fontColor('#55FFFFFF')
        .margin({ bottom: 16 });

      Stack() {
        XComponent({
          id: this.xComponentId,
          type: XComponentType.SURFACE,
          libraryname: 'glex',
        })
          .width('100%')
          .height('100%')
          .onTouch((event: TouchEvent) => {
            this.handleTouch(event);
          })
          .onLoad(() => {
            this.onRenderLoad();
            this.updateTouchScale();
          })
          .onDestroy(() => {
            this.onRenderDestroy();
          });
      }
        .width('100%')
        .height('70%')
        .borderRadius(12)
        .clip(true)
        .onAreaChange((_oldValue, newValue) => {
          this.viewWidth = this.toNumber(newValue.width as number | string);
          this.viewHeight = this.toNumber(newValue.height as number | string);
          this.updateTouchScale();
        })
        .onTouch((event: TouchEvent) => {
          this.handleTouch(event);
        });
    }
    .width('100%')
    .height('100%')
    .backgroundColor('#0A0B1A')
    .padding({ left: 16, right: 16 });
  }
}
