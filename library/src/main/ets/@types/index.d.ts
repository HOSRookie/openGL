declare module '@yunshen1933/ohos_glex' {
  export type BuiltinPass = 'demo' | 'attack' | 'none';

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

  export interface ResourceManagerHandle {}

  export interface GLEXComponentParams {
    targetFPS?: number;
    clearColor?: number[];
    autoStart?: boolean;
    vertexShader?: string;
    fragmentShader?: string;
    uniforms?: Record<string, number | number[]>;
    builtinPass?: BuiltinPass;
    onReady?: () => void;
    onStopped?: () => void;
    onError?: (message: string) => void;
    onTouchEvent?: (x: number, y: number, action: number) => void;
  }

  export interface GLEXComponentInterface {
    (params?: GLEXComponentParams): GLEXComponentAttribute;
  }

  export interface GLEXComponentAttribute {
    width(value: string | number): GLEXComponentAttribute;
    height(value: string | number): GLEXComponentAttribute;
    borderRadius(value: string | number): GLEXComponentAttribute;
    clip(value: boolean): GLEXComponentAttribute;
    onLoad(callback: () => void): GLEXComponentAttribute;
    onDestroy(callback: () => void): GLEXComponentAttribute;
  }

  export const GLEXComponent: GLEXComponentInterface;

  export interface GlexNativeInstance {
    bindXComponent(id: string): void;
    unbindXComponent(): void;
    setSurfaceId(surfaceId: string | number | bigint): void;
    setTargetFPS(fps: number): void;
    resize(width: number, height: number): void;
    setBackgroundColor(r: number, g: number, b: number, a?: number): void;
    startRender(): void;
    stopRender(): void;
    destroySurface(): void;
    setShaderSources(vertexShader: string, fragmentShader: string): void;
    loadShaderFromRawfile(resourceManager: ResourceManagerHandle, vertexPath: string, fragmentPath: string): void;
    loadRawfileBytes(resourceManager: ResourceManagerHandle, path: string): ArrayBuffer;
    setUniform(name: string, value: number | number[]): void;
    setPasses(passes: string[]): void;
    addPass(name: string): void;
    removePass(name: string): void;
    getPasses(): string[];
    setTouchEvent(x: number, y: number, action: number, pointerId?: number): void;
    getCurrentFPS(): number;
    getGLInfo(): GLInfo;
    getGpuStats(): GpuStats;
    getLastError(): string;
    clearLastError(): void;
  }

  export function createGlexRenderer(): GlexNativeInstance;
}
