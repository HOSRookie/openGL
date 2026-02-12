/**
 * GLEXComponent - OpenGL ES 渲染组件
 *
 * 封装 XComponent + Native 渲染管线，提供开箱即用的 OpenGL ES 渲染能力。
 * 通过 libraryname 绑定 Native 模块，XComponent 回调自动管理 EGL 生命周期。
 *
 * 用法：
 *   GLEXComponent({
 *     targetFPS: 60,
 *   })
 *     .width('100%')
 *     .height(300)
 */
import glex from 'libglex.so';

/**
 * GL 信息接口
 */
export interface GLInfo {
  version: string;
  renderer: string;
  width: number;
  height: number;
}

@ComponentV2
export struct GLEXComponent {
  /** 目标帧率（默认 60） */
  @Param targetFPS: number = 60;

  /** 清屏颜色 [r, g, b, a]（0.0 ~ 1.0），对应 glClearColor */
  @Param clearColor: number[] = [0.02, 0.03, 0.10, 1.0];

  /** 是否自动启动渲染（默认 true） */
  @Param autoStart: boolean = true;

  /** Surface 就绪回调 */
  @Event onReady: () => void = () => {};

  /** 渲染停止回调 */
  @Event onStopped: () => void = () => {};

  private xComponentId: string = 'glex_' + Date.now();
  private initialized: boolean = false;

  @Monitor('targetFPS')
  onTargetFPSChange(): void {
    if (this.initialized) {
      glex.setTargetFPS(this.targetFPS);
    }
  }

  @Monitor('clearColor')
  onClearColorChange(): void {
    if (this.initialized && this.clearColor.length >= 3) {
      const a = this.clearColor.length >= 4 ? this.clearColor[3] : 1.0;
      glex.setBackgroundColor(
        this.clearColor[0],
        this.clearColor[1],
        this.clearColor[2],
        a
      );
    }
  }

  aboutToDisappear(): void {
    // Native 层的 OnSurfaceDestroyed 回调会自动处理清理
    this.initialized = false;
  }

  /**
   * 获取当前帧率
   */
  public getCurrentFPS(): number {
    try {
      return glex.getCurrentFPS() as number;
    } catch {
      return 0;
    }
  }

  /**
   * 获取 GL 信息
   */
  public getGLInfo(): GLInfo {
    try {
      return glex.getGLInfo() as GLInfo;
    } catch {
      return { version: 'unknown', renderer: 'unknown', width: 0, height: 0 };
    }
  }

  /**
   * 手动启动渲染
   */
  public startRender(): void {
    glex.startRender();
  }

  /**
   * 手动停止渲染
   */
  public stopRender(): void {
    glex.stopRender();
    this.onStopped();
  }

  /**
   * XComponent 加载完成
   * 此时 Native 层的 OnSurfaceCreated 已经触发，GL 上下文已就绪
   */
  private onXComponentLoad(): void {
    console.info('[GLEXComponent] XComponent loaded');

    try {
      // 设置渲染参数（此时 GL 上下文已由 XComponent 回调初始化）
      glex.setTargetFPS(this.targetFPS);

      if (this.clearColor.length >= 3) {
        const a = this.clearColor.length >= 4 ? this.clearColor[3] : 1.0;
        glex.setBackgroundColor(
          this.clearColor[0],
          this.clearColor[1],
          this.clearColor[2],
          a
        );
      }

      this.initialized = true;

      // 启动渲染循环
      if (this.autoStart) {
        glex.startRender();
      }

      this.onReady();
      console.info('[GLEXComponent] Initialized successfully');
    } catch (e) {
      console.error(`[GLEXComponent] Init error: ${e}`);
    }
  }

  private onXComponentDestroy(): void {
    console.info('[GLEXComponent] XComponent destroyed');
    // Native 层的 OnSurfaceDestroyed 回调会自动处理
    this.initialized = false;
  }

  build() {
    Stack() {
      XComponent({
        id: this.xComponentId,
        type: XComponentType.SURFACE,
        libraryname: 'glex',
      })
        .width('100%')
        .height('100%')
        .onLoad(() => {
          this.onXComponentLoad();
        })
        .onDestroy(() => {
          this.onXComponentDestroy();
        })
    }
    .width('100%')
    .height('100%')
  }
}
