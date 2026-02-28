
#include "bridge/GLEXBridge.h"

#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <future>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <GLES3/gl3.h>
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <native_window/external_window.h>
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>
#include <sys/mman.h>
#include <unistd.h>

#include "glex/GLEX.h"
#include "glex/GLResourceTracker.h"
#include "ShaderPass.h"
#include "BuiltinPassRegistry.h"
#include "glex/PassRegistry.h"

namespace glex {
namespace bridge {

class GLEXEngine;

struct PendingSurface {
    OHNativeWindow* window = nullptr;
    uint64_t width = 0;
    uint64_t height = 0;
    bool hasSize = false;
};

struct EngineRegistry {
    std::mutex mutex;
    std::unordered_map<std::string, GLEXEngine*> engines;
    std::unordered_map<std::string, PendingSurface> pending;
};

static EngineRegistry& GetRegistry()
{
    static EngineRegistry registry;
    return registry;
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

static std::string GetXComponentId(OH_NativeXComponent* component)
{
    if (!component) {
        return {};
    }
    char idBuffer[OH_XCOMPONENT_ID_LEN_MAX + 1] = { 0 };
    uint64_t size = OH_XCOMPONENT_ID_LEN_MAX;
    if (OH_NativeXComponent_GetXComponentId(component, idBuffer, &size) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        return {};
    }
    if (size > OH_XCOMPONENT_ID_LEN_MAX) {
        size = OH_XCOMPONENT_ID_LEN_MAX;
    }
    if (size > 0 && idBuffer[size - 1] == '\0') {
        size -= 1;
    }
    return std::string(idBuffer, idBuffer + size);
}

struct MappedRawfile {
    void* map = nullptr;
    size_t length = 0;
};

static void FinalizeMappedRawfile(napi_env env, void* data, void* hint)
{
    (void)env;
    (void)data;
    auto* info = reinterpret_cast<MappedRawfile*>(hint);
    if (info) {
        if (info->map && info->length > 0) {
            munmap(info->map, info->length);
        }
        delete info;
    }
}

// ============================================================
// 渲染实例
// ============================================================

class GLEXEngine {
public:
    explicit GLEXEngine(napi_env env) : env_(env) {}
    ~GLEXEngine();

    void BindXComponentId(const std::string& id);
    void UnbindXComponentId();

    void HandleSurfaceCreated(OHNativeWindow* window);
    void HandleSurfaceChanged(uint64_t width, uint64_t height);
    void HandleSurfaceDestroyed();

    static napi_value NapiNew(napi_env env, napi_callback_info info);
    static void NapiFinalize(napi_env env, void* data, void* hint);

    static napi_value NapiBindXComponent(napi_env env, napi_callback_info info);
    static napi_value NapiUnbindXComponent(napi_env env, napi_callback_info info);

    static napi_value NapiSetSurfaceId(napi_env env, napi_callback_info info);
    static napi_value NapiDestroySurface(napi_env env, napi_callback_info info);
    static napi_value NapiStartRender(napi_env env, napi_callback_info info);
    static napi_value NapiStopRender(napi_env env, napi_callback_info info);
    static napi_value NapiResize(napi_env env, napi_callback_info info);

    static napi_value NapiSetBackgroundColor(napi_env env, napi_callback_info info);
    static napi_value NapiSetTargetFPS(napi_env env, napi_callback_info info);
    static napi_value NapiSetShaderSources(napi_env env, napi_callback_info info);
    static napi_value NapiLoadShaderFromRawfile(napi_env env, napi_callback_info info);
    static napi_value NapiLoadRawfileBytes(napi_env env, napi_callback_info info);
    static napi_value NapiSetUniform(napi_env env, napi_callback_info info);
    static napi_value NapiSetPasses(napi_env env, napi_callback_info info);
    static napi_value NapiAddPass(napi_env env, napi_callback_info info);
    static napi_value NapiRemovePass(napi_env env, napi_callback_info info);
    static napi_value NapiGetPasses(napi_env env, napi_callback_info info);
    static napi_value NapiSetTouchEvent(napi_env env, napi_callback_info info);

    static napi_value NapiGetCurrentFPS(napi_env env, napi_callback_info info);
    static napi_value NapiGetGLInfo(napi_env env, napi_callback_info info);
    static napi_value NapiGetGpuStats(napi_env env, napi_callback_info info);
    static napi_value NapiGetLastError(napi_env env, napi_callback_info info);
    static napi_value NapiClearLastError(napi_env env, napi_callback_info info);

private:
    void SetError(const std::string& msg);
    void ClearError();
    std::string GetError();

    void RequestResize(int width, int height);
    void RequestShaderUpdate(const std::string& vert, const std::string& frag);
    void RequestUniform(const std::string& name, const std::vector<float>& values);
    bool ReadRawfileToString(napi_env env, napi_value jsResMgr, const std::string& path, std::string& out);
    bool ReadRawfileToBytes(napi_env env, napi_value jsResMgr, const std::string& path, std::vector<uint8_t>& out);

    bool IsKnownPassName(const std::string& name);
    void NormalizePassList(std::vector<std::string>& passes);
    void RequestPasses(std::vector<std::string> passes);
    void RequestAddPass(const std::string& name);
    void RequestRemovePass(const std::string& name);
    std::vector<std::string> GetRequestedPassesSnapshot();
    std::shared_ptr<RenderPass> CreatePassByName(const std::string& name);
    void ApplyRequestedPasses(int width, int height);

    void InitializeRenderer(int width, int height);
    void DestroyRenderer();
    void StartRenderLoopLocked();
    void StopRenderLoopLocked();
    void DestroySurfaceLocked(bool keepStartRequested);
    bool RunOnRenderThreadSync(std::function<void()> task);

    napi_env env_;

    std::mutex mutex_;
    std::mutex errorMutex_;
    OHNativeWindow* nativeWindow_ = nullptr;
    bool ownsWindow_ = false;
    uint64_t surfaceId_ = 0;
    std::atomic<bool> startRequested_{false};

    std::unique_ptr<GLContext> glContext_;
    std::unique_ptr<RenderThread> renderThread_;
    std::unique_ptr<RenderPipeline> pipeline_;

    std::atomic<float> bgColorR_{0.02f};
    std::atomic<float> bgColorG_{0.03f};
    std::atomic<float> bgColorB_{0.10f};
    std::atomic<float> bgColorA_{1.0f};
    std::atomic<int> targetFPS_{60};
    std::atomic<int> pendingWidth_{0};
    std::atomic<int> pendingHeight_{0};
    std::atomic<bool> resizePending_{false};

    std::string lastError_;
    std::mutex shaderMutex_;
    std::string pendingVert_;
    std::string pendingFrag_;
    std::atomic<bool> shaderPending_{false};
    std::shared_ptr<ShaderPass> customPass_;
    std::mutex uniformMutex_;
    std::unordered_map<std::string, std::vector<float>> pendingUniforms_;
    std::atomic<bool> uniformDirty_{false};

    std::atomic<float> touchX_{0.0f};
    std::atomic<float> touchY_{0.0f};
    std::atomic<int> touchAction_{0};
    std::atomic<int> touchPointerId_{0};
    std::atomic<uint64_t> touchSeq_{0};
    uint64_t lastAppliedTouchSeq_ = 0;

    std::mutex passMutex_;
    std::vector<std::string> requestedPasses_{ "DemoPass" };
    std::atomic<bool> passesDirty_{false};

    std::string xcomponentId_;
};

GLEXEngine::~GLEXEngine()
{
    UnbindXComponentId();
    std::lock_guard<std::mutex> lock(mutex_);
    DestroyRenderer();
    StopRenderLoopLocked();
    if (glContext_) {
        glContext_->destroy();
        glContext_.reset();
    }
    if (nativeWindow_ != nullptr && ownsWindow_) {
        OH_NativeWindow_DestroyNativeWindow(nativeWindow_);
    }
    nativeWindow_ = nullptr;
    ownsWindow_ = false;
}

void GLEXEngine::BindXComponentId(const std::string& id)
{
    if (id.empty()) {
        SetError("bindXComponent: invalid id");
        return;
    }

    PendingSurface pending;
    {
        auto& registry = GetRegistry();
        std::lock_guard<std::mutex> lock(registry.mutex);
        if (!xcomponentId_.empty()) {
            registry.engines.erase(xcomponentId_);
        }
        xcomponentId_ = id;
        registry.engines[id] = this;
        auto it = registry.pending.find(id);
        if (it != registry.pending.end()) {
            pending = it->second;
            registry.pending.erase(it);
        }
    }

    if (pending.window) {
        HandleSurfaceCreated(pending.window);
        if (pending.hasSize) {
            HandleSurfaceChanged(pending.width, pending.height);
        }
    }
}

void GLEXEngine::UnbindXComponentId()
{
    if (xcomponentId_.empty()) {
        return;
    }
    auto& registry = GetRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    registry.engines.erase(xcomponentId_);
    xcomponentId_.clear();
}

void GLEXEngine::HandleSurfaceCreated(OHNativeWindow* window)
{
    GLEX_LOGI("XComponent: OnSurfaceCreated");
    std::lock_guard<std::mutex> lock(mutex_);

    DestroySurfaceLocked(true);
    ownsWindow_ = false;
    nativeWindow_ = window;

    glContext_ = std::make_unique<GLContext>();
    if (!glContext_->initialize(reinterpret_cast<EGLNativeWindowType>(window))) {
        GLEX_LOGE("XComponent: GL init failed");
        SetError("XComponent: GL init failed");
        glContext_.reset();
        return;
    }

    InitializeRenderer(glContext_->getWidth(), glContext_->getHeight());

    glContext_->clearCurrent();

    if (startRequested_.load()) {
        StartRenderLoopLocked();
        startRequested_.store(false);
    }
}

void GLEXEngine::HandleSurfaceChanged(uint64_t width, uint64_t height)
{
    std::lock_guard<std::mutex> lock(mutex_);
    GLEX_LOGI("XComponent: OnSurfaceChanged %{public}dx%{public}d",
              static_cast<int>(width), static_cast<int>(height));
    RequestResize(static_cast<int>(width), static_cast<int>(height));
}

void GLEXEngine::HandleSurfaceDestroyed()
{
    GLEX_LOGI("XComponent: OnSurfaceDestroyed");
    std::lock_guard<std::mutex> lock(mutex_);
    DestroyRenderer();
    StopRenderLoopLocked();
    if (glContext_) {
        glContext_->destroy();
        glContext_.reset();
    }
    nativeWindow_ = nullptr;
    ownsWindow_ = false;
    surfaceId_ = 0;
    startRequested_.store(false);
    resizePending_.store(false, std::memory_order_relaxed);
    pendingWidth_.store(0, std::memory_order_relaxed);
    pendingHeight_.store(0, std::memory_order_relaxed);
}

void GLEXEngine::SetError(const std::string& msg)
{
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = msg;
}

void GLEXEngine::ClearError()
{
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.clear();
}

std::string GLEXEngine::GetError()
{
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}
void GLEXEngine::RequestResize(int width, int height)
{
    pendingWidth_.store(width, std::memory_order_relaxed);
    pendingHeight_.store(height, std::memory_order_relaxed);
    resizePending_.store(true, std::memory_order_release);
    if (glContext_) {
        glContext_->setSurfaceSize(width, height);
    }
}

void GLEXEngine::RequestShaderUpdate(const std::string& vert, const std::string& frag)
{
    std::lock_guard<std::mutex> lock(shaderMutex_);
    pendingVert_ = vert;
    pendingFrag_ = frag;
    shaderPending_.store(true, std::memory_order_release);
}

void GLEXEngine::RequestUniform(const std::string& name, const std::vector<float>& values)
{
    if (name.empty() || values.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(uniformMutex_);
    pendingUniforms_[name] = values;
    uniformDirty_.store(true, std::memory_order_release);
}

bool GLEXEngine::ReadRawfileToString(napi_env env, napi_value jsResMgr, const std::string& path, std::string& out)
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

bool GLEXEngine::ReadRawfileToBytes(napi_env env, napi_value jsResMgr, const std::string& path,
                                    std::vector<uint8_t>& out)
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

bool GLEXEngine::IsKnownPassName(const std::string& name)
{
    if (name == "ShaderPass") {
        return true;
    }
    return IsPassRegistered(name);
}

void GLEXEngine::NormalizePassList(std::vector<std::string>& passes)
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

void GLEXEngine::RequestPasses(std::vector<std::string> passes)
{
    NormalizePassList(passes);
    std::lock_guard<std::mutex> lock(passMutex_);
    requestedPasses_ = std::move(passes);
    passesDirty_.store(true, std::memory_order_release);
}

void GLEXEngine::RequestAddPass(const std::string& name)
{
    if (!IsKnownPassName(name)) {
        SetError("addPass: unknown pass " + name);
        return;
    }
    std::lock_guard<std::mutex> lock(passMutex_);
    if (std::find(requestedPasses_.begin(), requestedPasses_.end(), name) == requestedPasses_.end()) {
        requestedPasses_.push_back(name);
        passesDirty_.store(true, std::memory_order_release);
    }
}

void GLEXEngine::RequestRemovePass(const std::string& name)
{
    std::lock_guard<std::mutex> lock(passMutex_);
    auto it = std::remove(requestedPasses_.begin(), requestedPasses_.end(), name);
    if (it != requestedPasses_.end()) {
        requestedPasses_.erase(it, requestedPasses_.end());
        passesDirty_.store(true, std::memory_order_release);
    }
}

std::vector<std::string> GLEXEngine::GetRequestedPassesSnapshot()
{
    std::lock_guard<std::mutex> lock(passMutex_);
    return requestedPasses_;
}

std::shared_ptr<RenderPass> GLEXEngine::CreatePassByName(const std::string& name)
{
    if (name == "ShaderPass") {
        if (!customPass_) {
            customPass_ = std::make_shared<ShaderPass>();
        }
        return customPass_;
    }
    return glex::CreatePass(name);
}

void GLEXEngine::ApplyRequestedPasses(int width, int height)
{
    std::vector<std::string> passes = GetRequestedPassesSnapshot();

    if (!pipeline_) {
        pipeline_ = std::make_unique<RenderPipeline>();
    }

    pipeline_->destroy();

    if (!passes.empty()) {
        if (!glContext_ || glContext_->getGLESVersionMajor() < 3) {
            GLEX_LOGE("Builtin passes require OpenGL ES 3.0+");
            SetError("Builtin passes require OpenGL ES 3.0+");
            return;
        }
        for (const auto& name : passes) {
            auto pass = CreatePassByName(name);
            if (pass) {
                pipeline_->addPass(pass);
            } else {
                SetError("createPass failed: " + name);
            }
        }
        if (pipeline_->getPassCount() > 0 && !pipeline_->isInitialized()) {
            pipeline_->initialize(width, height);
            ClearError();
        }
    }
}

void GLEXEngine::InitializeRenderer(int width, int height)
{
    RequestResize(width, height);
    passesDirty_.store(true, std::memory_order_release);
}

void GLEXEngine::DestroyRenderer()
{
    if (!pipeline_) {
        return;
    }
    if (renderThread_ && renderThread_->isRunning()) {
        RunOnRenderThreadSync([this]() {
            if (pipeline_) {
                pipeline_->destroy();
            }
        });
    }
    pipeline_.reset();
}

void GLEXEngine::StartRenderLoopLocked()
{
    if (!glContext_ || !glContext_->isInitialized()) return;
    if (renderThread_ && renderThread_->isRunning()) return;

    glContext_->clearCurrent();

    if (!renderThread_) {
        renderThread_ = std::make_unique<RenderThread>();
    }

    lastAppliedTouchSeq_ = 0;
    renderThread_->setTargetFPS(targetFPS_.load(std::memory_order_relaxed));
    renderThread_->start(glContext_.get(), [this](float deltaTime) {
        if (passesDirty_.exchange(false, std::memory_order_acq_rel)) {
            ApplyRequestedPasses(glContext_->getWidth(), glContext_->getHeight());
        }
        if (shaderPending_.exchange(false, std::memory_order_acq_rel)) {
            std::string vert;
            std::string frag;
            {
                std::lock_guard<std::mutex> lock(shaderMutex_);
                vert = pendingVert_;
                frag = pendingFrag_;
            }
            if (glContext_ && glContext_->getGLESVersionMajor() >= 3) {
                if (!customPass_) {
                    customPass_ = std::make_shared<ShaderPass>();
                }
                customPass_->setShaderSources(vert, frag);
                ClearError();
            } else {
                SetError("Custom shader requires OpenGL ES 3.0+");
            }
        }

        if (uniformDirty_.exchange(false, std::memory_order_acq_rel)) {
            std::unordered_map<std::string, std::vector<float>> snapshot;
            {
                std::lock_guard<std::mutex> lock(uniformMutex_);
                snapshot = pendingUniforms_;
            }
            if (customPass_) {
                for (const auto& item : snapshot) {
                    customPass_->setUniform(item.first, item.second);
                }
            }
        }

        if (resizePending_.exchange(false, std::memory_order_acq_rel)) {
            int rw = pendingWidth_.load(std::memory_order_relaxed);
            int rh = pendingHeight_.load(std::memory_order_relaxed);
            if (pipeline_) {
                pipeline_->resize(rw, rh);
            }
            if (glContext_) {
                glContext_->setSurfaceSize(rw, rh);
            }
        }

        uint64_t seq = touchSeq_.load(std::memory_order_relaxed);
        if (seq != lastAppliedTouchSeq_) {
            lastAppliedTouchSeq_ = seq;
            if (pipeline_) {
                pipeline_->dispatchTouch(
                    touchX_.load(std::memory_order_relaxed),
                    touchY_.load(std::memory_order_relaxed),
                    touchAction_.load(std::memory_order_relaxed),
                    touchPointerId_.load(std::memory_order_relaxed)
                );
            }
        }

        int w = glContext_->getWidth();
        int h = glContext_->getHeight();

        glViewport(0, 0, w, h);
        glClearColor(
            bgColorR_.load(std::memory_order_relaxed),
            bgColorG_.load(std::memory_order_relaxed),
            bgColorB_.load(std::memory_order_relaxed),
            bgColorA_.load(std::memory_order_relaxed)
        );
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (pipeline_) {
            pipeline_->update(deltaTime);
            pipeline_->render();
        }
    });
}

void GLEXEngine::StopRenderLoopLocked()
{
    if (renderThread_) {
        renderThread_->stop();
    }
}

void GLEXEngine::DestroySurfaceLocked(bool keepStartRequested)
{
    bool pendingStart = startRequested_.load();
    DestroyRenderer();
    StopRenderLoopLocked();

    if (glContext_) {
        glContext_->destroy();
        glContext_.reset();
    }

    if (nativeWindow_ != nullptr && ownsWindow_) {
        OH_NativeWindow_DestroyNativeWindow(nativeWindow_);
    }

    nativeWindow_ = nullptr;
    ownsWindow_ = false;
    surfaceId_ = 0;
    if (keepStartRequested) {
        startRequested_.store(pendingStart);
    } else {
        startRequested_.store(false);
    }
    resizePending_.store(false, std::memory_order_relaxed);
    pendingWidth_.store(0, std::memory_order_relaxed);
    pendingHeight_.store(0, std::memory_order_relaxed);
}

bool GLEXEngine::RunOnRenderThreadSync(std::function<void()> task)
{
    if (!renderThread_ || !renderThread_->isRunning() || !task) {
        return false;
    }
    auto done = std::make_shared<std::promise<void>>();
    auto future = done->get_future();
    renderThread_->post([task = std::move(task), done]() mutable {
        task();
        done->set_value();
    });
    future.wait();
    return true;
}
// ============================================================
// NAPI 接口实现
// ============================================================

napi_value GLEXEngine::NapiNew(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    napi_value thisArg;
    napi_get_cb_info(env, info, &argc, nullptr, &thisArg, nullptr);

    auto* engine = new (std::nothrow) GLEXEngine(env);
    if (!engine) {
        return GetUndefined(env);
    }
    if (napi_wrap(env, thisArg, engine, GLEXEngine::NapiFinalize, nullptr, nullptr) != napi_ok) {
        delete engine;
        return GetUndefined(env);
    }
    return thisArg;
}

void GLEXEngine::NapiFinalize(napi_env env, void* data, void* hint)
{
    (void)env;
    (void)hint;
    auto* engine = reinterpret_cast<GLEXEngine*>(data);
    delete engine;
}

static GLEXEngine* UnwrapEngine(napi_env env, napi_callback_info info, size_t* argc,
                                napi_value* args, size_t maxArgs)
{
    napi_value thisArg;
    size_t actual = maxArgs;
    if (napi_get_cb_info(env, info, &actual, args, &thisArg, nullptr) != napi_ok) {
        return nullptr;
    }
    if (argc) {
        *argc = actual;
    }
    GLEXEngine* engine = nullptr;
    if (napi_unwrap(env, thisArg, reinterpret_cast<void**>(&engine)) != napi_ok) {
        return nullptr;
    }
    return engine;
}

napi_value GLEXEngine::NapiBindXComponent(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 1);
    if (!engine) return GetUndefined(env);

    if (argc < 1) {
        engine->SetError("bindXComponent: missing id");
        return GetUndefined(env);
    }

    std::string id;
    if (!GetString(env, args[0], id) || id.empty()) {
        engine->SetError("bindXComponent: invalid id");
        return GetUndefined(env);
    }

    engine->BindXComponentId(id);
    return GetUndefined(env);
}

napi_value GLEXEngine::NapiUnbindXComponent(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    napi_value args[1];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 0);
    if (!engine) return GetUndefined(env);

    engine->UnbindXComponentId();
    return GetUndefined(env);
}

napi_value GLEXEngine::NapiSetSurfaceId(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 1);
    if (!engine) return GetUndefined(env);

