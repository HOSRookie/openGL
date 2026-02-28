#include "AttackPass.h"
#include "glex/GLResourceTracker.h"
#include "glex/Log.h"

#include <algorithm>
#include <cmath>

namespace glex {

namespace {

constexpr float kPi = 3.14159265358979323846f;

static float DegToRad(float deg)
{
    return deg * kPi / 180.0f;
}

static void MakeOrtho(float* m, float l, float r, float b, float t, float n, float f)
{
    for (int i = 0; i < 16; i++) m[i] = 0.0f;
    m[0]  =  2.0f / (r - l);
    m[5]  =  2.0f / (t - b);
    m[10] = -2.0f / (f - n);
    m[12] = -(r + l) / (r - l);
    m[13] = -(t + b) / (t - b);
    m[14] = -(f + n) / (f - n);
    m[15] = 1.0f;
}

static const char* kAttackVertSrc = R"(#version 300 es
layout(location = 0) in vec2 a_position;
layout(location = 1) in float a_size;
layout(location = 2) in float a_life;
layout(location = 3) in float a_alpha;

uniform mat4 u_projection;

out float v_life;
out float v_alpha;

void main() {
    v_life = a_life;
    v_alpha = a_alpha;
    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
    gl_PointSize = a_size;
}
)";

static const char* kAttackFragSrc = R"(#version 300 es
precision highp float;

in float v_life;
in float v_alpha;
out vec4 fragColor;

void main() {
    vec2 uv = gl_PointCoord - vec2(0.5);
    float dist = length(uv);
    float alpha = smoothstep(0.5, 0.1, dist);
    float core = smoothstep(0.25, 0.0, dist);
    float a = alpha * (0.6 + 0.4 * core);

    vec3 c1 = vec3(1.0, 1.0, 0.82);
    vec3 c2 = vec3(1.0, 0.70, 0.28);
    vec3 c3 = vec3(0.29, 0.0, 0.51);

    vec3 color;
    if (v_life < 0.2) {
        color = mix(c1, c2, v_life / 0.2);
    } else if (v_life < 0.6) {
        color = mix(c2, c3, (v_life - 0.2) / 0.4);
    } else {
        color = mix(c3, c3 * 0.6, (v_life - 0.6) / 0.4);
    }

    float fade = 1.0 - v_life;
    fragColor = vec4(color, a * fade * v_alpha);
}
)";

struct AttackVertex {
    float x;
    float y;
    float size;
    float life;
    float alpha;
};

} // namespace

void AttackPass::onInitialize(int width, int height)
{
    initParticles();
    updateOrigin();
    idleTimer_ = slashInterval_;

    GLfloat range[2] = { 1.0f, maxPointSize_ };
    glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, range);
    if (range[1] > 0.0f) {
        maxPointSize_ = std::min(maxPointSize_, range[1]);
    }

    if (!shader_.build(kAttackVertSrc, kAttackFragSrc)) {
        GLEX_LOGE("AttackPass: shader build failed");
        glReady_ = false;
        return;
    }

    glGenVertexArrays(1, &vao_);
    GLResourceTracker::Get().OnCreateVertexArray();
    glGenBuffers(1, &vbo_);
    GLResourceTracker::Get().OnCreateBuffer();
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(maxParticles_ * sizeof(AttackVertex)),
                 nullptr,
                 GL_DYNAMIC_DRAW);

    constexpr GLsizei stride = sizeof(AttackVertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
    glBindVertexArray(0);

    glReady_ = true;
    GLEX_LOGI("AttackPass initialized %{public}dx%{public}d", width, height);
}

void AttackPass::onResize(int width, int height)
{
    (void)width;
    (void)height;
    updateOrigin();
}

void AttackPass::onUpdate(float deltaTime)
{
    time_ += deltaTime;

    if (touchCooldown_ > 0.0f) {
        touchCooldown_ -= deltaTime;
        if (touchCooldown_ < 0.0f) touchCooldown_ = 0.0f;
    }
    if (shockTimer_ >= 0.0f) {
        shockTimer_ += deltaTime;
        if (shockTimer_ >= shockDuration_) {
            shockTimer_ = -1.0f;
        }
    }

    const float damping = std::exp(-drag_ * deltaTime);
    for (auto& p : particles_) {
        if (p.life <= 0.0f) {
            continue;
        }
        p.x += p.vx * deltaTime;
        p.y += p.vy * deltaTime;
        p.vx *= damping;
        p.vy *= damping;
        p.life -= deltaTime;
        if (p.life <= 0.0f) {
            p.life = 0.0f;
        }
    }

    idleTimer_ += deltaTime;
    if (idleTimer_ >= slashInterval_) {
        idleTimer_ = 0.0f;
        beginSlash(sweepCenterDeg_);
    }

    if (slashTimer_ >= 0.0f) {
        slashTimer_ += deltaTime;
        float progress = slashTimer_ / slashDuration_;
        if (progress >= 1.0f) {
            slashTimer_ = -1.0f;
        } else {
            float sweepAngle = sweepStartDeg_ + (sweepEndDeg_ - sweepStartDeg_) * progress;
            float total = static_cast<float>(burstCount_) * (deltaTime / slashDuration_);
            spawnAccumulator_ += total;
            int count = static_cast<int>(spawnAccumulator_);
            if (count > 0) {
                spawnAccumulator_ -= static_cast<float>(count);
                spawnBurst(sweepAngle, count);
            }
        }
    }
}

