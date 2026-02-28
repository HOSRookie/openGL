import glex from 'libglex.so';

interface GlexNativeInstance {
  bindXComponent(id: string): void;
  unbindXComponent(): void;
  setTargetFPS(fps: number): void;
  setBackgroundColor(r: number, g: number, b: number, a?: number): void;
  startRender(): void;
  stopRender(): void;
  destroySurface(): void;
  getCurrentFPS(): number;
  getGLInfo(): GLInfo;
  getGpuStats(): GpuStats;
  setShaderSources(vertexShader: string, fragmentShader: string): void;
  loadShaderFromRawfile(resourceManager: ResourceManagerHandle, vertexPath: string, fragmentPath: string): void;
  loadRawfileBytes(resourceManager: ResourceManagerHandle, path: string): ArrayBuffer;
  setUniform(name: string, value: number | number[]): void;
  setPasses(passes: string[]): void;
  addPass(name: string): void;
  removePass(name: string): void;
  getPasses(): string[];
  setTouchEvent(x: number, y: number, action: number, pointerId?: number): void;
  getLastError(): string;
  clearLastError(): void;
}

interface GlexNativeModule {
  createRenderer(): GlexNativeInstance;
}

const glexModule: GlexNativeModule = glex as GlexNativeModule;

export interface GLInfo {
  version: string;
  renderer: string;
  width: number;
  height: number;
}

export interface GpuStats {
  programs: number;
  shaders: number;
  buffers: number;
  vaos: number;
  textures: number;
}

export type BuiltinPass = 'demo' | 'attack' | 'none';

export interface ResourceManagerHandle {}

@ComponentV2
export struct GLEXComponent {
  @Param targetFPS: number = 60;
  @Param clearColor: number[] = [0.02, 0.03, 0.10, 1.0];
  @Param autoStart: boolean = true;
  @Param vertexShader: string = '';
  @Param fragmentShader: string = '';
  @Param uniforms: Record<string, number | number[]> = {};
  @Param builtinPass: BuiltinPass = 'demo';

  @Event onReady: () => void = () => {};
  @Event onStopped: () => void = () => {};
  @Event onError: (message: string) => void = (_: string) => {};
  @Event onTouchEvent: (x: number, y: number, action: number) => void = (_x: number, _y: number, _a: number) => {};

  private xComponentId: string = 'glex_' + Date.now();
  private native: GlexNativeInstance = glexModule.createRenderer();
  private initialized: boolean = false;
  private viewWidth: number = 0;
  private viewHeight: number = 0;
  private touchScaleX: number = 1.0;
  private touchScaleY: number = 1.0;

  @Monitor('targetFPS')
  onTargetFPSChange(): void {
    if (this.initialized) {
      this.native.setTargetFPS(this.targetFPS);
    }
  }

  @Monitor('clearColor')
  onClearColorChange(): void {
    if (this.initialized && this.clearColor.length >= 3) {
      const a = this.clearColor.length >= 4 ? this.clearColor[3] : 1.0;
      this.native.setBackgroundColor(
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
    this.initialized = false;
  }

  public getCurrentFPS(): number {
    try {
      return this.native.getCurrentFPS() as number;
    } catch {
      return 0;
    }
  }

  public getGLInfo(): GLInfo {
    try {
      return this.native.getGLInfo() as GLInfo;
    } catch {
      return { version: 'unknown', renderer: 'unknown', width: 0, height: 0 };
    }
  }

  public getGpuStats(): GpuStats {
    try {
      return this.native.getGpuStats() as GpuStats;
    } catch {
      return { programs: 0, shaders: 0, buffers: 0, vaos: 0, textures: 0 };
    }
  }

  public startRender(): void {
    this.native.startRender();
  }

  public stopRender(): void {
    this.native.stopRender();
    this.onStopped();
  }

  public loadShaderFromRawfile(resourceManager: ResourceManagerHandle, vertexPath: string, fragmentPath: string): void {
    try {
      this.native.loadShaderFromRawfile(resourceManager, vertexPath, fragmentPath);
      this.reportLastError();
    } catch {
      this.onError('GLEX loadShaderFromRawfile failed');
    }
  }

  public loadRawfileBytes(resourceManager: ResourceManagerHandle, path: string): ArrayBuffer {
    try {
      return this.native.loadRawfileBytes(resourceManager, path);
    } catch {
      this.onError('GLEX loadRawfileBytes failed');
      return new ArrayBuffer(0);
    }
  }

  public setPasses(passes: string[]): void {
    try {
      this.native.setPasses(passes);
    } catch {
      // ignore
    }
  }

  public addPass(name: string): void {
    try {
      this.native.addPass(name);
    } catch {
      // ignore
    }
  }

  public removePass(name: string): void {
    try {
      this.native.removePass(name);
    } catch {
      // ignore
    }
  }

  public getPasses(): string[] {
    try {
      return this.native.getPasses();
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
        this.native.setUniform(key, value);
      }
      this.reportLastError();
    } catch {
      this.onError('GLEX setUniform failed');
    }
  }

  private reportLastError(): void {
    try {
      const msg: string = this.native.getLastError();
      if (msg && msg.length > 0) {
        this.onError(msg);
        this.native.clearLastError();
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
        this.native.setShaderSources(this.vertexShader, this.fragmentShader);
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
      this.native.setPasses(passList);
    } catch {
      // ignore
    }
  }

  private onXComponentLoad(): void {
    try {
      this.native.bindXComponent(this.xComponentId);
      this.applyBuiltinPass();
      if (this.vertexShader && this.fragmentShader) {
        this.native.setShaderSources(this.vertexShader, this.fragmentShader);
      }

      this.native.setTargetFPS(this.targetFPS);

      if (this.clearColor.length >= 3) {
        const a = this.clearColor.length >= 4 ? this.clearColor[3] : 1.0;
        this.native.setBackgroundColor(
          this.clearColor[0],
          this.clearColor[1],
          this.clearColor[2],
          a
        );
      }

      this.initialized = true;

      if (this.autoStart) {
        this.native.startRender();
      }

      this.applyUniforms();
      this.reportLastError();
      this.onReady();
    } catch {
      this.onError('GLEX init failed');
    }
  }

  private onXComponentDestroy(): void {
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
      this.native.setTouchEvent(clampedX, clampedY, action, touch.id);
    } catch {
      // ignore
    }
  }

  private updateTouchScale(): void {
    if (!this.initialized || this.viewWidth <= 0 || this.viewHeight <= 0) {
      return;
    }
    try {
      const info = this.native.getGLInfo();
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