    if (argc < 1) return GetUndefined(env);

    uint64_t surfaceId = 0;
    if (!GetSurfaceId(env, args[0], &surfaceId)) {
        GLEX_LOGE("setSurfaceId: invalid surface id");
        engine->SetError("setSurfaceId: invalid surface id");
        return GetUndefined(env);
    }

    std::lock_guard<std::mutex> lock(engine->mutex_);

    if (engine->surfaceId_ == surfaceId && engine->nativeWindow_ != nullptr) {
        return GetUndefined(env);
    }

    engine->DestroySurfaceLocked(true);

    engine->surfaceId_ = surfaceId;

    OHNativeWindow* window = nullptr;
    int32_t result = OH_NativeWindow_CreateNativeWindowFromSurfaceId(surfaceId, &window);
    if (result != 0 || window == nullptr) {
        GLEX_LOGE("setSurfaceId: create native window failed, result=%{public}d", result);
        engine->SetError("setSurfaceId: create native window failed");
        return GetUndefined(env);
    }
    engine->nativeWindow_ = window;
    engine->ownsWindow_ = true;

    engine->glContext_ = std::make_unique<GLContext>();
    if (!engine->glContext_->initialize(reinterpret_cast<EGLNativeWindowType>(window))) {
        GLEX_LOGE("setSurfaceId: GL init failed");
        engine->SetError("setSurfaceId: GL init failed");
        engine->glContext_.reset();
        return GetUndefined(env);
    }

