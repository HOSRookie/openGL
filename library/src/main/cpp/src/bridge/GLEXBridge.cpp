#include "bridge/GLEXBridge.h"

#include <atomic>
#include <cinttypes>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>

#include <GLES3/gl3.h>
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <native_window/external_window.h>

#include "glex/GLEX.h"
#include "DemoPass.h"

namespace glex {
namespace bridge {

// ============================================================
// 全局状态
// ============================================================

static std::mutex g_mutex;
static std::mutex g_errorMutex;
static OHNativeWindow* g_nativeWindow = nullptr;
static uint64_t g_surfaceId = 0;
static std::atomic<bool> g_startRequested{false};

static std::unique_ptr<GLContext> g_glContext;
static std::unique_ptr<RenderThread> g_renderThread;
static std::unique_ptr<RenderPipeline> g_pipeline;

static std::atomic<float> g_bgColorR{0.02f};
static std::atomic<float> g_bgColorG{0.03f};
static std::atomic<float> g_bgColorB{0.10f};
static std::atomic<float> g_bgColorA{1.0f};
static std::atomic<int> g_targetFPS{60};
static std::string g_lastError;

static void SetError(const std::string& msg)
{
    std::lock_guard<std::mutex> lock(g_errorMutex);
    g_lastError = msg;
}

static void ClearError()
{
    std::lock_guard<std::mutex> lock(g_errorMutex);
    g_lastError.clear();
}

static std::string GetError()
{
    std::lock_guard<std::mutex> lock(g_errorMutex);
    return g_lastError;
}

// ============================================================
// NAPI 工具函数
// ============================================================

static napi_value GetUndefined(napi_env env)
{
    napi_value result;
    napi_get_undefined(env, &result);
    return result;
}

static bool GetInt32(napi_env env, napi_value value, int32_t* out)
{
    return napi_get_value_int32(env, value, out) == napi_ok;
}

static bool GetDouble(napi_env env, napi_value value, double* out)
{
    return napi_get_value_double(env, value, out) == napi_ok;
}

static bool GetSurfaceId(napi_env env, napi_value value, uint64_t* out)
{
    napi_valuetype type;
    if (napi_typeof(env, value, &type) != napi_ok) return false;

    if (type == napi_bigint) {
        bool lossless = false;
        return napi_get_value_bigint_uint64(env, value, out, &lossless) == napi_ok;
    }
    if (type == napi_string) {
        size_t length = 0;
        napi_get_value_string_utf8(env, value, nullptr, 0, &length);
        std::string str(length + 1, '\0');
        size_t copied = 0;
        napi_get_value_string_utf8(env, value, str.data(), str.size(), &copied);
        *out = static_cast<uint64_t>(strtoull(str.c_str(), nullptr, 0));
        return true;
    }
    if (type == napi_number) {
        double num = 0.0;
        if (GetDouble(env, value, &num)) {
            *out = static_cast<uint64_t>(num);
            return true;
        }
    }
    return false;
}

// ============================================================
// 渲染管理
// ============================================================

static void InitializeRenderer(int width, int height)
{
    if (!g_pipeline) {
        g_pipeline = std::make_unique<RenderPipeline>();
        if (g_glContext && g_glContext->getGLESVersionMajor() >= 3) {
            g_pipeline->addPass(std::make_shared<DemoPass>());
        } else {
            GLEX_LOGE("DemoPass requires OpenGL ES 3.0+");
            SetError("DemoPass requires OpenGL ES 3.0+");
        }
    }
    if (g_pipeline && g_pipeline->getPassCount() > 0) {
        g_pipeline->initialize(width, height);
        ClearError();
    }
}

static void DestroyRenderer()
{
    if (g_pipeline) {
        g_pipeline->destroy();
        g_pipeline.reset();
    }
}

static void StartRenderLoopLocked()
{
    if (!g_glContext || !g_glContext->isInitialized()) return;
    if (g_renderThread && g_renderThread->isRunning()) return;

    // 渲染线程启动前清除主线程的 GL 上下文绑定
    g_glContext->clearCurrent();

    if (!g_renderThread) {
        g_renderThread = std::make_unique<RenderThread>();
    }

    g_renderThread->setTargetFPS(g_targetFPS.load(std::memory_order_relaxed));
    g_renderThread->start(g_glContext.get(), [](float deltaTime) {
        int w = g_glContext->getWidth();
        int h = g_glContext->getHeight();

        glViewport(0, 0, w, h);
        glClearColor(
            g_bgColorR.load(std::memory_order_relaxed),
            g_bgColorG.load(std::memory_order_relaxed),
            g_bgColorB.load(std::memory_order_relaxed),
            g_bgColorA.load(std::memory_order_relaxed)
        );
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (g_pipeline) {
            g_pipeline->update(deltaTime);
            g_pipeline->render();
        }
    });
}

static void StopRenderLoopLocked()
{
    if (g_renderThread) {
        g_renderThread->stop();
    }
}

static void StartRenderLoop()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    StartRenderLoopLocked();
}

static void StopRenderLoop()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    StopRenderLoopLocked();
}

