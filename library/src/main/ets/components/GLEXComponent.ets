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

interface GlexNative {
  setTargetFPS(fps: number): void;
  setBackgroundColor(r: number, g: number, b: number, a?: number): void;
  startRender(): void;
  stopRender(): void;
  getCurrentFPS(): number;
  getGLInfo(): GLInfo;
  setShaderSources(vertexShader: string, fragmentShader: string): void;
  loadShaderFromRawfile(resourceManager: ResourceManagerHandle, vertexPath: string, fragmentPath: string): void;
  setUniform(name: string, value: number | number[]): void;
  setPasses(passes: string[]): void;
  addPass(name: string): void;
  removePass(name: string): void;
  getPasses(): string[];
  setTouchEvent(x: number, y: number, action: number, pointerId?: number): void;
  getLastError(): string;
  clearLastError(): void;
}

const glexApi: GlexNative = glex as GlexNative;

/**
 * GL 信息接口
 */
export interface GLInfo {
  version: string;
  renderer: string;
  width: number;
  height: number;
}

export type BuiltinPass = 'demo' | 'attack' | 'none';

export interface ResourceManagerHandle {}

@ComponentV2
export struct GLEXComponent {
  /** 目标帧率（默认 60） */
  @Param targetFPS: number = 60;

  /** 清屏颜色 [r, g, b, a]（0.0 ~ 1.0），对应 glClearColor */
  @Param clearColor: number[] = [0.02, 0.03, 0.10, 1.0];

  /** 是否自动启动渲染（默认 true） */
  @Param autoStart: boolean = true;

  /** 自定义顶点着色器源码（ES 3.0+） */
  @Param vertexShader: string = '';

  /** 自定义片元着色器源码（ES 3.0+） */
  @Param fragmentShader: string = '';

  /** 自定义 Uniform（number 或 number[]） */
  @Param uniforms: Record<string, number | number[]> = {};

  @Param builtinPass: BuiltinPass = 'demo';

  /** Surface 就绪回调 */
  @Event onReady: () => void = () => {};

  /** 渲染停止回调 */
  @Event onStopped: () => void = () => {};

  /** 错误回调 */
  @Event onError: (message: string) => void = (_: string) => {};

  /** 触摸事件回调（x, y, action） */
  @Event onTouchEvent: (x: number, y: number, action: number) => void = (_x: number, _y: number, _a: number) => {};

  private xComponentId: string = 'glex_' + Date.now();
  private initialized: boolean = false;
  private viewWidth: number = 0;
  private viewHeight: number = 0;
  private touchScaleX: number = 1.0;
  private touchScaleY: number = 1.0;

  @Monitor('targetFPS')
  onTargetFPSChange(): void {
    if (this.initialized) {
      glexApi.setTargetFPS(this.targetFPS);
    }
  }

  @Monitor('clearColor')
  onClearColorChange(): void {
    if (this.initialized && this.clearColor.length >= 3) {
      const a = this.clearColor.length >= 4 ? this.clearColor[3] : 1.0;
      glexApi.setBackgroundColor(
        this.clearColor[0],
        this.clearColor[1],
        this.clearColor[2],
        a
      );
    }
  }

