#pragma once

/**
 * @file RenderPass.h
 * @brief 渲染阶段抽象基类
 *
 * 定义了渲染管线中单个 Pass 的接口。
 * 开发者继承此类实现自定义的渲染逻辑。
 *
 * 用法：
 *   class MySkyPass : public RenderPass {
 *   public:
 *       MySkyPass() : RenderPass("SkyPass") {}
 *       void onInitialize(int w, int h) override { ... }
 *       void onUpdate(float dt) override { ... }
 *       void onRender() override { ... }
 *       void onDestroy() override { ... }
 *   };
 */

#include <string>

namespace glex {

class RenderPass {
public:
    explicit RenderPass(const std::string& name) : name_(name) {}
    virtual ~RenderPass() = default;

    // 禁止拷贝
    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;

    /** 初始化（GL 上下文已就绪时调用） */
    void initialize(int width, int height) {
        width_ = width;
        height_ = height;
        onInitialize(width, height);
        initialized_ = true;
    }

    /** 尺寸变化 */
    void resize(int width, int height) {
        width_ = width;
        height_ = height;
        onResize(width, height);
    }

    /** 每帧更新逻辑 */
    void update(float deltaTime) {
        if (enabled_ && initialized_) {
            onUpdate(deltaTime);
        }
    }

    /** 每帧渲染 */
    void render() {
        if (enabled_ && initialized_) {
            onRender();
        }
    }

    /** 销毁资源 */
    void destroy() {
        if (initialized_) {
            onDestroy();
            initialized_ = false;
        }
    }

    // ============================================================
    // 属性
    // ============================================================

    const std::string& getName() const { return name_; }

    bool isEnabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }

    bool isInitialized() const { return initialized_; }

    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

protected:
    /** 子类实现：初始化 GL 资源 */
    virtual void onInitialize(int width, int height) = 0;

    /** 子类实现：尺寸变化处理 */
    virtual void onResize(int width, int height) {}

    /** 子类实现：每帧更新 */
    virtual void onUpdate(float deltaTime) {}

    /** 子类实现：渲染绘制 */
    virtual void onRender() = 0;

    /** 子类实现：销毁 GL 资源 */
    virtual void onDestroy() = 0;

    std::string name_;
    bool enabled_ = true;
    bool initialized_ = false;
    int width_ = 0;
    int height_ = 0;
};

} // namespace glex
