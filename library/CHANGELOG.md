# 更新日志

## [1.0.0] - 2026-02-06

### 新增
- **GLContext**: EGL 上下文管理，支持 ES 3.2/3.0/2.0 逐级回退
- **ShaderProgram**: 着色器编译与链接，Uniform 缓存管理
- **RenderPass**: 渲染阶段抽象基类
- **RenderPipeline**: 多阶段渲染管线编排
- **RenderThread**: 独立渲染线程，帧率控制与 FPS 统计
- **GLEXComponent**: ArkTS 封装组件，开箱即用
- **DemoPass**: 内置演示渲染器（星空 + 流星效果）
- **NAPI Bridge**: XComponent 生命周期管理与 ArkTS 桥接
