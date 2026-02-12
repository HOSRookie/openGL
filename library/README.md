# GLEX - OpenGL ES Rendering Framework

**GLEX** (GL Extension) 是一个轻量级的 OpenGL ES 渲染框架，专为 OpenHarmony / HarmonyOS 设计。

它封装了 EGL 上下文管理、着色器编译、多 Pass 渲染管线、独立渲染线程和 XComponent 集成，让开发者可以专注于渲染逻辑本身，无需关心底层 EGL 和 NAPI 的繁琐配置。

## 效果预览

![GLEX 星空演示效果](screenshots/demo.png)

内置演示效果：渐变深空背景 + 200 颗闪烁星星 + 流星拖尾动画，全部由 OpenGL ES 3.0 Shader 实时渲染。

## 特性

- **EGL 上下文管理** — 自动初始化，支持 ES 3.2 / 3.0 / 2.0 逐级回退
- **Shader 工具** — 着色器编译、链接、Uniform 缓存，便捷的 set 方法
- **渲染管线** — RenderPass 抽象基类 + RenderPipeline 多阶段编排
- **渲染线程** — 独立线程运行，帧率控制，实时 FPS 统计
- **ArkTS 组件** — `GLEXComponent` 开箱即用，声明式 API
- **XComponent 集成** — Surface 生命周期全自动管理
- **内置演示** — 星空 + 流星动画效果，展示框架用法

## 下载安装

```bash
ohpm install @yunshen1933/ohos_glex
```

## 使用说明

### 基本用法

在你的页面中直接使用 `GLEXComponent`：

```typescript
import { GLEXComponent } from '@yunshen1933/ohos_glex';

@Entry
@Component
struct MyPage {
  build() {
    Column() {
      GLEXComponent({
        targetFPS: 60,
        clearColor: [0.02, 0.03, 0.10, 1.0],
        autoStart: true,
      })
        .width('100%')
        .height(400)
    }
  }
}
```

运行后即可看到内置的星空演示效果。

### 获取运行时信息

```typescript
import { GLEXComponent, GLInfo } from '@yunshen1933/ohos_glex';

@Entry
@Component
struct MyPage {
  private glexRef: GLEXComponent | null = null;

  build() {
    Column() {
      GLEXComponent({
        targetFPS: 60,
        autoStart: true,
        onReady: () => {
          // Surface 就绪后可获取 GL 信息
          console.info('FPS: ' + this.glexRef?.getCurrentFPS());
          const info: GLInfo | undefined = this.glexRef?.getGLInfo();
          console.info('GL Version: ' + info?.version);
          console.info('GPU: ' + info?.renderer);
        }
      })
        .width('100%')
        .height(400)
    }
  }
}
```

### 自定义渲染（C++ 扩展）

GLEX 框架的 C++ 层提供了清晰的分层架构，你可以基于源码扩展自己的渲染逻辑：

```
include/glex/
├── GLContext.h        ← EGL 上下文管理
├── ShaderProgram.h    ← 着色器编译 & Uniform 管理
├── RenderPass.h       ← 渲染阶段抽象基类
├── RenderPipeline.h   ← 多阶段渲染管线
├── RenderThread.h     ← 独立渲染线程
├── Log.h              ← 日志工具
└── GLEX.h             ← 主头文件（包含以上所有）
```

**创建自定义 RenderPass：**

```cpp
#include "glex/GLEX.h"

class MyCustomPass : public glex::RenderPass {
public:
    MyCustomPass() : RenderPass("MyCustomPass") {}

protected:
    void onInitialize(int width, int height) override {
        // 初始化 GL 资源（着色器、VAO、VBO 等）
        shader_.build(vertexSrc, fragmentSrc);
    }

    void onUpdate(float deltaTime) override {
        // 每帧更新逻辑
        time_ += deltaTime;
    }

    void onRender() override {
        // 渲染绘制
        shader_.use();
        shader_.setUniform1f("u_time", time_);
        // ... glDraw* 调用 ...
    }

    void onDestroy() override {
        shader_.destroy();
    }

private:
    glex::ShaderProgram shader_;
    float time_ = 0.0f;
};
```

## API 参考

### GLEXComponent (ArkTS)

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| targetFPS | number | 60 | 目标帧率 |
| clearColor | number[] | [0.02, 0.03, 0.10, 1.0] | 清屏颜色 RGBA（对应 glClearColor） |
| autoStart | boolean | true | 是否自动启动渲染 |

