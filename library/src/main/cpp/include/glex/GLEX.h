#pragma once

/**
 * @file GLEX.h
 * @brief GLEX 框架主头文件
 *
 * 包含此文件即可使用 GLEX 框架的所有核心功能。
 *
 * GLEX (GL Extension for OpenHarmony) 是一个轻量级的
 * OpenGL ES 渲染框架，专为 OpenHarmony/HarmonyOS 设计。
 *
 * 核心组件：
 *   - GLContext: EGL 上下文管理
 *   - ShaderProgram: 着色器编译与 Uniform 管理
 *   - RenderPass: 渲染阶段抽象
 *   - RenderPipeline: 多阶段渲染管线
 *   - RenderThread: 独立渲染线程
 *
 * @author 云深
 * @version 1.0.2
 */

#include "glex/GLContext.h"
#include "glex/ShaderProgram.h"
#include "glex/RenderPass.h"
#include "glex/RenderPipeline.h"
#include "glex/RenderThread.h"
#include "glex/Log.h"

#define GLEX_VERSION_MAJOR 1
#define GLEX_VERSION_MINOR 0
#define GLEX_VERSION_PATCH 2
#define GLEX_VERSION_STRING "1.0.2"
