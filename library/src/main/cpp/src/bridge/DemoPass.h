#pragma once

/**
 * @file DemoPass.h
 * @brief 内置演示渲染阶段
 *
 * 使用 OpenGL ES 渲染一个动态粒子星空效果，
 * 展示 GLEX 框架的 ShaderProgram + RenderPass 用法。
 *
 * 效果：渐变背景 + 闪烁星星 + 流星
 */

#include <random>
#include <vector>

#include <GLES3/gl3.h>

#include "glex/RenderPass.h"
#include "glex/ShaderProgram.h"

namespace glex {

struct DemoStar {
    float x, y;          // 位置
    float size;           // 大小
    float brightness;     // 亮度
    float twinklePhase;   // 闪烁相位
    float twinkleSpeed;   // 闪烁速度
    float r, g, b;        // 颜色
};

struct DemoMeteor {
    float x, y;          // 位置
    float vx, vy;        // 速度
    float life;          // 剩余生命
    float maxLife;       // 最大生命
    float size;          // 大小
    bool active;         // 是否活跃
};

class DemoPass : public RenderPass {
public:
    DemoPass() : RenderPass("DemoPass") {}

protected:
    void onInitialize(int width, int height) override;
    void onResize(int width, int height) override;
    void onUpdate(float deltaTime) override;
    void onRender() override;
    void onDestroy() override;

private:
    void initStars();
    void initMeteors();
    void initGLResources();
    void spawnMeteor();

    // 星星
    std::vector<DemoStar> stars_;
    int starCount_ = 200;

    // 流星
    std::vector<DemoMeteor> meteors_;
    static constexpr int MAX_METEORS = 3;
    float meteorTimer_ = 0.0f;
    float nextMeteorTime_ = 2.0f;

    // 时间
    float time_ = 0.0f;

    // 随机数
    std::mt19937 rng_{42};

    // GL 资源 - 背景
    ShaderProgram bgShader_;
    GLuint bgVao_ = 0;
    GLuint bgVbo_ = 0;

    // GL 资源 - 星星
    ShaderProgram starShader_;
    GLuint starVao_ = 0;
    GLuint starVbo_ = 0;

    // GL 资源 - 流星
    ShaderProgram meteorShader_;
    GLuint meteorVao_ = 0;
    GLuint meteorVbo_ = 0;

    bool glReady_ = false;
};

} // namespace glex
