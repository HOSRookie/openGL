declare module 'libglex.so' {
  // ============================================================
  // 核心实例
  // ============================================================

  export class GLEXEngine {
    /** 绑定 XComponent 的 id（ArkTS 侧传入 XComponent id） */
    bindXComponent(id: string): void;

    /** 解除绑定 */
    unbindXComponent(): void;

    /** 设置 Surface ID，初始化 EGL 上下文 */
    setSurfaceId(surfaceId: string | number | bigint): void;

    /** 销毁 Surface 和 EGL 上下文 */
    destroySurface(): void;

    /** 启动渲染循环 */
    startRender(): void;

    /** 停止渲染循环 */
    stopRender(): void;

    /** 调整渲染尺寸 */
    resize(width: number, height: number): void;

    // ============================================================
    // 渲染配置
    // ============================================================

    /** 设置清屏颜色（对应 glClearColor） */
    setBackgroundColor(r: number, g: number, b: number, a?: number): void;

    /** 设置目标帧率 */
    setTargetFPS(fps: number): void;

    /** 设置自定义着色器源码（仅支持 ES 3.0+） */
    setShaderSources(vertexShader: string, fragmentShader: string): void;

    /** 从 Rawfile 加载 Shader 源码 */
    loadShaderFromRawfile(resourceManager: object, vertexPath: string, fragmentPath: string): void;

    /** 从 Rawfile 加载二进制数据（优先 mmap 零拷贝） */
    loadRawfileBytes(resourceManager: object, path: string): ArrayBuffer;

    /** 设置自定义 Uniform（number 或 number[]） */
    setUniform(name: string, value: number | number[]): void;

    /** 设置 Pass 列表（按顺序） */
    setPasses(passes: string[]): void;

    /** 添加 Pass */
    addPass(name: string): void;

    /** 移除 Pass */
    removePass(name: string): void;

    /** 获取当前 Pass 列表（配置层面） */
    getPasses(): string[];

    /** 传递触摸事件（由 ArkTS 调用） */
    setTouchEvent(x: number, y: number, action: number, pointerId?: number): void;

    // ============================================================
    // 状态查询
    // ============================================================

    /** 获取当前实际帧率 */
    getCurrentFPS(): number;

    /** 获取 GL 信息（版本、渲染器、尺寸） */
    getGLInfo(): {
      version: string;
      renderer: string;
      width: number;
      height: number;
    };

    /** 获取 GPU 资源统计（program/shader/buffer/vao/texture） */
    getGpuStats(): {
      programs: number;
      shaders: number;
      buffers: number;
      vaos: number;
      textures: number;
    };

    /** 获取最近一次错误信息（空字符串表示无错误） */
    getLastError(): string;

    /** 清除最近一次错误信息 */
    clearLastError(): void;
  }

  /** 创建渲染实例 */
  export function createRenderer(): GLEXEngine;
}