    engine->InitializeRenderer(engine->glContext_->getWidth(), engine->glContext_->getHeight());

    engine->glContext_->clearCurrent();

    GLEX_LOGI("setSurfaceId: ok, id=%" PRIu64 ", size=%{public}dx%{public}d",
              surfaceId, engine->glContext_->getWidth(), engine->glContext_->getHeight());

    if (engine->startRequested_.load()) {
        engine->StartRenderLoopLocked();
        engine->startRequested_.store(false);
    }

    return GetUndefined(env);
}

napi_value GLEXEngine::NapiDestroySurface(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    napi_value args[1];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 0);
    if (!engine) return GetUndefined(env);

    std::lock_guard<std::mutex> lock(engine->mutex_);
    engine->DestroySurfaceLocked(false);
    GLEX_LOGI("destroySurface: done");
    return GetUndefined(env);
}

napi_value GLEXEngine::NapiStartRender(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    napi_value args[1];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 0);
    if (!engine) return GetUndefined(env);

    std::lock_guard<std::mutex> lock(engine->mutex_);
    engine->startRequested_.store(true);

    if (engine->glContext_ && engine->glContext_->isInitialized()) {
        engine->StartRenderLoopLocked();
        engine->startRequested_.store(false);
    } else {
        GLEX_LOGW("startRender: GL not ready, deferred");
    }

    return GetUndefined(env);
}