// ============================================================
// NAPI 接口实现
// ============================================================

static napi_value SetSurfaceId(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) return GetUndefined(env);

    uint64_t surfaceId = 0;
    if (!GetSurfaceId(env, args[0], &surfaceId)) {
        GLEX_LOGE("setSurfaceId: invalid surface id");
        SetError("setSurfaceId: invalid surface id");
        return GetUndefined(env);
    }

    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_surfaceId == surfaceId && g_nativeWindow != nullptr) {
        return GetUndefined(env);
    }

    // 清理旧的
    if (g_nativeWindow != nullptr) {
        StopRenderLoop();
        DestroyRenderer();
        if (g_glContext) {
            g_glContext->destroy();
            g_glContext.reset();
        }
        OH_NativeWindow_DestroyNativeWindow(g_nativeWindow);
        g_nativeWindow = nullptr;
    }

    g_surfaceId = surfaceId;

    // 创建原生窗口
    OHNativeWindow* window = nullptr;
    int32_t result = OH_NativeWindow_CreateNativeWindowFromSurfaceId(surfaceId, &window);
    if (result != 0 || window == nullptr) {
        GLEX_LOGE("setSurfaceId: create native window failed, result=%{public}d", result);
        SetError("setSurfaceId: create native window failed");
        return GetUndefined(env);
    }
    g_nativeWindow = window;

    // 初始化 GL 上下文
    g_glContext = std::make_unique<GLContext>();
    if (!g_glContext->initialize(reinterpret_cast<EGLNativeWindowType>(window))) {
        GLEX_LOGE("setSurfaceId: GL init failed");
        SetError("setSurfaceId: GL init failed");
        g_glContext.reset();
        return GetUndefined(env);
    }

    // 初始化渲染器
    InitializeRenderer(g_glContext->getWidth(), g_glContext->getHeight());

    // 解绑上下文，为渲染线程让出绑定权
    g_glContext->clearCurrent();

    GLEX_LOGI("setSurfaceId: ok, id=%" PRIu64 ", size=%{public}dx%{public}d",
              surfaceId, g_glContext->getWidth(), g_glContext->getHeight());

    // 如果之前已请求启动
    if (g_startRequested.load()) {
        StartRenderLoop();
        g_startRequested.store(false);
    }

    return GetUndefined(env);
}

static napi_value DestroySurface(napi_env env, napi_callback_info info)
{
    (void)info;
    std::lock_guard<std::mutex> lock(g_mutex);

    StopRenderLoop();
    DestroyRenderer();

    if (g_glContext) {
        g_glContext->destroy();
        g_glContext.reset();
    }

    if (g_nativeWindow != nullptr) {
        OH_NativeWindow_DestroyNativeWindow(g_nativeWindow);
        g_nativeWindow = nullptr;
    }

    g_surfaceId = 0;
    g_startRequested.store(false);
    GLEX_LOGI("destroySurface: done");
    return GetUndefined(env);
}

static napi_value NapiStartRender(napi_env env, napi_callback_info info)
{
    (void)info;
    g_startRequested.store(true);

    if (g_glContext && g_glContext->isInitialized()) {
        StartRenderLoop();
        g_startRequested.store(false);
    } else {
        GLEX_LOGW("startRender: GL not ready, deferred");
    }

    return GetUndefined(env);
}

