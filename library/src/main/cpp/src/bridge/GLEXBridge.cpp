#include "bridge/GLEXBridge.h"

#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <GLES3/gl3.h>
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <native_window/external_window.h>
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>

#include "glex/GLEX.h"
#include "ShaderPass.h"
#include "BuiltinPassRegistry.h"
#include "glex/PassRegistry.h"

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
static std::atomic<int> g_pendingWidth{0};
static std::atomic<int> g_pendingHeight{0};
static std::atomic<bool> g_resizePending{false};
static std::string g_lastError;
static std::mutex g_shaderMutex;
static std::string g_pendingVert;
static std::string g_pendingFrag;
static std::atomic<bool> g_shaderPending{false};
static std::shared_ptr<ShaderPass> g_customPass;
static std::mutex g_uniformMutex;
static std::unordered_map<std::string, std::vector<float>> g_pendingUniforms;
static std::atomic<bool> g_uniformDirty{false};
static std::atomic<float> g_touchX{0.0f};
static std::atomic<float> g_touchY{0.0f};
static std::atomic<int> g_touchAction{0};
static std::atomic<int> g_touchPointerId{0};
static std::atomic<uint64_t> g_touchSeq{0};

static std::mutex g_passMutex;
static std::vector<std::string> g_requestedPasses{"DemoPass"};
static std::atomic<bool> g_passesDirty{false};

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

static void RequestResize(int width, int height)
{
    g_pendingWidth.store(width, std::memory_order_relaxed);
    g_pendingHeight.store(height, std::memory_order_relaxed);
    g_resizePending.store(true, std::memory_order_release);
    if (g_glContext) {
        g_glContext->setSurfaceSize(width, height);
    }
}

static void RequestShaderUpdate(const std::string& vert, const std::string& frag)
{
    std::lock_guard<std::mutex> lock(g_shaderMutex);
    g_pendingVert = vert;
    g_pendingFrag = frag;
    g_shaderPending.store(true, std::memory_order_release);
}

static void RequestUniform(const std::string& name, const std::vector<float>& values)
{
    if (name.empty() || values.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_uniformMutex);
    g_pendingUniforms[name] = values;
    g_uniformDirty.store(true, std::memory_order_release);
}

static bool ReadRawfileToString(napi_env env, napi_value jsResMgr, const std::string& path, std::string& out)
{
    NativeResourceManager* resMgr = OH_ResourceManager_InitNativeResourceManager(env, jsResMgr);
    if (!resMgr) {
        SetError("rawfile: init resource manager failed");
        return false;
    }

    RawFile* rawFile = OH_ResourceManager_OpenRawFile(resMgr, path.c_str());
    if (!rawFile) {
        OH_ResourceManager_ReleaseNativeResourceManager(resMgr);
        SetError("rawfile: open failed: " + path);
        return false;
    }

    long size = OH_ResourceManager_GetRawFileSize(rawFile);
    if (size <= 0) {
        OH_ResourceManager_CloseRawFile(rawFile);
        OH_ResourceManager_ReleaseNativeResourceManager(resMgr);
        SetError("rawfile: size invalid: " + path);
        return false;
    }

    out.resize(static_cast<size_t>(size));
    int readBytes = OH_ResourceManager_ReadRawFile(rawFile, out.data(), static_cast<size_t>(size));
    OH_ResourceManager_CloseRawFile(rawFile);
    OH_ResourceManager_ReleaseNativeResourceManager(resMgr);

    if (readBytes <= 0) {
        SetError("rawfile: read failed: " + path);
        out.clear();
        return false;
    }

    if (readBytes < size) {
        out.resize(static_cast<size_t>(readBytes));
    }
    return true;
}

static bool IsKnownPassName(const std::string& name)
{
    if (name == "ShaderPass") {
        return true;
    }
    return IsPassRegistered(name);
}

static void NormalizePassList(std::vector<std::string>& passes)
{
    std::vector<std::string> normalized;
    normalized.reserve(passes.size());
    for (const auto& name : passes) {
        if (!IsKnownPassName(name)) {
            SetError("setPasses: unknown pass " + name);
            continue;
        }
        if (std::find(normalized.begin(), normalized.end(), name) == normalized.end()) {
            normalized.push_back(name);
        }
    }
    passes.swap(normalized);
}

