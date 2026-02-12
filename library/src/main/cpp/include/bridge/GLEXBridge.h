#pragma once

/**
 * @file GLEXBridge.h
 * @brief GLEX NAPI 桥接层
 *
 * 提供 ArkTS 与 Native OpenGL 渲染之间的桥接。
 * 管理 XComponent 生命周期、EGL 上下文创建、渲染线程启停。
 *
 * 内置演示渲染器，展示框架的渲染管线能力。
 */

#include <napi/native_api.h>

namespace glex {
namespace bridge {

/**
 * NAPI 模块初始化函数
 * 注册所有导出的 NAPI 方法和 XComponent 回调
 */
napi_value Init(napi_env env, napi_value exports);

} // namespace bridge
} // namespace glex
