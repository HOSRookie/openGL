declare module 'libglex.so' {
  // ============================================================
  // 核心生命周期
  // ============================================================

  /** 设置 Surface ID，初始化 EGL 上下文 */
  export function setSurfaceId(surfaceId: string | number | bigint): void;

  /** 销毁 Surface 和 EGL 上下文 */
  export function destroySurface(): void;

  /** 启动渲染循环 */
  export function startRender(): void;

  /** 停止渲染循环 */
  export function stopRender(): void;

  /** 调整渲染尺寸 */
  export function resize(width: number, height: number): void;

  // ============================================================
  // 渲染配置
  // ============================================================

  /** 设置清屏颜色（对应 glClearColor） */
  export function setBackgroundColor(r: number, g: number, b: number, a?: number): void;

  /** 设置目标帧率 */
  export function setTargetFPS(fps: number): void;

  // ============================================================
  // 状态查询
  // ============================================================

  /** 获取当前实际帧率 */
  export function getCurrentFPS(): number;

  /** 获取 GL 信息（版本、渲染器、尺寸） */
  export function getGLInfo(): {
    version: string;
    renderer: string;
    width: number;
    height: number;
  };

  /** 获取最近一次错误信息（空字符串表示无错误） */
  export function getLastError(): string;

  /** 清除最近一次错误信息 */
  export function clearLastError(): void;
}
