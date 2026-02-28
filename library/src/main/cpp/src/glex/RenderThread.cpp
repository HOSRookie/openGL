#include "glex/RenderThread.h"
#include "glex/GLContext.h"
#include "glex/Log.h"

#include <chrono>

namespace glex {

RenderThread::~RenderThread()
{
    stop();
}

void RenderThread::start(GLContext* context, FrameCallback callback)
{
    if (running_.load()) {
        GLEX_LOGW("RenderThread already running");
        return;
    }

    if (!context || !context->isInitialized()) {
        GLEX_LOGE("RenderThread: GLContext not ready");
        return;
    }

    context_ = context;
    callback_ = std::move(callback);
    running_.store(true);

    thread_ = std::thread([this]() { loop(); });
    GLEX_LOGI("RenderThread started (target %{public}d FPS)", targetFPS_.load());
}

void RenderThread::stop()
{
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    if (thread_.joinable()) {
        thread_.join();
    }

    context_ = nullptr;
    callback_ = nullptr;
    currentFPS_.store(0.0f);
    GLEX_LOGI("RenderThread stopped");
}

void RenderThread::setTargetFPS(int fps)
{
    targetFPS_.store(fps > 0 ? fps : 1);
}

void RenderThread::post(std::function<void()> task)
{
    if (!task) {
        return;
    }
    std::lock_guard<std::mutex> lock(taskMutex_);
    tasks_.push_back(std::move(task));
}

void RenderThread::loop()
{
    if (!context_ || !context_->makeCurrent()) {
        GLEX_LOGE("RenderThread: failed to make context current");
        running_.store(false);
        return;
    }

    GLEX_LOGI("RenderThread: GL context bound to render thread");

    auto prev = std::chrono::steady_clock::now();
    int frameCount = 0;
    auto fpsTimer = prev;

    while (running_.load()) {
        auto now = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(now - prev).count();
        prev = now;

        drainTasks();

        // 调用用户渲染回调
        if (callback_) {
            callback_(deltaTime);
        }

        // 交换缓冲区
        if (!context_->swapBuffers()) {
            GLEX_LOGE("RenderThread: swapBuffers failed");
            running_.store(false);
            break;
        }

        // 统计 FPS
        frameCount++;
        auto elapsed = std::chrono::duration<float>(now - fpsTimer).count();
        if (elapsed >= 1.0f) {
            currentFPS_.store(static_cast<float>(frameCount) / elapsed);
            frameCount = 0;
            fpsTimer = now;
        }

        // 帧率控制
        int fps = targetFPS_.load();
        int frameIntervalMs = 1000 / fps;
        auto end = std::chrono::steady_clock::now();
        auto renderTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - now);
        if (renderTime.count() < frameIntervalMs) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(frameIntervalMs - renderTime.count()));
        }
    }

    drainTasks();
    context_->clearCurrent();
    GLEX_LOGI("RenderThread: render loop exited");
}

void RenderThread::drainTasks()
{
    std::vector<std::function<void()>> tasks;
    {
        std::lock_guard<std::mutex> lock(taskMutex_);
        tasks.swap(tasks_);
    }
    for (auto& task : tasks) {
        task();
    }
}

} // namespace glex
