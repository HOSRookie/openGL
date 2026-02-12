#pragma once

/**
 * @file RenderThread.h
 * @brief 独立渲染线程
 *
 * 在独立线程上运行渲染循环，支持帧率控制。
 * 自动管理 EGL 上下文的线程绑定。
 *
 * 用法：
 *   RenderThread thread;
 *   thread.setTargetFPS(60);
 *   thread.start(glContext, [](float dt) {
 *       // 你的渲染代码
 *   });
 *   // ...
 *   thread.stop();
 */

#include <atomic>
#include <functional>
#include <thread>

namespace glex {

class GLContext;

class RenderThread {
public:
    /**
     * 帧回调函数类型
     * @param deltaTime 距上一帧的时间（秒）
     */
    using FrameCallback = std::function<void(float deltaTime)>;

    RenderThread() = default;
    ~RenderThread();

    // 禁止拷贝
    RenderThread(const RenderThread&) = delete;
    RenderThread& operator=(const RenderThread&) = delete;

    /**
     * 启动渲染循环
     * @param context GL 上下文（渲染线程会自动绑定）
     * @param callback 每帧回调
     */
    void start(GLContext* context, FrameCallback callback);

    /**
     * 停止渲染循环并等待线程退出
     */
    void stop();

    /**
     * 设置目标帧率
     * @param fps 目标 FPS（默认 60）
     */
    void setTargetFPS(int fps);

    /** 获取目标帧率 */
    int getTargetFPS() const { return targetFPS_.load(); }

    /** 渲染循环是否正在运行 */
    bool isRunning() const { return running_.load(); }

    /** 获取实际帧率（最近一秒的平均值） */
    float getCurrentFPS() const { return currentFPS_.load(); }

private:
    void loop();

    GLContext* context_ = nullptr;
    FrameCallback callback_;
    std::thread thread_;

    std::atomic<bool> running_{false};
    std::atomic<int> targetFPS_{60};
    std::atomic<float> currentFPS_{0.0f};
};

} // namespace glex