static void RequestPasses(std::vector<std::string> passes)
{
    NormalizePassList(passes);
    std::lock_guard<std::mutex> lock(g_passMutex);
    g_requestedPasses = std::move(passes);
    g_passesDirty.store(true, std::memory_order_release);
}

static void RequestAddPass(const std::string& name)
{
    if (!IsKnownPassName(name)) {
        SetError("addPass: unknown pass " + name);
        return;
    }
    std::lock_guard<std::mutex> lock(g_passMutex);
    if (std::find(g_requestedPasses.begin(), g_requestedPasses.end(), name) == g_requestedPasses.end()) {
        g_requestedPasses.push_back(name);
        g_passesDirty.store(true, std::memory_order_release);
    }
}

static void RequestRemovePass(const std::string& name)
{
    std::lock_guard<std::mutex> lock(g_passMutex);
    auto it = std::remove(g_requestedPasses.begin(), g_requestedPasses.end(), name);
    if (it != g_requestedPasses.end()) {
        g_requestedPasses.erase(it, g_requestedPasses.end());
        g_passesDirty.store(true, std::memory_order_release);
    }
}

static std::vector<std::string> GetRequestedPassesSnapshot()
{
    std::lock_guard<std::mutex> lock(g_passMutex);
    return g_requestedPasses;
}

static std::shared_ptr<RenderPass> CreatePassByName(const std::string& name)
{
    if (name == "ShaderPass") {
        if (!g_customPass) {
            g_customPass = std::make_shared<ShaderPass>();
        }
        return g_customPass;
    }
    return glex::CreatePass(name);
}