void AttackPass::onRender()
{
    if (!glReady_) return;

    std::vector<AttackVertex> verts;
    verts.reserve(static_cast<size_t>(maxParticles_) * static_cast<size_t>(std::max(1, trailSteps_)));

    for (const auto& p : particles_) {
        if (p.life <= 0.0f || p.maxLife <= 0.0f) continue;
        float t = 1.0f - (p.life / p.maxLife);
        float size = p.baseSize * (1.0f - t);
        if (size <= 0.1f) continue;
        if (size > maxPointSize_) size = maxPointSize_;
        float life = std::min(std::max(t, 0.0f), 1.0f);

        float vlen = std::sqrt(p.vx * p.vx + p.vy * p.vy);
        float nx = 0.0f;
        float ny = -1.0f;
        if (vlen > 0.0001f) {
            nx = p.vx / vlen;
            ny = p.vy / vlen;
        }

        for (int i = 0; i < trailSteps_; i++) {
            float trailT = static_cast<float>(i) / static_cast<float>(trailSteps_);
            float offset = trailT * trailSpacing_ * 1.2f;
            float trailSize = size * (1.0f - trailT * 0.6f);
            float trailAlpha = 1.0f - trailT * 0.8f;
            if (trailSize <= 0.1f) continue;
            AttackVertex v;
            v.x = p.x - nx * offset;
            v.y = p.y - ny * offset;
            v.size = trailSize;
            v.life = life;
            v.alpha = trailAlpha;
            verts.push_back(v);
        }
    }

    if (slashTimer_ >= 0.0f) {
        float progress = slashTimer_ / slashDuration_;
        if (progress > 1.0f) progress = 1.0f;
        float currentAngle = sweepStartDeg_ + (sweepEndDeg_ - sweepStartDeg_) * progress;
        float arcRadius = (arcInner_ + arcOuter_) * 0.5f;
        constexpr int ARC_POINTS = 28;
        for (int i = 0; i < ARC_POINTS; i++) {
            float t = static_cast<float>(i) / static_cast<float>(ARC_POINTS - 1);
            float angle = sweepStartDeg_ + (sweepEndDeg_ - sweepStartDeg_) * t;
            float diff = std::fabs(angle - currentAngle);
            float head = std::max(0.0f, 1.0f - diff / 25.0f);
            float angleRad = DegToRad(angle);
            float x = originX_ + std::cos(angleRad) * arcRadius;
            float y = originY_ + std::sin(angleRad) * arcRadius;

            AttackVertex v;
            v.x = x;
            v.y = y;
            v.size = 14.0f + 10.0f * head;
            v.life = 0.12f + 0.2f * progress;
            v.alpha = 0.25f + 0.75f * head;
            verts.push_back(v);
        }

        AttackVertex head;
        float headRad = DegToRad(currentAngle);
        head.x = originX_ + std::cos(headRad) * arcRadius;
        head.y = originY_ + std::sin(headRad) * arcRadius;
        head.size = 28.0f;
        head.life = 0.08f;
        head.alpha = 1.0f;
        verts.push_back(head);
    }

    if (shockTimer_ >= 0.0f) {
        float t = shockTimer_ / shockDuration_;
        if (t > 1.0f) t = 1.0f;
        float radius = shockRadiusStart_ + (shockRadiusEnd_ - shockRadiusStart_) * t;
        float alpha = 1.0f - t;
        constexpr int RING_POINTS = 36;
        for (int i = 0; i < RING_POINTS; i++) {
            float a = (2.0f * kPi) * (static_cast<float>(i) / static_cast<float>(RING_POINTS));
            AttackVertex v;
            v.x = originX_ + std::cos(a) * radius;
            v.y = originY_ + std::sin(a) * radius;
            v.size = 10.0f - 4.0f * t;
            v.life = 0.55f;
            v.alpha = 0.25f * alpha;
            verts.push_back(v);
        }
    }

    if (verts.empty()) return;

    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);
    float proj[16];
    MakeOrtho(proj, 0.0f, w, h, 0.0f, -1.0f, 1.0f);

    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    shader_.use();
    shader_.setUniformMatrix4fv("u_projection", proj);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(AttackVertex)),
                 verts.data(),
                 GL_DYNAMIC_DRAW);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(verts.size()));
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    if (depthEnabled) {
        glEnable(GL_DEPTH_TEST);
    }
}

