# GLEX

轻量的 HarmonyOS OpenGL ES 渲染库，提供 Native 渲染实例、Pass 管理、触摸事件转发和基础调试能力。

## 当前状态

- 项目类型：可运行的 1.0.2 开源项目（持续迭代中）
- 主要能力：
  - `XComponent + Native` 渲染链路
  - 可切换内置 Pass（`DemoPass`、`AttackPass`、`ShaderPass`）
  - 触摸事件传递到渲染管线
  - `Rawfile` Shader 读取、基础 GPU 资源统计
- 推荐接入方式：`XComponent + createGlexRenderer()`

## 稳定性与变更策略

- 当前阶段以稳定迭代为主：默认保持 API 与渲染主链路稳定，不做无必要的大规模重构。
- 仅在以下场景引入较大调整：
  - HarmonyOS / ArkUI / NAPI 官方接口出现破坏性变化
  - 必须处理的安全、兼容性或稳定性问题
  - 影响长期维护成本的底层结构缺陷
- 涉及破坏性变更时，会在 `library/CHANGELOG.md` 提前标注并给出迁移说明。

## 环境要求

- DevEco Studio 5.0+
- HarmonyOS SDK API 12+
- OpenGL ES 3.0+
- ABI：`arm64-v8a`、`x86_64`

## 安装

```bash
ohpm install @yunshen1933/ohos_glex
```

## 快速接入（推荐）

当前最稳定的方式是页面内直接使用 `XComponent`，通过库导出的 `createGlexRenderer()` 绑定 Native 渲染实例。

```ts
import { createGlexRenderer, GlexNativeInstance } from '@yunshen1933/ohos_glex';

@Entry
@Component
struct Index {
  private xId: string = 'glex_' + Date.now().toString();
  private native: GlexNativeInstance = createGlexRenderer();

  build() {
    XComponent({
      id: this.xId,
      type: XComponentType.SURFACE,
      libraryname: 'glex',
    })
      .width('100%')
      .height('100%')
      .onLoad(() => {
        this.native.bindXComponent(this.xId);
        this.native.setPasses(['DemoPass']);
        this.native.startRender();
      })
      .onDestroy(() => {
        this.native.stopRender();
        this.native.destroySurface();
        this.native.unbindXComponent();
      });
  }
}
```

## 示例能力

`entry/src/main/ets/pages/Index.ets` 已实现：

- 星空（`DemoPass`）与斩击（`AttackPass`）切换
- 触摸转发（斩击模式）
- 渲染区域尺寸与触摸坐标缩放处理

## 文档

- 组件与 API：`library/README.md`
- 环境配置：`SETUP.md`
- 基线说明：`docs/phase0_baseline.md`

## 已知事项

- `GLEXComponent` 组件封装仍在持续优化，遇到 ArkTS 版本兼容问题时，优先使用上面的推荐接入方式。
- 命令行构建依赖本机 SDK 环境变量，建议优先在 DevEco Studio 中构建与运行。

## 目录结构

```text
entry/      示例应用
library/    可发布库（ArkTS + C++）
docs/       设计与阶段性文档
```

## 开源协议

Apache-2.0