static void ApplyRequestedPasses(int width, int height)
{
    std::vector<std::string> passes = GetRequestedPassesSnapshot();

    if (!g_pipeline) {
        g_pipeline = std::make_unique<RenderPipeline>();
    }

    g_pipeline->destroy();

    if (!passes.empty()) {
        if (!g_glContext || g_glContext->getGLESVersionMajor() < 3) {
            GLEX_LOGE("Builtin passes require OpenGL ES 3.0+");
            SetError("Builtin passes require OpenGL ES 3.0+");
            return;
        }
        for (const auto& name : passes) {
            auto pass = CreatePassByName(name);
            if (pass) {
                g_pipeline->addPass(pass);
            } else {
                SetError("createPass failed: " + name);
            }
        }
        if (g_pipeline->getPassCount() > 0 && !g_pipeline->isInitialized()) {
            g_pipeline->initialize(width, height);
            ClearError();
        }
    }
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

static bool GetString(napi_env env, napi_value value, std::string& out)
{
    size_t length = 0;
    if (napi_get_value_string_utf8(env, value, nullptr, 0, &length) != napi_ok) {
        return false;
    }
    std::string str(length + 1, '\0');
    size_t copied = 0;
    if (napi_get_value_string_utf8(env, value, str.data(), str.size(), &copied) != napi_ok) {
        return false;
    }
    str.resize(copied);
    out = str;
    return true;
}

static bool GetFloatArray(napi_env env, napi_value value, std::vector<float>& out)
{
    napi_valuetype type;
    if (napi_typeof(env, value, &type) != napi_ok) return false;

    if (type == napi_number) {
        double num = 0.0;
        if (!GetDouble(env, value, &num)) return false;
        out = { static_cast<float>(num) };
        return true;
    }

    bool isArray = false;
    if (napi_is_array(env, value, &isArray) != napi_ok || !isArray) {
        return false;
    }

    uint32_t len = 0;
    if (napi_get_array_length(env, value, &len) != napi_ok || len == 0) {
        return false;
    }
    out.clear();
    out.reserve(len);
    for (uint32_t i = 0; i < len; i++) {
        napi_value elem;
        if (napi_get_element(env, value, i, &elem) != napi_ok) return false;
        double num = 0.0;
        if (!GetDouble(env, elem, &num)) return false;
        out.push_back(static_cast<float>(num));
    }
    return true;
}

// ============================================================
// 渲染管理
// ============================================================

static void InitializeRenderer(int width, int height)
{
    ApplyRequestedPasses(width, height);
    g_passesDirty.store(false, std::memory_order_relaxed);
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
        static uint64_t lastTouchSeq = 0;
        if (g_passesDirty.exchange(false, std::memory_order_acq_rel)) {
            ApplyRequestedPasses(g_glContext->getWidth(), g_glContext->getHeight());
        }
        if (g_shaderPending.exchange(false, std::memory_order_acq_rel)) {
            std::string vert;
            std::string frag;
            {
                std::lock_guard<std::mutex> lock(g_shaderMutex);
                vert = g_pendingVert;
                frag = g_pendingFrag;
            }
            if (g_glContext && g_glContext->getGLESVersionMajor() >= 3) {
                if (!g_customPass) {
                    g_customPass = std::make_shared<ShaderPass>();
                }
                g_customPass->setShaderSources(vert, frag);
                ClearError();
            } else {
                SetError("Custom shader requires OpenGL ES 3.0+");
            }
        }

        if (g_uniformDirty.exchange(false, std::memory_order_acq_rel)) {
            std::unordered_map<std::string, std::vector<float>> snapshot;
            {
                std::lock_guard<std::mutex> lock(g_uniformMutex);
                snapshot = g_pendingUniforms;
            }
            if (g_customPass) {
                for (const auto& item : snapshot) {
                    g_customPass->setUniform(item.first, item.second);
                }
            }
        }

        if (g_resizePending.exchange(false, std::memory_order_acq_rel)) {
            int rw = g_pendingWidth.load(std::memory_order_relaxed);
            int rh = g_pendingHeight.load(std::memory_order_relaxed);
            if (g_pipeline) {
                g_pipeline->resize(rw, rh);
            }
            if (g_glContext) {
                g_glContext->setSurfaceSize(rw, rh);
            }
        }

        uint64_t seq = g_touchSeq.load(std::memory_order_relaxed);
        if (seq != lastTouchSeq) {
            lastTouchSeq = seq;
            if (g_pipeline) {
                g_pipeline->dispatchTouch(
                    g_touchX.load(std::memory_order_relaxed),
                    g_touchY.load(std::memory_order_relaxed),
                    g_touchAction.load(std::memory_order_relaxed),
                    g_touchPointerId.load(std::memory_order_relaxed)
                );
            }
        }

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
        StopRenderLoopLocked();
        DestroyRenderer();
        if (g_glContext) {
            g_glContext->destroy();
            g_glContext.reset();
        }
        OH_NativeWindow_DestroyNativeWindow(g_nativeWindow);
        g_nativeWindow = nullptr;
    }
    g_resizePending.store(false, std::memory_order_relaxed);
    g_pendingWidth.store(0, std::memory_order_relaxed);
    g_pendingHeight.store(0, std::memory_order_relaxed);

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
        StartRenderLoopLocked();
        g_startRequested.store(false);
    }

    return GetUndefined(env);
}

static napi_value DestroySurface(napi_env env, napi_callback_info info)
{
    (void)info;
    std::lock_guard<std::mutex> lock(g_mutex);

    StopRenderLoopLocked();
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
    g_resizePending.store(false, std::memory_order_relaxed);
    g_pendingWidth.store(0, std::memory_order_relaxed);
    g_pendingHeight.store(0, std::memory_order_relaxed);
    GLEX_LOGI("destroySurface: done");
    return GetUndefined(env);
}

static napi_value NapiStartRender(napi_env env, napi_callback_info info)
{
    (void)info;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_startRequested.store(true);

    if (g_glContext && g_glContext->isInitialized()) {
        StartRenderLoopLocked();
        g_startRequested.store(false);
    } else {
        GLEX_LOGW("startRender: GL not ready, deferred");
    }

    return GetUndefined(env);
}

static napi_value NapiStopRender(napi_env env, napi_callback_info info)
{
    (void)info;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_startRequested.store(false);
    StopRenderLoopLocked();
    return GetUndefined(env);
}

