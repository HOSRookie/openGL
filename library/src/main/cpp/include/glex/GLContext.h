#pragma once

/**
 * @file GLContext.h
 * @brief EGL 上下文管理器
 *
 * 封装 EGL 初始化、上下文创建、Surface 管理、缓冲交换等操作。
 * 支持 OpenGL ES 3.2 → 3.0 → 2.0 逐级回退。
 *
 * 用法：
 *   GLContext ctx;
 *   ctx.initialize(nativeWindow);
 *   // ... 渲染循环 ...
 *   ctx.destroy();
 */

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

namespace glex {

/**
 * EGL 配置选项
 */
struct GLContextConfig {
    int redSize = 8;
    int greenSize = 8;
    int blueSize = 8;
    int alphaSize = 8;
    int depthSize = 16;
    int stencilSize = 0;
    bool vsyncEnabled = true;
};

class GLContext {
public:
    GLContext() = default;
    ~GLContext();

    // 禁止拷贝
    GLContext(const GLContext&) = delete;
    GLContext& operator=(const GLContext&) = delete;

    /**
     * 初始化 EGL 上下文
     * @param window 原生窗口句柄（来自 XComponent 或 NativeWindow）
     * @param config EGL 配置选项
     * @return 成功返回 true
     */
    bool initialize(EGLNativeWindowType window, const GLContextConfig& config = {});

    /**
     * 销毁 EGL 上下文和所有资源
     */
    void destroy();

    /** 绑定当前线程的 EGL 上下文 */
    bool makeCurrent();

    /** 解绑当前线程的 EGL 上下文 */
    void clearCurrent();

    /** 交换前后缓冲区 */
    bool swapBuffers();

    /** 设置垂直同步 */
    void setVSyncEnabled(bool enabled);

    /** 获取 Surface 宽度（像素） */
    int getWidth() const { return width_; }

    /** 获取 Surface 高度（像素） */
    int getHeight() const { return height_; }

    /** 是否已成功初始化 */
    bool isInitialized() const { return initialized_; }

    /** 获取 EGL 版本字符串 */
    const char* getGLVersion() const;

    /** 获取 GPU 渲染器名称 */
    const char* getGLRenderer() const;

    /** 获取 OpenGL ES 主版本号 */
    int getGLESVersionMajor() const { return glMajor_; }

    /** 获取 OpenGL ES 次版本号 */
    int getGLESVersionMinor() const { return glMinor_; }

    /** 更新 Surface 尺寸（来自外部 resize） */
    void setSurfaceSize(int width, int height) { width_ = width; height_ = height; }

private:
    bool chooseConfig(const GLContextConfig& config);

    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
    EGLConfig eglConfig_ = nullptr;

    int width_ = 0;
    int height_ = 0;
    int glMajor_ = 0;
    int glMinor_ = 0;
    bool initialized_ = false;
};

} // namespace glex
