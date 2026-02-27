declare module 'libglex.so' {
  // ============================================================
  // 鏍稿績鐢熷懡鍛ㄦ湡
  // ============================================================

  /** 璁剧疆 Surface ID锛屽垵濮嬪寲 EGL 涓婁笅鏂?*/
  export function setSurfaceId(surfaceId: string | number | bigint): void;

  /** 閿€姣?Surface 鍜?EGL 涓婁笅鏂?*/
  export function destroySurface(): void;

  /** 鍚姩娓叉煋寰幆 */
  export function startRender(): void;

  /** 鍋滄娓叉煋寰幆 */
  export function stopRender(): void;

  /** 璋冩暣娓叉煋灏哄 */
  export function resize(width: number, height: number): void;

  // ============================================================
  // 娓叉煋閰嶇疆
  // ============================================================

  /** 璁剧疆娓呭睆棰滆壊锛堝搴?glClearColor锛?*/
  export function setBackgroundColor(r: number, g: number, b: number, a?: number): void;

  /** 璁剧疆鐩爣甯х巼 */
  export function setTargetFPS(fps: number): void;

  /** 璁剧疆鑷畾涔夌潃鑹插櫒婧愮爜锛堜粎鏀寔 ES 3.0+锛?*/
  export function setShaderSources(vertexShader: string, fragmentShader: string): void;

  /** 从 Rawfile 加载 Shader 源码 */
  export function loadShaderFromRawfile(resourceManager: object, vertexPath: string, fragmentPath: string): void;

  /** 璁剧疆鑷畾涔?Uniform锛坣umber 鎴?number[]锛?*/
  export function setUniform(name: string, value: number | number[]): void;

  /** 璁剧疆 Pass 鍒楄〃锛堟寜椤哄簭锛?*/
  export function setPasses(passes: string[]): void;

  /** 娣诲姞 Pass */
  export function addPass(name: string): void;

  /** 绉婚櫎 Pass */
  export function removePass(name: string): void;

  /** 鑾峰彇褰撳墠 Pass 鍒楄〃锛堥厤缃眰闈級 */
  export function getPasses(): string[];

  /** 浼犻€掕Е鎽镐簨浠讹紙鐢?ArkTS 渚ц皟鐢級 */
  export function setTouchEvent(x: number, y: number, action: number, pointerId?: number): void;

  // ============================================================
  // 鐘舵€佹煡璇?  // ============================================================

  /** 鑾峰彇褰撳墠瀹為檯甯х巼 */
  export function getCurrentFPS(): number;

  /** 鑾峰彇 GL 淇℃伅锛堢増鏈€佹覆鏌撳櫒銆佸昂瀵革級 */
  export function getGLInfo(): {
    version: string;
    renderer: string;
    width: number;
    height: number;
  };

  /** 鑾峰彇鏈€杩戜竴娆￠敊璇俊鎭紙绌哄瓧绗︿覆琛ㄧず鏃犻敊璇級 */
  export function getLastError(): string;

  /** 娓呴櫎鏈€杩戜竴娆￠敊璇俊鎭?*/
  export function clearLastError(): void;
}