| 事件 | 说明 |
|------|------|
| onReady | Surface 就绪并开始渲染时触发 |
| onStopped | 渲染停止时触发 |

| 方法 | 返回值 | 说明 |
|------|--------|------|
| getCurrentFPS() | number | 获取当前实际帧率 |
| getGLInfo() | GLInfo | 获取 GL 版本、GPU 名称、Surface 尺寸 |
| startRender() | void | 手动启动渲染 |
| stopRender() | void | 手动停止渲染 |

### GLContext (C++)

EGL 上下文管理器，支持配置化初始化。

```cpp
glex::GLContext ctx;
glex::GLContextConfig config;
config.depthSize = 24;       // 深度缓冲位数
config.stencilSize = 8;      // 模板缓冲位数
config.vsyncEnabled = true;  // 垂直同步

ctx.initialize(nativeWindow, config);
ctx.makeCurrent();
// ... 渲染操作 ...
ctx.swapBuffers();
ctx.destroy();
```

### ShaderProgram (C++)

着色器程序，自动缓存 Uniform Location。

```cpp
glex::ShaderProgram shader;
shader.build(vertSrc, fragSrc);
shader.use();
shader.setUniform1f("u_time", 1.0f);
shader.setUniform3f("u_color", 1.0f, 0.5f, 0.0f);
shader.setUniformMatrix4fv("u_mvp", mvpMatrix);
```

### RenderPipeline (C++)

多阶段渲染管线，按顺序执行所有 Pass。

```cpp
glex::RenderPipeline pipeline;
pipeline.addPass(std::make_shared<BackgroundPass>());
pipeline.addPass(std::make_shared<ParticlePass>());
pipeline.addPass(std::make_shared<PostProcessPass>());
pipeline.initialize(width, height);

// 每帧调用
pipeline.update(deltaTime);
pipeline.render();
```

## 架构

```
┌─────────────────────────────┐
│     ArkTS (GLEXComponent)   │  ← 开发者使用的 UI 组件
├─────────────────────────────┤
│     NAPI Bridge             │  ← JS ↔ Native 桥接
├─────────────────────────────┤
│     RenderThread            │  ← 独立渲染线程（帧率控制）
├─────────────────────────────┤
│     RenderPipeline          │  ← 渲染管线编排
│  ┌────────┬────────┬──────┐ │
│  │ Pass 1 │ Pass 2 │ ... │ │  ← 可扩展的渲染阶段
│  └────────┴────────┴──────┘ │
├─────────────────────────────┤
│  GLContext │ ShaderProgram   │  ← 核心 GL 工具类
├─────────────────────────────┤
│     EGL / OpenGL ES 3.x     │  ← 系统 API
└─────────────────────────────┘
```

## 约束与限制

- **开发环境**：DevEco Studio 5.0.0 或以上版本
- **HarmonyOS SDK**：API 12 (6.0.2) 或以上
- **语言版本**：ArkTS，C++ 17
- **设备要求**：支持 OpenGL ES 3.0 或以上的设备
- **支持设备类型**：phone、tablet、2in1
- **ABI 架构**：arm64-v8a、x86_64
- 本库使用 XComponent SURFACE 模式进行渲染，不支持 TEXTURE 模式
- 渲染线程为独立线程，请勿在 ArkTS 主线程中直接调用 GL 函数

## 目录结构

```
library/
├── Index.ets                      ← 库导出入口
├── oh-package.json5               ← 包元数据
├── README.md                      ← 使用文档
├── CHANGELOG.md                   ← 版本记录
├── LICENSE                        ← Apache 2.0 许可证
└── src/main/
    ├── ets/
    │   ├── components/
    │   │   └── GLEXComponent.ets  ← ArkTS 渲染组件
    │   └── @types/
    │       └── libglex.so.d.ts    ← Native 类型声明
    └── cpp/
        ├── CMakeLists.txt         ← CMake 构建脚本
        ├── include/glex/          ← C++ 头文件
        │   ├── GLEX.h             ← 主头文件
        │   ├── GLContext.h        ← EGL 管理
        │   ├── ShaderProgram.h    ← Shader 工具
        │   ├── RenderPass.h       ← 渲染阶段抽象
        │   ├── RenderPipeline.h   ← 渲染管线
        │   ├── RenderThread.h     ← 渲染线程
        │   └── Log.h              ← 日志工具
        └── src/
            ├── glex/              ← 核心实现
            └── bridge/            ← NAPI 桥接 + 演示渲染器
```

## 许可证

本项目基于 [Apache License 2.0](LICENSE) 开源。

## 作者

**云深**
