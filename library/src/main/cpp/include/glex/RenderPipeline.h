#pragma once

/**
 * @file RenderPipeline.h
 * @brief 多阶段渲染管线
 *
 * 管理有序的 RenderPass 列表，按顺序执行初始化、更新和渲染。
 *
 * 用法：
 *   RenderPipeline pipeline;
 *   pipeline.addPass(std::make_shared<MySkyPass>());
 *   pipeline.addPass(std::make_shared<MyScenePass>());
 *   pipeline.initialize(width, height);
 *
 *   // 每帧：
 *   pipeline.update(deltaTime);
 *   pipeline.render();
 */

#include <memory>
#include <string>
#include <vector>

#include "glex/RenderPass.h"

namespace glex {

class RenderPipeline {
public:
    RenderPipeline() = default;
    ~RenderPipeline();

    /**
     * 添加渲染阶段（按添加顺序执行）
     */
    void addPass(std::shared_ptr<RenderPass> pass);

    /**
     * 按名称移除渲染阶段
     */
    bool removePass(const std::string& name);

    /**
     * 按名称查找渲染阶段
     */
    RenderPass* getPass(const std::string& name) const;

    /**
     * 获取渲染阶段数量
     */
    size_t getPassCount() const { return passes_.size(); }

    /** 初始化所有 Pass */
    void initialize(int width, int height);

    /** 调整所有 Pass 尺寸 */
    void resize(int width, int height);

    /** 更新所有 Pass */
    void update(float deltaTime);

    /** 按顺序渲染所有启用的 Pass */
    void render();

    /** 分发触摸事件 */
    void dispatchTouch(float x, float y, int action, int pointerId);

    /** 销毁所有 Pass */
    void destroy();

    bool isInitialized() const { return initialized_; }

private:
    std::vector<std::shared_ptr<RenderPass>> passes_;
    int width_ = 0;
    int height_ = 0;
    bool initialized_ = false;
};

} // namespace glex
