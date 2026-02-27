#include "glex/RenderPipeline.h"
#include "glex/Log.h"

#include <algorithm>

namespace glex {

RenderPipeline::~RenderPipeline()
{
    destroy();
}

void RenderPipeline::addPass(std::shared_ptr<RenderPass> pass)
{
    if (!pass) {
        return;
    }

    // 如果管线已初始化，自动初始化新添加的 Pass
    if (initialized_) {
        pass->initialize(width_, height_);
    }

    passes_.push_back(std::move(pass));
    GLEX_LOGI("Pipeline: added pass '%{public}s' (total: %{public}d)",
              passes_.back()->getName().c_str(), static_cast<int>(passes_.size()));
}

bool RenderPipeline::removePass(const std::string& name)
{
    auto it = std::find_if(passes_.begin(), passes_.end(),
        [&name](const std::shared_ptr<RenderPass>& p) {
            return p->getName() == name;
        });

    if (it != passes_.end()) {
        (*it)->destroy();
        passes_.erase(it);
        GLEX_LOGI("Pipeline: removed pass '%{public}s'", name.c_str());
        return true;
    }

    return false;
}

RenderPass* RenderPipeline::getPass(const std::string& name) const
{
    for (auto& p : passes_) {
        if (p->getName() == name) {
            return p.get();
        }
    }
    return nullptr;
}

void RenderPipeline::initialize(int width, int height)
{
    width_ = width;
    height_ = height;

    for (auto& pass : passes_) {
        pass->initialize(width, height);
    }

    initialized_ = true;
    GLEX_LOGI("Pipeline initialized: %{public}dx%{public}d, %{public}d passes",
              width, height, static_cast<int>(passes_.size()));
}

void RenderPipeline::resize(int width, int height)
{
    width_ = width;
    height_ = height;

    for (auto& pass : passes_) {
        pass->resize(width, height);
    }

    GLEX_LOGI("Pipeline resized: %{public}dx%{public}d", width, height);
}

void RenderPipeline::update(float deltaTime)
{
    for (auto& pass : passes_) {
        pass->update(deltaTime);
    }
}

void RenderPipeline::render()
{
    for (auto& pass : passes_) {
        pass->render();
    }
}

void RenderPipeline::dispatchTouch(float x, float y, int action, int pointerId)
{
    for (auto& pass : passes_) {
        pass->touch(x, y, action, pointerId);
    }
}

void RenderPipeline::destroy()
{
    for (auto& pass : passes_) {
        pass->destroy();
    }
    passes_.clear();
    initialized_ = false;
    GLEX_LOGI("Pipeline destroyed");
}

} // namespace glex