static napi_value NapiResize(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    std::lock_guard<std::mutex> lock(g_mutex);
    if (argc >= 2) {
        int32_t w = 0, h = 0;
        if (GetInt32(env, args[0], &w) && GetInt32(env, args[1], &h)) {
            RequestResize(w, h);
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

        g_bgColorR.store(static_cast<float>(r), std::memory_order_relaxed);
        g_bgColorG.store(static_cast<float>(g), std::memory_order_relaxed);
        g_bgColorB.store(static_cast<float>(b), std::memory_order_relaxed);
        g_bgColorA.store(static_cast<float>(a), std::memory_order_relaxed);
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
            g_targetFPS.store(fps, std::memory_order_relaxed);
            if (g_renderThread) {
                g_renderThread->setTargetFPS(fps);
            }
        }
    }
    return GetUndefined(env);
}

static napi_value SetShaderSources(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 2) {
        SetError("setShaderSources: missing parameters");
        return GetUndefined(env);
    }

    std::string vert;
    std::string frag;
    if (!GetString(env, args[0], vert) || !GetString(env, args[1], frag)) {
        SetError("setShaderSources: invalid parameters");
        return GetUndefined(env);
    }

    RequestPasses({ "ShaderPass" });
    RequestShaderUpdate(vert, frag);
    return GetUndefined(env);
}

static napi_value SetUniform(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 2) {
        SetError("setUniform: missing parameters");
        return GetUndefined(env);
    }

    std::string name;
    if (!GetString(env, args[0], name)) {
        SetError("setUniform: invalid name");
        return GetUndefined(env);
    }

    std::vector<float> values;
    if (!GetFloatArray(env, args[1], values)) {
        SetError("setUniform: invalid value");
        return GetUndefined(env);
    }

    RequestUniform(name, values);
    return GetUndefined(env);
}

static napi_value LoadShaderFromRawfile(napi_env env, napi_callback_info info)
{
    size_t argc = 3;
    napi_value args[3];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 3) {
        SetError("loadShaderFromRawfile: missing parameters");
        return GetUndefined(env);
    }

    std::string vertPath;
    std::string fragPath;
    if (!GetString(env, args[1], vertPath) || !GetString(env, args[2], fragPath)) {
        SetError("loadShaderFromRawfile: invalid paths");
        return GetUndefined(env);
    }

    std::string vert;
    std::string frag;
    if (!ReadRawfileToString(env, args[0], vertPath, vert) ||
        !ReadRawfileToString(env, args[0], fragPath, frag)) {
        return GetUndefined(env);
    }

    RequestPasses({ "ShaderPass" });
    RequestShaderUpdate(vert, frag);
    return GetUndefined(env);
}

static napi_value SetPasses(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        SetError("setPasses: missing parameters");
        return GetUndefined(env);
    }

    bool isArray = false;
    if (napi_is_array(env, args[0], &isArray) != napi_ok || !isArray) {
        SetError("setPasses: invalid parameters");
        return GetUndefined(env);
    }

    uint32_t len = 0;
    if (napi_get_array_length(env, args[0], &len) != napi_ok) {
        SetError("setPasses: invalid parameters");
        return GetUndefined(env);
    }

    std::vector<std::string> passes;
    passes.reserve(len);
    for (uint32_t i = 0; i < len; i++) {
        napi_value elem;
        if (napi_get_element(env, args[0], i, &elem) != napi_ok) {
            SetError("setPasses: invalid parameters");
            return GetUndefined(env);
        }
        std::string name;
        if (!GetString(env, elem, name)) {
            SetError("setPasses: invalid pass name");
            return GetUndefined(env);
        }
        passes.push_back(name);
    }

    RequestPasses(std::move(passes));
    return GetUndefined(env);
}

static napi_value AddPass(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        SetError("addPass: missing parameters");
        return GetUndefined(env);
    }

    std::string name;
    if (!GetString(env, args[0], name)) {
        SetError("addPass: invalid pass name");
        return GetUndefined(env);
    }

    RequestAddPass(name);
    return GetUndefined(env);
}

static napi_value RemovePass(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 1) {
        SetError("removePass: missing parameters");
        return GetUndefined(env);
    }

    std::string name;
    if (!GetString(env, args[0], name)) {
        SetError("removePass: invalid pass name");
        return GetUndefined(env);
    }

    RequestRemovePass(name);
    return GetUndefined(env);
}