napi_value GLEXEngine::NapiStopRender(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    napi_value args[1];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 0);
    if (!engine) return GetUndefined(env);

    std::lock_guard<std::mutex> lock(engine->mutex_);
    engine->startRequested_.store(false);
    engine->StopRenderLoopLocked();
    return GetUndefined(env);
}

napi_value GLEXEngine::NapiResize(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 2);
    if (!engine) return GetUndefined(env);

    std::lock_guard<std::mutex> lock(engine->mutex_);
    if (argc >= 2) {
        int32_t w = 0, h = 0;
        if (GetInt32(env, args[0], &w) && GetInt32(env, args[1], &h)) {
            engine->RequestResize(w, h);
            GLEX_LOGI("resize: %{public}dx%{public}d", w, h);
        }
    }
    return GetUndefined(env);
}
napi_value GLEXEngine::NapiSetBackgroundColor(napi_env env, napi_callback_info info)
{
    size_t argc = 4;
    napi_value args[4];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 4);
    if (!engine) return GetUndefined(env);

    if (argc >= 3) {
        double r = 0, g = 0, b = 0, a = 1.0;
        GetDouble(env, args[0], &r);
        GetDouble(env, args[1], &g);
        GetDouble(env, args[2], &b);
        if (argc >= 4) GetDouble(env, args[3], &a);

        engine->bgColorR_.store(static_cast<float>(r), std::memory_order_relaxed);
        engine->bgColorG_.store(static_cast<float>(g), std::memory_order_relaxed);
        engine->bgColorB_.store(static_cast<float>(b), std::memory_order_relaxed);
        engine->bgColorA_.store(static_cast<float>(a), std::memory_order_relaxed);
    }
    return GetUndefined(env);
}

