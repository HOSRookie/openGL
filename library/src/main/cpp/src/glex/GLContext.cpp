#include "glex/GLContext.h"
#include "glex/Log.h"

namespace glex {

GLContext::~GLContext()
{
    destroy();
}

bool GLContext::initialize(EGLNativeWindowType window, const GLContextConfig& config)
{
    if (initialized_) {
        GLEX_LOGW("GLContext already initialized");
        return true;
    }

    GLEX_LOGI("GLContext::initialize window=%{public}p", window);

    // 1. 获取 EGL Display
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY) {
        GLEX_LOGE("Failed to get EGL display, error=0x%{public}X", eglGetError());
        return false;
    }

    // 2. 初始化 EGL
    EGLint major = 0;
    EGLint minor = 0;
    if (!eglInitialize(display_, &major, &minor)) {
        GLEX_LOGE("Failed to initialize EGL, error=0x%{public}X", eglGetError());
        return false;
    }
    GLEX_LOGI("EGL version: %{public}d.%{public}d", major, minor);

    // 3. 选择 EGL 配置
    if (!chooseConfig(config)) {
        return false;
    }

    // 4. 创建窗口 Surface
    surface_ = eglCreateWindowSurface(display_, eglConfig_, window, nullptr);
    if (surface_ == EGL_NO_SURFACE) {
        GLEX_LOGE("Failed to create EGL surface, error=0x%{public}X", eglGetError());
        return false;
    }

    eglBindAPI(EGL_OPENGL_ES_API);

    // 5. 创建 EGL 上下文（ES 3.2 → 3.0 → 2.0 逐级回退）
    EGLint contextAttribs32[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 2,
        EGL_NONE
    };
    context_ = eglCreateContext(display_, eglConfig_, EGL_NO_CONTEXT, contextAttribs32);

    if (context_ == EGL_NO_CONTEXT) {
        GLEX_LOGW("ES 3.2 unavailable (0x%{public}X), trying 3.0", eglGetError());
        EGLint contextAttribs30[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
        context_ = eglCreateContext(display_, eglConfig_, EGL_NO_CONTEXT, contextAttribs30);
    }

    if (context_ == EGL_NO_CONTEXT) {
        GLEX_LOGW("ES 3.0 unavailable (0x%{public}X), trying 2.0", eglGetError());
        EGLint contextAttribs20[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        context_ = eglCreateContext(display_, eglConfig_, EGL_NO_CONTEXT, contextAttribs20);
    }

    if (context_ == EGL_NO_CONTEXT) {
        GLEX_LOGE("Failed to create any EGL context, error=0x%{public}X", eglGetError());
        return false;
    }

    // 6. 绑定上下文
    if (!makeCurrent()) {
        GLEX_LOGE("Failed to make context current, error=0x%{public}X", eglGetError());
        return false;
    }

    // 7. 查询 Surface 尺寸
    EGLint eglWidth = 0;
    EGLint eglHeight = 0;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &eglWidth);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &eglHeight);
    width_ = static_cast<int>(eglWidth);
    height_ = static_cast<int>(eglHeight);

    // 8. 设置 VSync
    eglSwapInterval(display_, config.vsyncEnabled ? 1 : 0);

    GLEX_LOGI("GL initialized: surface %{public}dx%{public}d", width_, height_);
    GLEX_LOGI("GL_VENDOR:   %{public}s", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
    GLEX_LOGI("GL_RENDERER: %{public}s", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    GLEX_LOGI("GL_VERSION:  %{public}s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));

    initialized_ = true;
    return true;
}

void GLContext::destroy()
{
    if (!initialized_) {
        return;
    }

    clearCurrent();

    if (context_ != EGL_NO_CONTEXT) {
        eglDestroyContext(display_, context_);
        context_ = EGL_NO_CONTEXT;
    }

    if (surface_ != EGL_NO_SURFACE) {
        eglDestroySurface(display_, surface_);
        surface_ = EGL_NO_SURFACE;
    }

    if (display_ != EGL_NO_DISPLAY) {
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }

    width_ = 0;
    height_ = 0;
    initialized_ = false;
    GLEX_LOGI("GLContext destroyed");
}

bool GLContext::makeCurrent()
{
    if (display_ == EGL_NO_DISPLAY || surface_ == EGL_NO_SURFACE || context_ == EGL_NO_CONTEXT) {
        return false;
    }
    return eglMakeCurrent(display_, surface_, surface_, context_) == EGL_TRUE;
}

void GLContext::clearCurrent()
{
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
}

bool GLContext::swapBuffers()
{
    if (!initialized_) {
        return false;
    }
    return eglSwapBuffers(display_, surface_) == EGL_TRUE;
}

void GLContext::setVSyncEnabled(bool enabled)
{
    if (initialized_) {
        eglSwapInterval(display_, enabled ? 1 : 0);
    }
}

const char* GLContext::getGLVersion() const
{
    return reinterpret_cast<const char*>(glGetString(GL_VERSION));
}

const char* GLContext::getGLRenderer() const
{
    return reinterpret_cast<const char*>(glGetString(GL_RENDERER));
}

bool GLContext::chooseConfig(const GLContextConfig& config)
{
#ifndef EGL_OPENGL_ES3_BIT
#define EGL_OPENGL_ES3_BIT 0x00000040
#endif
#ifndef EGL_OPENGL_ES2_BIT
#define EGL_OPENGL_ES2_BIT 0x00000004
#endif

    // 优先 ES3 配置
    const EGLint es3Attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, config.redSize,
        EGL_GREEN_SIZE, config.greenSize,
        EGL_BLUE_SIZE, config.blueSize,
        EGL_ALPHA_SIZE, config.alphaSize,
        EGL_DEPTH_SIZE, config.depthSize,
        EGL_STENCIL_SIZE, config.stencilSize,
        EGL_NONE
    };

    EGLint numConfigs = 0;
    if (eglChooseConfig(display_, es3Attribs, &eglConfig_, 1, &numConfigs) && numConfigs > 0) {
        GLEX_LOGI("Using ES3 EGL config");
        return true;
    }

    GLEX_LOGW("ES3 config unavailable, trying ES2 fallback");

    // 回退 ES2
    const EGLint es2Attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, config.redSize,
        EGL_GREEN_SIZE, config.greenSize,
        EGL_BLUE_SIZE, config.blueSize,
        EGL_ALPHA_SIZE, config.alphaSize,
        EGL_DEPTH_SIZE, config.depthSize,
        EGL_STENCIL_SIZE, config.stencilSize,
        EGL_NONE
    };

    if (eglChooseConfig(display_, es2Attribs, &eglConfig_, 1, &numConfigs) && numConfigs > 0) {
        GLEX_LOGI("Using ES2 fallback config");
        return true;
    }

    GLEX_LOGE("Failed to choose any EGL config, error=0x%{public}X", eglGetError());
    return false;
}

} // namespace glex
