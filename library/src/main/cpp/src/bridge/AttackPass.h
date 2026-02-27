#pragma once

/**
 * @file AttackPass.h
 * @brief 割草游戏“扇形斩击”粒子特效 Pass
 *
 * 表现为扇形爆发的高亮能量粒子，带拖尾与衰减。
 */

#include <random>
#include <vector>

#include <GLES3/gl3.h>

#include "glex/RenderPass.h"
#include "glex/ShaderProgram.h"

namespace glex {

struct AttackParticle {
    float x;
    float y;
    float vx;
    float vy;
    float life;
    float maxLife;
    float baseSize;
};

class AttackPass : public RenderPass {
public:
    AttackPass() : RenderPass("AttackPass") {}
    void setTouch(float x, float y, int action, int pointerId);

protected:
    void onInitialize(int width, int height) override;
    void onResize(int width, int height) override;
    void onUpdate(float deltaTime) override;
    void onRender() override;
    void onTouch(float x, float y, int action, int pointerId) override;
    void onDestroy() override;

private:
    void initParticles();
    void spawnBurst(float sweepAngleDeg, int count);
    void beginSlash(float centerDeg);
    void updateOrigin();

    std::vector<AttackParticle> particles_;
    int maxParticles_ = 1800;
    int burstCount_ = 240;
    int nextIndex_ = 0;

    float time_ = 0.0f;
    float slashTimer_ = -1.0f;
    float idleTimer_ = 0.0f;
    float slashDuration_ = 0.22f;
    float slashInterval_ = 0.85f;
    float spawnAccumulator_ = 0.0f;
    float sweepStartDeg_ = -130.0f;
    float sweepEndDeg_ = -50.0f;
    float sweepCenterDeg_ = -90.0f;
    float sweepSpanDeg_ = 80.0f;
    float arcInner_ = 60.0f;
    float arcOuter_ = 140.0f;
    float originX_ = 0.0f;
    float originY_ = 0.0f;
    float drag_ = 3.2f;
    int trailSteps_ = 4;
    float trailSpacing_ = 14.0f;
    float touchCooldown_ = 0.0f;
    float lastTouchX_ = -1.0f;
    float lastTouchY_ = -1.0f;
    int lastPointerId_ = -1;
    bool hasLastTouch_ = false;
    float shockTimer_ = -1.0f;
    float shockDuration_ = 0.25f;
    float shockRadiusStart_ = 40.0f;
    float shockRadiusEnd_ = 180.0f;

    std::mt19937 rng_{1337};

    float maxPointSize_ = 32.0f;

    ShaderProgram shader_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    bool glReady_ = false;
};

} // namespace glex