void AttackPass::onDestroy()
{
    shader_.destroy();
    if (vbo_) {
        GLResourceTracker::Get().OnDeleteBuffer();
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_) {
        GLResourceTracker::Get().OnDeleteVertexArray();
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    glReady_ = false;
    GLEX_LOGI("AttackPass destroyed");
}

void AttackPass::initParticles()
{
    particles_.clear();
    particles_.resize(static_cast<size_t>(maxParticles_));
    for (auto& p : particles_) {
        p.life = 0.0f;
        p.maxLife = 0.0f;
        p.baseSize = 0.0f;
        p.x = 0.0f;
        p.y = 0.0f;
        p.vx = 0.0f;
        p.vy = 0.0f;
    }
}

void AttackPass::spawnBurst(float sweepAngleDeg, int count)
{
    if (particles_.empty()) return;

    const float sizeMin = 18.0f;
    const float sizeMax = std::max(sizeMin, maxPointSize_);
    std::uniform_real_distribution<float> distAngleJitter(-6.0f, 6.0f);
    std::uniform_real_distribution<float> distSpeed(260.0f, 650.0f);
    std::uniform_real_distribution<float> distLife(0.45f, 0.75f);
    std::uniform_real_distribution<float> distSize(sizeMin, sizeMax);
    std::uniform_real_distribution<float> distRadius(arcInner_, arcOuter_);
    std::uniform_real_distribution<float> distMix(0.1f, 0.35f);

    for (int i = 0; i < count; i++) {
        AttackParticle& p = particles_[static_cast<size_t>(nextIndex_)];
        nextIndex_ = (nextIndex_ + 1) % maxParticles_;

        float sweepAngle = sweepAngleDeg + distAngleJitter(rng_);
        float angleRad = DegToRad(sweepAngle);
        float radialX = std::cos(angleRad);
        float radialY = std::sin(angleRad);
        float tangentX = -radialY;
        float tangentY = radialX;

        float speed = distSpeed(rng_);
        float life = distLife(rng_);
        float size = distSize(rng_);
        if (size > maxPointSize_) size = maxPointSize_;

        float radius = distRadius(rng_);
        p.x = originX_ + radialX * radius;
        p.y = originY_ + radialY * radius;

        float mix = distMix(rng_);
        float dirX = tangentX * (1.0f - mix) + radialX * mix;
        float dirY = tangentY * (1.0f - mix) + radialY * mix;
        float dirLen = std::sqrt(dirX * dirX + dirY * dirY);
        if (dirLen > 0.0001f) {
            dirX /= dirLen;
            dirY /= dirLen;
        }
        p.vx = dirX * speed;
        p.vy = dirY * speed;
        p.maxLife = life;
        p.life = life;
        p.baseSize = size;
    }
}

void AttackPass::beginSlash(float centerDeg)
{
    sweepCenterDeg_ = centerDeg;
    sweepStartDeg_ = sweepCenterDeg_ - sweepSpanDeg_ * 0.5f;
    sweepEndDeg_ = sweepCenterDeg_ + sweepSpanDeg_ * 0.5f;
    slashTimer_ = 0.0f;
    spawnAccumulator_ = 0.0f;
    shockTimer_ = 0.0f;
}

void AttackPass::updateOrigin()
{
    originX_ = static_cast<float>(width_) * 0.5f;
    originY_ = static_cast<float>(height_) * 0.72f;
}

void AttackPass::onTouch(float x, float y, int action, int pointerId)
{
    setTouch(x, y, action, pointerId);
}

void AttackPass::setTouch(float x, float y, int action, int pointerId)
{
    (void)action;
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return;
    }
    if (width_ > 0 && height_ > 0) {
        x = std::max(0.0f, std::min(x, static_cast<float>(width_)));
        y = std::max(0.0f, std::min(y, static_cast<float>(height_)));
    }
    float dx = 0.0f;
    float dy = 0.0f;
    bool hasDir = false;
    if (hasLastTouch_ && pointerId == lastPointerId_) {
        dx = x - lastTouchX_;
        dy = y - lastTouchY_;
        if ((dx * dx + dy * dy) > 16.0f) {
            hasDir = true;
        }
    }

    lastTouchX_ = x;
    lastTouchY_ = y;
    lastPointerId_ = pointerId;
    hasLastTouch_ = true;

    if (touchCooldown_ > 0.0f) {
        return;
    }
    touchCooldown_ = 0.12f;

    originX_ = x;
    originY_ = y;

    float center = sweepCenterDeg_;
    if (hasDir) {
        center = std::atan2(dy, dx) * 180.0f / kPi;
    }
    beginSlash(center);
    idleTimer_ = 0.0f;
}

} // namespace glex