static napi_value NapiStopRender(napi_env env, napi_callback_info info)
{
    (void)info;
    g_startRequested.store(false);
    StopRenderLoop();
    return GetUndefined(env);
}

static napi_value NapiResize(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc >= 2) {
        int32_t w = 0, h = 0;
        if (GetInt32(env, args[0], &w) && GetInt32(env, args[1], &h)) {
            if (g_pipeline) {
                g_pipeline->resize(w, h);
            }
            if (g_glContext) {
                g_glContext->setSurfaceSize(w, h);
            }
            GLEX_LOGI("resize: %{public}dx%{public}d", w, h);
        }
    }
    return GetUndefined(env);
}

static napi_value SetBackgroundColor(napi_env env, napi_callback_info info)
{
    size_t argc = 4;
    napi_value args[4];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc >= 3) {
        double r = 0, g = 0, b = 0, a = 1.0;
        GetDouble(env, args[0], &r);
        GetDouble(env, args[1], &g);
        GetDouble(env, args[2], &b);
        if (argc >= 4) GetDouble(env, args[3], &a);

        g_bgColor[0] = static_cast<float>(r);
        g_bgColor[1] = static_cast<float>(g);
        g_bgColor[2] = static_cast<float>(b);
        g_bgColor[3] = static_cast<float>(a);
    }
    return GetUndefined(env);
}

static napi_value SetTargetFPS(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc >= 1) {
        int32_t fps = 60;
        if (GetInt32(env, args[0], &fps)) {
            g_targetFPS = fps;
            if (g_renderThread) {
                g_renderThread->setTargetFPS(fps);
            }
        }
    }
    return GetUndefined(env);
}

static napi_value GetCurrentFPS(napi_env env, napi_callback_info info)
{
    (void)info;
    float fps = g_renderThread ? g_renderThread->getCurrentFPS() : 0.0f;
    napi_value result;
    napi_create_double(env, static_cast<double>(fps), &result);
    return result;
}

static napi_value GetGLInfo(napi_env env, napi_callback_info info)
{
    (void)info;
    napi_value result;
    napi_create_object(env, &result);

    auto setStr = [&](const char* key, const char* value) {
        napi_value v;
        napi_create_string_utf8(env, value ? value : "unknown", NAPI_AUTO_LENGTH, &v);
        napi_set_named_property(env, result, key, v);
    };

    if (g_glContext && g_glContext->isInitialized()) {
        setStr("version", g_glContext->getGLVersion());
        setStr("renderer", g_glContext->getGLRenderer());

        napi_value w, h;
        napi_create_int32(env, g_glContext->getWidth(), &w);
        napi_create_int32(env, g_glContext->getHeight(), &h);
        napi_set_named_property(env, result, "width", w);
        napi_set_named_property(env, result, "height", h);
    } else {
        setStr("version", "not initialized");
        setStr("renderer", "not initialized");
    }

    return result;
}

static napi_value GetLastError(napi_env env, napi_callback_info info)
{
    (void)info;
    std::string err = GetError();
    napi_value result;
    napi_create_string_utf8(env, err.c_str(), NAPI_AUTO_LENGTH, &result);
    return result;
}

static napi_value ClearLastError(napi_env env, napi_callback_info info)
{
    (void)info;
    ClearError();
    return GetUndefined(env);
}

// ============================================================
// XComponent 回调
// ============================================================

static OH_NativeXComponent_Callback g_xcomponentCallback;

static void OnSurfaceCreated(OH_NativeXComponent* component, void* window)
{
    GLEX_LOGI("XComponent: OnSurfaceCreated");
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_nativeWindow != nullptr) {
        StopRenderLoop();
        DestroyRenderer();
        if (g_glContext) {
            g_glContext->destroy();
            g_glContext.reset();
        }
        g_nativeWindow = nullptr;
    }

    g_nativeWindow = static_cast<OHNativeWindow*>(window);

    g_glContext = std::make_unique<GLContext>();
    if (!g_glContext->initialize(reinterpret_cast<EGLNativeWindowType>(window))) {
        GLEX_LOGE("XComponent: GL init failed");
        SetError("XComponent: GL init failed");
        g_glContext.reset();
        return;
    }

    InitializeRenderer(g_glContext->getWidth(), g_glContext->getHeight());

    // 关键：初始化完成后必须解绑上下文，否则渲染线程无法 makeCurrent
    g_glContext->clearCurrent();

    if (g_startRequested.load()) {
        StartRenderLoop();
        g_startRequested.store(false);
    }
}