  @Monitor('builtinPass')
  onBuiltinPassChange(): void {
    this.applyBuiltinPass();
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
      return glexApi.getCurrentFPS() as number;
    } catch {
      return 0;
    }
  }

  /**
   * 获取 GL 信息
   */
  public getGLInfo(): GLInfo {
    try {
      return glexApi.getGLInfo() as GLInfo;
    } catch {
      return { version: 'unknown', renderer: 'unknown', width: 0, height: 0 };
    }
  }

  /**
   * 手动启动渲染
   */
  public startRender(): void {
    glexApi.startRender();
  }

  /**
   * 手动停止渲染
   */
  public stopRender(): void {
    glexApi.stopRender();
    this.onStopped();
  }

  /**
   * 从 Rawfile 加载 Shader
   */
  public loadShaderFromRawfile(resourceManager: ResourceManagerHandle, vertexPath: string, fragmentPath: string): void {
    try {
      glexApi.loadShaderFromRawfile(resourceManager, vertexPath, fragmentPath);
      this.reportLastError();
    } catch {
      this.onError('GLEX loadShaderFromRawfile failed');
    }
  }

  public setPasses(passes: string[]): void {
    try {
      glexApi.setPasses(passes);
    } catch {
      // ignore
    }
  }

  public addPass(name: string): void {
    try {
      glexApi.addPass(name);
    } catch {
      // ignore
    }
  }

  public removePass(name: string): void {
    try {
      glexApi.removePass(name);
    } catch {
      // ignore
    }
  }

  public getPasses(): string[] {
    try {
      return glexApi.getPasses();
    } catch {
      return [];
    }
  }

  private applyUniforms(): void {
    const keys: string[] = Object.keys(this.uniforms);
    if (keys.length === 0) {
      return;
    }
    try {
      for (let i = 0; i < keys.length; i++) {
        const key: string = keys[i];
        const value: number | number[] = this.uniforms[key];
        glexApi.setUniform(key, value);
      }
      this.reportLastError();
    } catch {
      this.onError('GLEX setUniform failed');
    }
  }

  private reportLastError(): void {
    try {
      const msg: string = glexApi.getLastError();
      if (msg && msg.length > 0) {
        this.onError(msg);
        glexApi.clearLastError();
      }
    } catch {
      // ignore
    }
  }

  @Monitor('vertexShader')
  onVertexShaderChange(): void {
    this.tryUpdateShader();
  }

  @Monitor('fragmentShader')
  onFragmentShaderChange(): void {
    this.tryUpdateShader();
  }

  @Monitor('uniforms')
  onUniformsChange(): void {
    if (this.initialized) {
      this.applyUniforms();
    }
  }

  private tryUpdateShader(): void {
    if (this.initialized && this.vertexShader && this.fragmentShader) {
      try {
        glexApi.setShaderSources(this.vertexShader, this.fragmentShader);
        this.reportLastError();
      } catch {
        this.onError('GLEX setShaderSources failed');
      }
    }
  }

  private applyBuiltinPass(): void {
    const passList: string[] = [];
    if (this.builtinPass === 'demo') {
      passList.push('DemoPass');
    } else if (this.builtinPass === 'attack') {
      passList.push('AttackPass');
    }
    try {
      glexApi.setPasses(passList);
    } catch {
      // ignore
    }
  }

  /**
   * XComponent 加载完成
   * 此时 Native 层的 OnSurfaceCreated 已经触发，GL 上下文已就绪
   */
  private onXComponentLoad(): void {
    console.info('[GLEXComponent] XComponent loaded');

    try {
      this.applyBuiltinPass();
      if (this.vertexShader && this.fragmentShader) {
        glexApi.setShaderSources(this.vertexShader, this.fragmentShader);
      }

      // 设置渲染参数（此时 GL 上下文已由 XComponent 回调初始化）
      glexApi.setTargetFPS(this.targetFPS);

      if (this.clearColor.length >= 3) {
        const a = this.clearColor.length >= 4 ? this.clearColor[3] : 1.0;
        glexApi.setBackgroundColor(
          this.clearColor[0],
          this.clearColor[1],
          this.clearColor[2],
          a
        );
      }

      this.initialized = true;

      // 启动渲染循环
      if (this.autoStart) {
        glexApi.startRender();
      }

      this.applyUniforms();
      this.reportLastError();
      this.onReady();
      console.info('[GLEXComponent] Initialized successfully');
    } catch {
      console.error('[GLEXComponent] Init error');
      this.onError('GLEX init failed');
    }
  }

  private onXComponentDestroy(): void {
    console.info('[GLEXComponent] XComponent destroyed');
    // Native 层的 OnSurfaceDestroyed 回调会自动处理
    this.initialized = false;
  }

  private handleTouch(event: TouchEvent): void {
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
    this.onTouchEvent(clampedX, clampedY, action);
    try {
      glexApi.setTouchEvent(clampedX, clampedY, action, touch.id);
    } catch {
      // ignore
    }
  }

  private updateTouchScale(): void {
    if (!this.initialized || this.viewWidth <= 0 || this.viewHeight <= 0) {
      return;
    }
    try {
      const info = glexApi.getGLInfo();
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
    const parsed = Number.parseFloat(value);
    return Number.isFinite(parsed) ? parsed : 0;
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
          this.updateTouchScale();
        })
        .onDestroy(() => {
          this.onXComponentDestroy();
        })
    }
    .width('100%')
    .height('100%')
    .onAreaChange((_oldValue, newValue) => {
      this.viewWidth = this.toNumber(newValue.width as number | string);
      this.viewHeight = this.toNumber(newValue.height as number | string);
      this.updateTouchScale();
    })
    .onTouch((event) => {
      this.handleTouch(event);
    })
  }
}
