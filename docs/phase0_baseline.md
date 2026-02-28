# Phase 0 Baseline (GLEX)

## Scope
- Capture current public API and runtime behavior before multi-instance refactor.
- Define regression cases and performance baselines.

## Native N-API Exports (current)
- addPass
- clearLastError
- destroySurface
- getCurrentFPS
- getGLInfo
- getLastError
- getPasses
- loadShaderFromRawfile
- removePass
- resize
- setBackgroundColor
- setPasses
- setShaderSources
- setSurfaceId
- setTargetFPS
- setTouchEvent
- setUniform
- startRender
- stopRender

## ArkTS GLEXComponent Params (current)
- targetFPS: number
- clearColor: number[]
- autoStart: boolean
- vertexShader: string
- fragmentShader: string
- uniforms: Record<string, number | number[]>
- builtinPass: 'demo' | 'attack' | 'none'

## ArkTS GLEXComponent Events (current)
- onReady
- onStopped
- onError
- onTouchEvent

## ArkTS GLEXComponent Methods (current)
- getCurrentFPS()
- getGLInfo()
- startRender()
- stopRender()
- setPasses(passes: string[])
- addPass(name: string)
- removePass(name: string)
- getPasses()
- loadShaderFromRawfile(resourceManager, vertexPath, fragmentPath)

## Pipeline Features (current)
- RenderPipeline and RenderPass lifecycle
- PassRegistry for builtin pass creation
- Touch dispatch via RenderPipeline::dispatchTouch

## Builtin Passes (current)
- DemoPass
- AttackPass
- ShaderPass

## Known Constraints (current)
- Bridge 已切换为 ObjectWrap 实例上下文，支持多实例并行渲染。
- Shader rawfile 加载仍以字符串/内存读取为主，非完整端到端零拷贝。

## Regression Checklist (must pass after refactor)
- DemoPass renders
- AttackPass renders and responds to touch
- ShaderPass loads via setShaderSources
- ShaderPass loads via loadShaderFromRawfile
- Pass switching (setPasses/add/remove) works during render loop
- Resize updates viewport and pipeline
- startRender/stopRender toggle loop safely
- getGLInfo/getCurrentFPS return valid values
- Error reporting via getLastError/clearLastError

## Baseline Metrics (to capture)
- App start to first frame (ms)
- FPS steady-state (60s window)
- Memory peak (RSS)
- Shader load time (ms)
- Pass switch time (ms)