static void OnSurfaceChanged(OH_NativeXComponent* component, void* window)
{
    uint64_t w = 0, h = 0;
    OH_NativeXComponent_GetXComponentSize(component, window, &w, &h);
    GLEX_LOGI("XComponent: OnSurfaceChanged %{public}dx%{public}d",
              static_cast<int>(w), static_cast<int>(h));

    if (g_pipeline) {
        g_pipeline->resize(static_cast<int>(w), static_cast<int>(h));
    }
    if (g_glContext) {
        g_glContext->setSurfaceSize(static_cast<int>(w), static_cast<int>(h));
    }
}

static void OnSurfaceDestroyed(OH_NativeXComponent* component, void* window)
{
    GLEX_LOGI("XComponent: OnSurfaceDestroyed");
    std::lock_guard<std::mutex> lock(g_mutex);

    StopRenderLoop();
    DestroyRenderer();

    if (g_glContext) {
        g_glContext->destroy();
        g_glContext.reset();
    }

    g_nativeWindow = nullptr;
    g_surfaceId = 0;
}

static void OnDispatchTouchEvent(OH_NativeXComponent* component, void* window)
{
    // 预留触摸事件接口
}

static OH_NativeXComponent_Callback* GetXComponentCallback()
{
    static bool initialized = false;
    if (!initialized) {
        g_xcomponentCallback.OnSurfaceCreated = OnSurfaceCreated;
        g_xcomponentCallback.OnSurfaceChanged = OnSurfaceChanged;
        g_xcomponentCallback.OnSurfaceDestroyed = OnSurfaceDestroyed;
        g_xcomponentCallback.DispatchTouchEvent = OnDispatchTouchEvent;
        initialized = true;
    }
    return &g_xcomponentCallback;
}

// ============================================================
// 模块初始化
// ============================================================

napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        // 核心生命周期
        { "setSurfaceId",       nullptr, SetSurfaceId,       nullptr, nullptr, nullptr, napi_default, nullptr },
        { "destroySurface",     nullptr, DestroySurface,     nullptr, nullptr, nullptr, napi_default, nullptr },
        { "startRender",        nullptr, NapiStartRender,    nullptr, nullptr, nullptr, napi_default, nullptr },
        { "stopRender",         nullptr, NapiStopRender,     nullptr, nullptr, nullptr, napi_default, nullptr },
        { "resize",             nullptr, NapiResize,         nullptr, nullptr, nullptr, napi_default, nullptr },
        // 配置
        { "setBackgroundColor", nullptr, SetBackgroundColor, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setTargetFPS",       nullptr, SetTargetFPS,       nullptr, nullptr, nullptr, napi_default, nullptr },
        // 查询
        { "getCurrentFPS",      nullptr, GetCurrentFPS,      nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getGLInfo",          nullptr, GetGLInfo,          nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getLastError",       nullptr, GetLastError,       nullptr, nullptr, nullptr, napi_default, nullptr },
        { "clearLastError",     nullptr, ClearLastError,     nullptr, nullptr, nullptr, napi_default, nullptr },
    };

    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);

    // 注册 XComponent 回调
    napi_value xcomponentObj = nullptr;
    if (napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &xcomponentObj) == napi_ok) {
        OH_NativeXComponent* nativeXComponent = nullptr;
        if (napi_unwrap(env, xcomponentObj, reinterpret_cast<void**>(&nativeXComponent)) == napi_ok &&
            nativeXComponent != nullptr) {
            OH_NativeXComponent_RegisterCallback(nativeXComponent, GetXComponentCallback());
            GLEX_LOGI("XComponent callback registered");
        }
    }

    GLEX_LOGI("GLEX module initialized (v%{public}s)", GLEX_VERSION_STRING);
    return exports;
}

} // namespace bridge
} // namespace glex

// NAPI 模块注册
static napi_value GLEXInit(napi_env env, napi_value exports)
{
    return glex::bridge::Init(env, exports);
}

NAPI_MODULE(glex, GLEXInit)