static napi_value GetPasses(napi_env env, napi_callback_info info)
{
    (void)info;
    std::vector<std::string> passes = GetRequestedPassesSnapshot();
    napi_value result;
    napi_create_array_with_length(env, passes.size(), &result);
    for (size_t i = 0; i < passes.size(); i++) {
        napi_value v;
        napi_create_string_utf8(env, passes[i].c_str(), NAPI_AUTO_LENGTH, &v);
        napi_set_element(env, result, static_cast<uint32_t>(i), v);
    }
    return result;
}

static napi_value SetTouchEvent(napi_env env, napi_callback_info info)
{
    size_t argc = 4;
    napi_value args[4];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    if (argc < 3) {
        SetError("setTouchEvent: missing parameters");
        return GetUndefined(env);
    }

    double x = 0.0;
    double y = 0.0;
    int32_t action = 0;
    int32_t pointerId = 0;
    if (!GetDouble(env, args[0], &x) || !GetDouble(env, args[1], &y) || !GetInt32(env, args[2], &action)) {
        SetError("setTouchEvent: invalid parameters");
        return GetUndefined(env);
    }
    if (argc >= 4) {
        GetInt32(env, args[3], &pointerId);
    }

    if (!std::isfinite(x) || !std::isfinite(y)) {
        SetError("setTouchEvent: non-finite coordinates");
        return GetUndefined(env);
    }

    g_touchX.store(static_cast<float>(x), std::memory_order_relaxed);
    g_touchY.store(static_cast<float>(y), std::memory_order_relaxed);
    g_touchAction.store(action, std::memory_order_relaxed);
    g_touchPointerId.store(pointerId, std::memory_order_relaxed);
    g_touchSeq.fetch_add(1, std::memory_order_relaxed);
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
        napi_value w, h;
        napi_create_int32(env, 0, &w);
        napi_create_int32(env, 0, &h);
        napi_set_named_property(env, result, "width", w);
        napi_set_named_property(env, result, "height", h);
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
        StopRenderLoopLocked();
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
        StartRenderLoopLocked();
        g_startRequested.store(false);
    }
}

static void OnSurfaceChanged(OH_NativeXComponent* component, void* window)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    uint64_t w = 0, h = 0;
    OH_NativeXComponent_GetXComponentSize(component, window, &w, &h);
    GLEX_LOGI("XComponent: OnSurfaceChanged %{public}dx%{public}d",
              static_cast<int>(w), static_cast<int>(h));

    RequestResize(static_cast<int>(w), static_cast<int>(h));
}

static void OnSurfaceDestroyed(OH_NativeXComponent* component, void* window)
{
    GLEX_LOGI("XComponent: OnSurfaceDestroyed");
    std::lock_guard<std::mutex> lock(g_mutex);

    StopRenderLoopLocked();
    DestroyRenderer();

    if (g_glContext) {
        g_glContext->destroy();
        g_glContext.reset();
    }

    g_nativeWindow = nullptr;
    g_surfaceId = 0;
    g_startRequested.store(false);
    g_resizePending.store(false, std::memory_order_relaxed);
    g_pendingWidth.store(0, std::memory_order_relaxed);
    g_pendingHeight.store(0, std::memory_order_relaxed);
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
    RegisterBuiltinPasses();
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
        { "setShaderSources",   nullptr, SetShaderSources,   nullptr, nullptr, nullptr, napi_default, nullptr },
        { "loadShaderFromRawfile", nullptr, LoadShaderFromRawfile, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setUniform",         nullptr, SetUniform,         nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setPasses",          nullptr, SetPasses,          nullptr, nullptr, nullptr, napi_default, nullptr },
        { "addPass",            nullptr, AddPass,            nullptr, nullptr, nullptr, napi_default, nullptr },
        { "removePass",         nullptr, RemovePass,         nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getPasses",          nullptr, GetPasses,          nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setTouchEvent",      nullptr, SetTouchEvent,      nullptr, nullptr, nullptr, napi_default, nullptr },
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