napi_value GLEXEngine::NapiSetTargetFPS(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 1);
    if (!engine) return GetUndefined(env);

    if (argc >= 1) {
        int32_t fps = 60;
        if (GetInt32(env, args[0], &fps)) {
            engine->targetFPS_.store(fps, std::memory_order_relaxed);
            if (engine->renderThread_) {
                engine->renderThread_->setTargetFPS(fps);
            }
        }
    }
    return GetUndefined(env);
}

napi_value GLEXEngine::NapiSetShaderSources(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 2);
    if (!engine) return GetUndefined(env);

    if (argc < 2) {
        engine->SetError("setShaderSources: missing parameters");
        return GetUndefined(env);
    }

    std::string vert;
    std::string frag;
    if (!GetString(env, args[0], vert) || !GetString(env, args[1], frag)) {
        engine->SetError("setShaderSources: invalid parameters");
        return GetUndefined(env);
    }

    engine->RequestPasses({ "ShaderPass" });
    engine->RequestShaderUpdate(vert, frag);
    return GetUndefined(env);
}

napi_value GLEXEngine::NapiLoadShaderFromRawfile(napi_env env, napi_callback_info info)
{
    size_t argc = 3;
    napi_value args[3];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 3);
    if (!engine) return GetUndefined(env);

    if (argc < 3) {
        engine->SetError("loadShaderFromRawfile: missing parameters");
        return GetUndefined(env);
    }

    std::string vertPath;
    std::string fragPath;
    if (!GetString(env, args[1], vertPath) || !GetString(env, args[2], fragPath)) {
        engine->SetError("loadShaderFromRawfile: invalid paths");
        return GetUndefined(env);
    }

    std::string vert;
    std::string frag;
    if (!engine->ReadRawfileToString(env, args[0], vertPath, vert) ||
        !engine->ReadRawfileToString(env, args[0], fragPath, frag)) {
        return GetUndefined(env);
    }

    engine->RequestPasses({ "ShaderPass" });
    engine->RequestShaderUpdate(vert, frag);
    return GetUndefined(env);
}

