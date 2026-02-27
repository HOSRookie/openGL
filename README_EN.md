# GLEX - The First Open-Source OpenGL ES Rendering Framework for HarmonyOS

<div align="center">

**🎯 The First Open-Source Native OpenGL Rendering Framework in HarmonyOS Ecosystem**

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE)
[![HarmonyOS](https://img.shields.io/badge/HarmonyOS-API%2012+-orange.svg)](https://developer.huawei.com/consumer/cn/harmonyos/)
[![OpenGL ES](https://img.shields.io/badge/OpenGL%20ES-3.0%2B-green.svg)](https://www.khronos.org/opengles/)

English | [简体中文](README.md)

</div>

## Why GLEX?

HarmonyOS developers face significant challenges when working with OpenGL:

- ❌ **Complex EGL Initialization** — Hundreds of lines of boilerplate code for EGL config, context creation, and surface binding
- ❌ **Difficult XComponent Integration** — NAPI bridging, lifecycle management, thread synchronization with steep learning curve
- ❌ **No Rendering Abstractions** — Every project reinvents the wheel, lacking unified shader management and render pipeline
- ❌ **No Reference Implementation** — Community lacks complete, production-ready OpenGL frameworks

**GLEX is the first open-source framework in HarmonyOS ecosystem that solves these problems.**

## Core Value

### 🚀 Ready-to-Use ArkTS Component

```typescript
import { GLEXComponent } from '@yunshen1933/ohos_glex';

@Entry
@Component
struct MyPage {
  build() {
    GLEXComponent({ targetFPS: 60 })
      .width('100%')
      .height(400)
  }
}
```

**3 lines of code to start 60 FPS OpenGL rendering.** No need to understand EGL, NAPI, or XComponent internals.

### 🏗️ Production-Grade Architecture

GLEX is not a simple demo, but a carefully designed production-ready framework:

- **Clear Layering** — EGL Management → Shader Tools → Render Pipeline → ArkTS Component, each layer with distinct responsibilities
- **Thread Safety** — Independent render thread with automatic GL context switching and synchronization
- **Extensibility** — RenderPass abstract base class supporting multi-stage render pipeline orchestration
- **Fault Tolerance** — Automatic fallback from ES 3.2 → 3.0 → 2.0, compatible with different device capabilities

### 💎 Complete C++ Implementation

```cpp
class MyCustomPass : public glex::RenderPass {
protected:
    void onInitialize(int width, int height) override {
        shader_.build(vertexSrc, fragmentSrc);
    }

    void onRender() override {
        shader_.use();
        shader_.setUniform1f("u_time", time_);
        // Your rendering logic
    }
};
```

Provides complete C++ source code including:
- `GLContext` — EGL context management
- `ShaderProgram` — Shader compilation and uniform caching
- `RenderPass` / `RenderPipeline` — Render pipeline abstraction
- `RenderThread` — Independent render thread with frame rate control

### 🎨 Built-in Demo Effect

![GLEX Starfield Demo](library/screenshots/demo.png)

**200 twinkling stars + meteor trail animation**, all rendered in real-time with OpenGL ES 3.0 shaders, showcasing the framework's capabilities.

## Technical Highlights

| Feature | Description |
|---------|-------------|
| **Zero Dependencies** | Only depends on HarmonyOS system libraries, no third-party dependencies |
| **High Performance** | Independent render thread, stable 60 FPS, real-time FPS statistics |
| **Automation** | Fully automatic XComponent lifecycle management, no manual handling required |
| **Type Safety** | Complete TypeScript type declarations |
| **Modern C++** | C++17 standard, RAII resource management, smart pointers |
| **Cross-Architecture** | Supports arm64-v8a and x86_64 |

## Quick Start

### Installation

```bash
ohpm install @yunshen1933/ohos_glex
```

### Basic Usage

```typescript
import { GLEXComponent, GLInfo } from '@yunshen1933/ohos_glex';

@Entry
@Component
struct DemoPage {
  build() {
    Column() {
      GLEXComponent({
        targetFPS: 60,
        clearColor: [0.02, 0.03, 0.10, 1.0],
        autoStart: true,
        onReady: () => {
          console.info('OpenGL rendering started');
        }
      })
        .width('100%')
        .height('100%')
    }
  }
}
```

### Get Runtime Information

```typescript
private glexRef: GLEXComponent | null = null;

GLEXComponent({
  targetFPS: 60,
  onReady: () => {
    const fps = this.glexRef?.getCurrentFPS();
    const info: GLInfo | undefined = this.glexRef?.getGLInfo();
    console.info(`FPS: ${fps}, GPU: ${info?.renderer}`);
  }
})
```

## Architecture Design

```
┌─────────────────────────────────────────┐
│  ArkTS Layer (GLEXComponent)            │  ← Declarative UI Component
├─────────────────────────────────────────┤
│  NAPI Bridge                             │  ← JS ↔ Native Bridge
├─────────────────────────────────────────┤
│  RenderThread (Independent Thread)       │  ← Frame Rate Control + FPS Stats
├─────────────────────────────────────────┤
│  RenderPipeline                          │  ← Multi-Stage Render Pipeline
│  ┌──────────┬──────────┬──────────┐     │
│  │ Pass 1   │ Pass 2   │ Pass N   │     │  ← Extensible Render Stages
│  └──────────┴──────────┴──────────┘     │
├─────────────────────────────────────────┤
│  GLContext │ ShaderProgram              │  ← Core Utility Classes
├─────────────────────────────────────────┤
│  EGL / OpenGL ES 3.x                    │  ← System API
└─────────────────────────────────────────┘
```

## Use Cases

- ✅ **Game Engines** — 2D/3D game rendering
- ✅ **Data Visualization** — Large-scale particle systems, real-time charts
- ✅ **Image Processing** — GPU-accelerated filters and effects
- ✅ **AR/VR** — Immersive experiences
- ✅ **Creative Apps** — Music visualization, generative art

## Documentation

- [Complete API Documentation](library/README.md)
- [Environment Setup Guide](SETUP.md)
- [Example Code](entry/src/main/ets/pages/Index.ets)

## System Requirements

- **Development Environment**: DevEco Studio 5.0.0+
- **HarmonyOS SDK**: API 12 (6.0.2)+
- **Device Requirements**: OpenGL ES 3.0+ support
- **Supported Devices**: phone, tablet, 2in1
- **ABI Architectures**: arm64-v8a, x86_64

## Contributing

Issues and Pull Requests are welcome!

## License

This project is licensed under [Apache License 2.0](LICENSE).

## Author

**YunShen** - HarmonyOS Ecosystem Developer

---

<div align="center">

**If this project helps you, please give it a ⭐️ Star!**

This is the first open-source OpenGL framework in HarmonyOS ecosystem. Your support drives continuous maintenance.

</div>
