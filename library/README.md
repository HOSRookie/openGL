# @yunshen1933/ohos_glex

`@yunshen1933/ohos_glex` 是 GLEX 的库模块，负责：

- ArkTS 导出与类型声明
- Native 渲染实例工厂
- C++ 渲染管线与 NAPI 桥接

## 导出项

### 类型与组件

- `BuiltinPass`: `'demo' | 'attack' | 'none'`
- `GLInfo`
- `GpuStats`
- `GLEXComponent`（封装组件，持续优化中）

### Native 工厂（推荐）

- `createGlexRenderer(): GlexNativeInstance`

## GlexNativeInstance API

| 方法 | 说明 |
|---|---|
| `bindXComponent(id)` | 绑定页面中的 `XComponent` ID |
| `unbindXComponent()` | 解绑当前组件 |
| `setSurfaceId(surfaceId)` | 通过 SurfaceId 直接绑定渲染窗口（可选） |
| `startRender()` | 启动渲染循环 |
| `stopRender()` | 停止渲染循环 |
| `destroySurface()` | 销毁当前 surface 与上下文关联资源 |
| `resize(width, height)` | 主动更新渲染尺寸 |
| `setTargetFPS(fps)` | 设置目标帧率 |
| `setBackgroundColor(r, g, b, a?)` | 设置清屏颜色 |
| `setShaderSources(vs, fs)` | 设置自定义 Shader 源码 |
| `loadShaderFromRawfile(resMgr, vsPath, fsPath)` | 从 Rawfile 加载 Shader |
| `loadRawfileBytes(resMgr, path)` | 从 Rawfile 加载二进制数据 |
| `setUniform(name, value)` | 设置 Shader Uniform |
| `setPasses(names)` | 按顺序设置 Pass 列表 |
| `addPass(name)` | 增加一个 Pass |
| `removePass(name)` | 移除一个 Pass |
| `getPasses()` | 获取当前 Pass 列表 |
| `setTouchEvent(x, y, action, pointerId?)` | 传递触摸事件到渲染管线 |
| `getCurrentFPS()` | 获取当前渲染 FPS |
| `getGLInfo()` | 获取渲染尺寸与 GPU 信息 |
| `getGpuStats()` | 获取 GPU 资源统计 |
| `getLastError()` | 获取最近错误字符串 |
| `clearLastError()` | 清空最近错误 |

## 内置 Pass 名称

- `DemoPass`：星空演示
- `AttackPass`：斩击特效
- `ShaderPass`：自定义 shader 通道

`setPasses` 示例：

```ts
native.setPasses(['AttackPass']);
```

## 触摸 action 约定

- `0`: Down
- `1`: Move
- `2`: Up
- `3`: Cancel/Other

## 推荐接入示例

```ts
import { createGlexRenderer, GlexNativeInstance } from '@yunshen1933/ohos_glex';

@Entry
@Component
struct Page {
  private xId: string = 'glex_' + Date.now().toString();
  private native: GlexNativeInstance = createGlexRenderer();

  build() {
    Stack() {
      XComponent({ id: this.xId, type: XComponentType.SURFACE, libraryname: 'glex' })
        .width('100%')
        .height('100%')
        .onLoad(() => {
          this.native.bindXComponent(this.xId);
          this.native.setTargetFPS(60);
          this.native.setPasses(['DemoPass']);
          this.native.startRender();
        })
        .onDestroy(() => {
          this.native.stopRender();
          this.native.destroySurface();
          this.native.unbindXComponent();
        });
    }
    .onTouch((event: TouchEvent) => {
      const t: TouchObject[] = event.touches && event.touches.length > 0
        ? event.touches
        : (event.changedTouches ?? []);
      if (t.length === 0) {
        return;
      }
      const p: TouchObject = t[0];
      let action: number = 3;
      if (event.type === TouchType.Down) {
        action = 0;
      } else if (event.type === TouchType.Move) {
        action = 1;
      } else if (event.type === TouchType.Up) {
        action = 2;
      }
      this.native.setTouchEvent(p.x, p.y, action, p.id);
    });
  }
}
```

## C++ 扩展入口

C++ 相关代码位于：

- `src/main/cpp/include/glex/`
- `src/main/cpp/src/glex/`
- `src/main/cpp/src/bridge/`

可基于 `RenderPass` 扩展自定义效果，并通过注册机制加入管线。

## 兼容性策略（0.x）

- 当前处于 0.x 阶段，但维护策略是“稳定优先”：非必要不做大改，不随意重命名或移除已有导出。
- 常规迭代以新增能力和修复问题为主，尽量保持现有接入方式可用。
- 仅在以下场景考虑破坏性调整：
  - HarmonyOS / ArkUI / NAPI 官方接口发生重大变更
  - 必须处理的安全、兼容性、稳定性问题
- 若出现破坏性调整，会在 `CHANGELOG.md` 明确标注影响范围与迁移建议。