napi_value GLEXEngine::NapiLoadRawfileBytes(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 2);
    if (!engine) return GetUndefined(env);

    if (argc < 2) {
        engine->SetError("loadRawfileBytes: missing parameters");
        return GetUndefined(env);
    }

    std::string path;
    if (!GetString(env, args[1], path)) {
        engine->SetError("loadRawfileBytes: invalid path");
        return GetUndefined(env);
    }

    NativeResourceManager* resMgr = OH_ResourceManager_InitNativeResourceManager(env, args[0]);
    if (!resMgr) {
        engine->SetError("rawfile: init resource manager failed");
        return GetUndefined(env);
    }

    RawFile* rawFile = OH_ResourceManager_OpenRawFile(resMgr, path.c_str());
    if (!rawFile) {
        OH_ResourceManager_ReleaseNativeResourceManager(resMgr);
        engine->SetError("rawfile: open failed: " + path);
        return GetUndefined(env);
    }

    RawFileDescriptor desc;
    bool gotDesc = OH_ResourceManager_GetRawFileDescriptorData(rawFile, &desc);
    if (gotDesc && desc.length > 0 && desc.fd >= 0) {
        long pageSize = sysconf(_SC_PAGESIZE);
        long aligned = desc.start & ~(pageSize - 1);
        size_t delta = static_cast<size_t>(desc.start - aligned);
        size_t mapLen = static_cast<size_t>(desc.length) + delta;
        void* map = mmap(nullptr, mapLen, PROT_READ, MAP_PRIVATE, desc.fd, static_cast<off_t>(aligned));
        if (map != MAP_FAILED) {
            void* data = static_cast<uint8_t*>(map) + delta;
            auto* info = new MappedRawfile{map, mapLen};
            napi_value arraybuffer;
            napi_create_external_arraybuffer(env, data, static_cast<size_t>(desc.length),
                                             FinalizeMappedRawfile, info, &arraybuffer);
            OH_ResourceManager_ReleaseRawFileDescriptorData(&desc);
            OH_ResourceManager_CloseRawFile(rawFile);
            OH_ResourceManager_ReleaseNativeResourceManager(resMgr);
            return arraybuffer;
        }
        OH_ResourceManager_ReleaseRawFileDescriptorData(&desc);
    }

    OH_ResourceManager_CloseRawFile(rawFile);
    OH_ResourceManager_ReleaseNativeResourceManager(resMgr);

    std::vector<uint8_t> bytes;
    if (!engine->ReadRawfileToBytes(env, args[0], path, bytes)) {
        return GetUndefined(env);
    }

    void* buffer = nullptr;
    napi_value arraybuffer;
    napi_create_arraybuffer(env, bytes.size(), &buffer, &arraybuffer);
    if (buffer && !bytes.empty()) {
        memcpy(buffer, bytes.data(), bytes.size());
    }
    return arraybuffer;
}

napi_value GLEXEngine::NapiSetUniform(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 2);
    if (!engine) return GetUndefined(env);

    if (argc < 2) {
        engine->SetError("setUniform: missing parameters");
        return GetUndefined(env);
    }

    std::string name;
    if (!GetString(env, args[0], name)) {
        engine->SetError("setUniform: invalid name");
        return GetUndefined(env);
    }

    std::vector<float> values;
    if (!GetFloatArray(env, args[1], values)) {
        engine->SetError("setUniform: invalid value");
        return GetUndefined(env);
    }

    engine->RequestUniform(name, values);
    return GetUndefined(env);
}

napi_value GLEXEngine::NapiSetPasses(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 1);
    if (!engine) return GetUndefined(env);

    if (argc < 1) {
        engine->SetError("setPasses: missing parameters");
        return GetUndefined(env);
    }

    bool isArray = false;
    if (napi_is_array(env, args[0], &isArray) != napi_ok || !isArray) {
        engine->SetError("setPasses: invalid parameters");
        return GetUndefined(env);
    }

    uint32_t len = 0;
    if (napi_get_array_length(env, args[0], &len) != napi_ok) {
        engine->SetError("setPasses: invalid parameters");
        return GetUndefined(env);
    }

    std::vector<std::string> passes;
    passes.reserve(len);
    for (uint32_t i = 0; i < len; i++) {
        napi_value elem;
        if (napi_get_element(env, args[0], i, &elem) != napi_ok) {
            engine->SetError("setPasses: invalid parameters");
            return GetUndefined(env);
        }
        std::string name;
        if (!GetString(env, elem, name)) {
            engine->SetError("setPasses: invalid pass name");
            return GetUndefined(env);
        }
        passes.push_back(name);
    }

    engine->RequestPasses(std::move(passes));
    return GetUndefined(env);
}

napi_value GLEXEngine::NapiAddPass(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 1);
    if (!engine) return GetUndefined(env);

    if (argc < 1) {
        engine->SetError("addPass: missing parameters");
        return GetUndefined(env);
    }

    std::string name;
    if (!GetString(env, args[0], name)) {
        engine->SetError("addPass: invalid pass name");
        return GetUndefined(env);
    }

    engine->RequestAddPass(name);
    return GetUndefined(env);
}

napi_value GLEXEngine::NapiRemovePass(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 1);
    if (!engine) return GetUndefined(env);

    if (argc < 1) {
        engine->SetError("removePass: missing parameters");
        return GetUndefined(env);
    }

    std::string name;
    if (!GetString(env, args[0], name)) {
        engine->SetError("removePass: invalid pass name");
        return GetUndefined(env);
    }

    engine->RequestRemovePass(name);
    return GetUndefined(env);
}

napi_value GLEXEngine::NapiGetPasses(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    napi_value args[1];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 0);
    if (!engine) return GetUndefined(env);

    std::vector<std::string> passes = engine->GetRequestedPassesSnapshot();
    napi_value result;
    napi_create_array_with_length(env, passes.size(), &result);
    for (size_t i = 0; i < passes.size(); i++) {
        napi_value v;
        napi_create_string_utf8(env, passes[i].c_str(), NAPI_AUTO_LENGTH, &v);
        napi_set_element(env, result, static_cast<uint32_t>(i), v);
    }
    return result;
}

napi_value GLEXEngine::NapiSetTouchEvent(napi_env env, napi_callback_info info)
{
    size_t argc = 4;
    napi_value args[4];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 4);
    if (!engine) return GetUndefined(env);

    if (argc < 3) {
        engine->SetError("setTouchEvent: missing parameters");
        return GetUndefined(env);
    }

    double x = 0.0;
    double y = 0.0;
    int32_t action = 0;
    int32_t pointerId = 0;
    if (!GetDouble(env, args[0], &x) || !GetDouble(env, args[1], &y) || !GetInt32(env, args[2], &action)) {
        engine->SetError("setTouchEvent: invalid parameters");
        return GetUndefined(env);
    }
    if (argc >= 4) {
        GetInt32(env, args[3], &pointerId);
    }

    if (!std::isfinite(x) || !std::isfinite(y)) {
        engine->SetError("setTouchEvent: non-finite coordinates");
        return GetUndefined(env);
    }

    engine->touchX_.store(static_cast<float>(x), std::memory_order_relaxed);
    engine->touchY_.store(static_cast<float>(y), std::memory_order_relaxed);
    engine->touchAction_.store(action, std::memory_order_relaxed);
    engine->touchPointerId_.store(pointerId, std::memory_order_relaxed);
    engine->touchSeq_.fetch_add(1, std::memory_order_relaxed);
    return GetUndefined(env);
}
napi_value GLEXEngine::NapiGetCurrentFPS(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    napi_value args[1];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 0);
    if (!engine) return GetUndefined(env);

    float fps = engine->renderThread_ ? engine->renderThread_->getCurrentFPS() : 0.0f;
    napi_value result;
    napi_create_double(env, static_cast<double>(fps), &result);
    return result;
}

napi_value GLEXEngine::NapiGetGLInfo(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    napi_value args[1];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 0);
    if (!engine) return GetUndefined(env);

    napi_value result;
    napi_create_object(env, &result);

    auto setStr = [&](const char* key, const char* value) {
        napi_value v;
        napi_create_string_utf8(env, value ? value : "unknown", NAPI_AUTO_LENGTH, &v);
        napi_set_named_property(env, result, key, v);
    };

    if (engine->glContext_ && engine->glContext_->isInitialized()) {
        setStr("version", engine->glContext_->getGLVersion());
        setStr("renderer", engine->glContext_->getGLRenderer());

        napi_value w, h;
        napi_create_int32(env, engine->glContext_->getWidth(), &w);
        napi_create_int32(env, engine->glContext_->getHeight(), &h);
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

napi_value GLEXEngine::NapiGetGpuStats(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    napi_value args[1];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 0);
    if (!engine) return GetUndefined(env);

    GLResourceStats stats = GLResourceTracker::Get().GetStats();
    napi_value result;
    napi_create_object(env, &result);

    auto setInt = [&](const char* key, int value) {
        napi_value v;
        napi_create_int32(env, value, &v);
        napi_set_named_property(env, result, key, v);
    };

    setInt("programs", stats.programs);
    setInt("shaders", stats.shaders);
    setInt("buffers", stats.buffers);
    setInt("vaos", stats.vaos);
    setInt("textures", stats.textures);

    return result;
}

napi_value GLEXEngine::NapiGetLastError(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    napi_value args[1];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 0);
    if (!engine) return GetUndefined(env);

    std::string err = engine->GetError();
    napi_value result;
    napi_create_string_utf8(env, err.c_str(), NAPI_AUTO_LENGTH, &result);
    return result;
}

napi_value GLEXEngine::NapiClearLastError(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    napi_value args[1];
    GLEXEngine* engine = UnwrapEngine(env, info, &argc, args, 0);
    if (!engine) return GetUndefined(env);

    engine->ClearError();
    return GetUndefined(env);
}

// ============================================================
// XComponent 回调
// ============================================================

static OH_NativeXComponent_Callback g_xcomponentCallback;

static void OnSurfaceCreated(OH_NativeXComponent* component, void* window)
{
    std::string id = GetXComponentId(component);
    if (id.empty()) {
        return;
    }

    GLEXEngine* engine = nullptr;
    PendingSurface pending;
    {
        auto& registry = GetRegistry();
        std::lock_guard<std::mutex> lock(registry.mutex);
        auto it = registry.engines.find(id);
        if (it != registry.engines.end()) {
            engine = it->second;
        } else {
            auto& entry = registry.pending[id];
            entry.window = static_cast<OHNativeWindow*>(window);
            uint64_t w = 0, h = 0;
            if (OH_NativeXComponent_GetXComponentSize(component, window, &w, &h) ==
                OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
                entry.width = w;
                entry.height = h;
                entry.hasSize = true;
            }
            return;
        }
    }

    if (engine) {
        engine->HandleSurfaceCreated(static_cast<OHNativeWindow*>(window));
    }
}

static void OnSurfaceChanged(OH_NativeXComponent* component, void* window)
{
    std::string id = GetXComponentId(component);
    if (id.empty()) {
        return;
    }

    GLEXEngine* engine = nullptr;
    uint64_t w = 0, h = 0;
    if (OH_NativeXComponent_GetXComponentSize(component, window, &w, &h) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        return;
    }

    {
        auto& registry = GetRegistry();
        std::lock_guard<std::mutex> lock(registry.mutex);
        auto it = registry.engines.find(id);
        if (it != registry.engines.end()) {
            engine = it->second;
        } else {
            auto& entry = registry.pending[id];
            entry.width = w;
            entry.height = h;
            entry.hasSize = true;
            return;
        }
    }

    if (engine) {
        engine->HandleSurfaceChanged(w, h);
    }
}

static void OnSurfaceDestroyed(OH_NativeXComponent* component, void* window)
{
    (void)window;
    std::string id = GetXComponentId(component);
    if (id.empty()) {
        return;
    }

    GLEXEngine* engine = nullptr;
    {
        auto& registry = GetRegistry();
        std::lock_guard<std::mutex> lock(registry.mutex);
        auto it = registry.engines.find(id);
        if (it != registry.engines.end()) {
            engine = it->second;
        } else {
            registry.pending.erase(id);
            return;
        }
    }

    if (engine) {
        engine->HandleSurfaceDestroyed();
    }
}

static void OnDispatchTouchEvent(OH_NativeXComponent* component, void* window)
{
    (void)component;
    (void)window;
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

static napi_ref g_engineCtorRef = nullptr;

static napi_value CreateRenderer(napi_env env, napi_callback_info info)
{
    (void)info;
    napi_value ctor = nullptr;
    if (!g_engineCtorRef || napi_get_reference_value(env, g_engineCtorRef, &ctor) != napi_ok) {
        return GetUndefined(env);
    }
    napi_value instance = nullptr;
    if (napi_new_instance(env, ctor, 0, nullptr, &instance) != napi_ok) {
        return GetUndefined(env);
    }
    return instance;
}

napi_value Init(napi_env env, napi_value exports)
{
    RegisterBuiltinPasses();

    napi_property_descriptor props[] = {
        { "bindXComponent", nullptr, GLEXEngine::NapiBindXComponent, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "unbindXComponent", nullptr, GLEXEngine::NapiUnbindXComponent, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setSurfaceId", nullptr, GLEXEngine::NapiSetSurfaceId, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "destroySurface", nullptr, GLEXEngine::NapiDestroySurface, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "startRender", nullptr, GLEXEngine::NapiStartRender, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "stopRender", nullptr, GLEXEngine::NapiStopRender, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "resize", nullptr, GLEXEngine::NapiResize, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setBackgroundColor", nullptr, GLEXEngine::NapiSetBackgroundColor, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setTargetFPS", nullptr, GLEXEngine::NapiSetTargetFPS, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setShaderSources", nullptr, GLEXEngine::NapiSetShaderSources, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "loadShaderFromRawfile", nullptr, GLEXEngine::NapiLoadShaderFromRawfile, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "loadRawfileBytes", nullptr, GLEXEngine::NapiLoadRawfileBytes, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setUniform", nullptr, GLEXEngine::NapiSetUniform, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setPasses", nullptr, GLEXEngine::NapiSetPasses, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "addPass", nullptr, GLEXEngine::NapiAddPass, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "removePass", nullptr, GLEXEngine::NapiRemovePass, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getPasses", nullptr, GLEXEngine::NapiGetPasses, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "setTouchEvent", nullptr, GLEXEngine::NapiSetTouchEvent, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getCurrentFPS", nullptr, GLEXEngine::NapiGetCurrentFPS, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getGLInfo", nullptr, GLEXEngine::NapiGetGLInfo, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getGpuStats", nullptr, GLEXEngine::NapiGetGpuStats, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "getLastError", nullptr, GLEXEngine::NapiGetLastError, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "clearLastError", nullptr, GLEXEngine::NapiClearLastError, nullptr, nullptr, nullptr, napi_default, nullptr },
    };

    napi_value ctor = nullptr;
    if (napi_define_class(env, "GLEXEngine", NAPI_AUTO_LENGTH, GLEXEngine::NapiNew, nullptr,
                          sizeof(props) / sizeof(props[0]), props, &ctor) == napi_ok) {
        napi_create_reference(env, ctor, 1, &g_engineCtorRef);
        napi_set_named_property(env, exports, "GLEXEngine", ctor);
    }

    napi_property_descriptor createDesc = {
        "createRenderer", nullptr, CreateRenderer, nullptr, nullptr, nullptr, napi_default, nullptr
    };
    napi_define_properties(env, exports, 1, &createDesc);

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

static napi_value GLEXInit(napi_env env, napi_value exports)
{
    return glex::bridge::Init(env, exports);
}

NAPI_MODULE(glex, GLEXInit)
