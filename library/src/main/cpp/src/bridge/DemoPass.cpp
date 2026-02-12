#include "DemoPass.h"
#include "glex/Log.h"

#include <cmath>

namespace glex {

// ============================================================
// GLSL 着色器源码
// ============================================================

static const char* kBgVertSrc = R"(#version 300 es
layout(location = 0) in vec2 a_position;
out vec2 v_uv;
void main() {
    v_uv = a_position * 0.5 + 0.5;
    gl_Position = vec4(a_position, 0.0, 1.0);
}
)";

static const char* kBgFragSrc = R"(#version 300 es
precision highp float;
in vec2 v_uv;
out vec4 fragColor;
uniform float u_time;
void main() {
    // 深空渐变：顶部深蓝 → 底部暗蓝紫
    vec3 topColor = vec3(0.02, 0.03, 0.12);
    vec3 bottomColor = vec3(0.06, 0.04, 0.15);
    vec3 color = mix(bottomColor, topColor, v_uv.y);

    // 微弱的极光效果
    float aurora = sin(v_uv.x * 6.28 + u_time * 0.3) * 0.5 + 0.5;
    aurora *= smoothstep(0.4, 0.8, v_uv.y) * smoothstep(1.0, 0.7, v_uv.y);
    color += vec3(0.0, 0.08, 0.12) * aurora * 0.3;

    fragColor = vec4(color, 1.0);
}
)";

static const char* kStarVertSrc = R"(#version 300 es
layout(location = 0) in vec2 a_position;
layout(location = 1) in float a_size;
layout(location = 2) in vec4 a_color;

uniform mat4 u_projection;
uniform float u_time;

out vec4 v_color;

void main() {
    v_color = a_color;
    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
    gl_PointSize = a_size;
}
)";

static const char* kStarFragSrc = R"(#version 300 es
precision highp float;
in vec4 v_color;
out vec4 fragColor;
void main() {
    vec2 uv = gl_PointCoord * 2.0 - 1.0;
    float dist = dot(uv, uv);

    // 柔和的光晕
    float core = exp(-dist * 8.0);
    float glow = exp(-dist * 2.0) * 0.3;
    float alpha = core + glow;

    fragColor = vec4(v_color.rgb, v_color.a * alpha);
}
)";

static const char* kMeteorVertSrc = R"(#version 300 es
layout(location = 0) in vec2 a_position;
layout(location = 1) in float a_size;
layout(location = 2) in float a_alpha;

uniform mat4 u_projection;

out float v_alpha;

void main() {
    v_alpha = a_alpha;
    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
    gl_PointSize = a_size;
}
)";

static const char* kMeteorFragSrc = R"(#version 300 es
precision highp float;
in float v_alpha;
out vec4 fragColor;
void main() {
    vec2 uv = gl_PointCoord * 2.0 - 1.0;
    float dist = length(uv);
    float alpha = smoothstep(1.0, 0.0, dist);
    fragColor = vec4(0.9, 0.95, 1.0, alpha * v_alpha);
}
)";

// ============================================================
// 正交投影矩阵
// ============================================================

static void makeOrtho(float* m, float l, float r, float b, float t, float n, float f)
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

// ============================================================
// 实现
// ============================================================

void DemoPass::onInitialize(int width, int height)
{
    initStars();
    initMeteors();
    initGLResources();
    GLEX_LOGI("DemoPass initialized %{public}dx%{public}d, %{public}d stars", width, height, starCount_);
}

void DemoPass::onResize(int width, int height)
{
    initStars();
}

void DemoPass::onUpdate(float deltaTime)
{
    time_ += deltaTime;

    // 更新星星闪烁
    for (auto& star : stars_) {
        float twinkle = sinf(time_ * star.twinkleSpeed + star.twinklePhase);
        star.brightness = 0.5f + 0.5f * twinkle;
    }

    // 更新流星
    for (auto& m : meteors_) {
        if (!m.active) continue;
        m.x += m.vx * deltaTime;
        m.y += m.vy * deltaTime;
        m.life -= deltaTime;
        if (m.life <= 0.0f) {
            m.active = false;
        }
    }

    // 生成新流星
    meteorTimer_ += deltaTime;
    if (meteorTimer_ >= nextMeteorTime_) {
        meteorTimer_ = 0.0f;
        std::uniform_real_distribution<float> dist(2.0f, 6.0f);
        nextMeteorTime_ = dist(rng_);
        spawnMeteor();
    }
}

void DemoPass::onRender()
{
    if (!glReady_) return;

    float w = static_cast<float>(width_);
    float h = static_cast<float>(height_);

    // ---- 1. 渲染背景 ----
    bgShader_.use();
    bgShader_.setUniform1f("u_time", time_);
    glBindVertexArray(bgVao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    // ---- 2. 渲染星星 ----
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    // 准备星星顶点数据：[x, y, size, r, g, b, a]
    std::vector<float> starData;
    starData.reserve(stars_.size() * 7);
    for (const auto& s : stars_) {
        starData.push_back(s.x);
        starData.push_back(s.y);
        starData.push_back(s.size * (0.6f + 0.4f * s.brightness));
        starData.push_back(s.r);
        starData.push_back(s.g);
        starData.push_back(s.b);
        starData.push_back(s.brightness * 0.9f);
    }

    float proj[16];
    makeOrtho(proj, 0, w, h, 0, -1, 1);

    starShader_.use();
    starShader_.setUniformMatrix4fv("u_projection", proj);
    starShader_.setUniform1f("u_time", time_);

    glBindVertexArray(starVao_);
    glBindBuffer(GL_ARRAY_BUFFER, starVbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(starData.size() * sizeof(float)),
                 starData.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(stars_.size()));
    glBindVertexArray(0);

    // ---- 3. 渲染流星 ----
    struct MeteorVert { float x, y, size, alpha; };
    std::vector<MeteorVert> meteorData;
    for (const auto& m : meteors_) {
        if (!m.active) continue;
        float progress = m.life / m.maxLife;

        // 流星拖尾：沿速度方向生成多个点
        constexpr int TRAIL = 12;
        for (int i = 0; i < TRAIL; i++) {
            float t = static_cast<float>(i) / TRAIL;
            float trailAlpha = progress * (1.0f - t * 0.9f);
            float trailSize = m.size * (1.0f - t * 0.7f);
            MeteorVert v;
            v.x = m.x - m.vx * t * 0.15f;
            v.y = m.y - m.vy * t * 0.15f;
            v.size = trailSize;
            v.alpha = trailAlpha;
            meteorData.push_back(v);
        }
    }

    if (!meteorData.empty()) {
        meteorShader_.use();
        meteorShader_.setUniformMatrix4fv("u_projection", proj);

        glBindVertexArray(meteorVao_);
        glBindBuffer(GL_ARRAY_BUFFER, meteorVbo_);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(meteorData.size() * sizeof(MeteorVert)),
                     meteorData.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(meteorData.size()));
        glBindVertexArray(0);
    }

    glDisable(GL_BLEND);
}

void DemoPass::onDestroy()
{
    bgShader_.destroy();
    starShader_.destroy();
    meteorShader_.destroy();

    auto deleteVAO = [](GLuint& vao, GLuint& vbo) {
        if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
        if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
    };
    deleteVAO(bgVao_, bgVbo_);
    deleteVAO(starVao_, starVbo_);
    deleteVAO(meteorVao_, meteorVbo_);

    glReady_ = false;
    GLEX_LOGI("DemoPass destroyed");
}

// ============================================================
// 初始化辅助
// ============================================================

void DemoPass::initStars()
{
    stars_.clear();
    stars_.resize(static_cast<size_t>(starCount_));

    std::uniform_real_distribution<float> distX(0.0f, static_cast<float>(width_));
    std::uniform_real_distribution<float> distY(0.0f, static_cast<float>(height_));
    std::uniform_real_distribution<float> distSize(1.0f, 4.0f);
    std::uniform_real_distribution<float> distPhase(0.0f, 6.28f);
    std::uniform_real_distribution<float> distSpeed(0.3f, 2.0f);
    std::uniform_real_distribution<float> distColor(0.8f, 1.0f);
    std::uniform_real_distribution<float> distColorWarm(0.6f, 1.0f);

    for (auto& s : stars_) {
        s.x = distX(rng_);
        s.y = distY(rng_);
        s.size = distSize(rng_);
        s.twinklePhase = distPhase(rng_);
        s.twinkleSpeed = distSpeed(rng_);
        s.brightness = 1.0f;

        // 随机星星颜色：白色偏蓝/偏黄
        float colorType = distPhase(rng_);
        if (colorType < 2.0f) {
            // 冷白色
            s.r = distColor(rng_);
            s.g = distColor(rng_);
            s.b = 1.0f;
        } else if (colorType < 4.0f) {
            // 暖白色
            s.r = 1.0f;
            s.g = distColorWarm(rng_);
            s.b = distColorWarm(rng_) * 0.8f;
        } else {
            // 纯白
            s.r = 1.0f;
            s.g = 1.0f;
            s.b = 1.0f;
        }
    }
}

void DemoPass::initMeteors()
{
    meteors_.resize(MAX_METEORS);
    for (auto& m : meteors_) {
        m.active = false;
    }
}

void DemoPass::spawnMeteor()
{
    for (auto& m : meteors_) {
        if (m.active) continue;

        std::uniform_real_distribution<float> distX(0.0f, static_cast<float>(width_));
        std::uniform_real_distribution<float> distSpeed(300.0f, 800.0f);
        std::uniform_real_distribution<float> distLife(0.5f, 1.5f);
        std::uniform_real_distribution<float> distAngle(0.5f, 1.2f);

        float angle = distAngle(rng_);
        float speed = distSpeed(rng_);

        m.x = distX(rng_);
        m.y = 0.0f;
        m.vx = cosf(angle) * speed;
        m.vy = sinf(angle) * speed;
        m.maxLife = distLife(rng_);
        m.life = m.maxLife;
        m.size = 3.0f;
        m.active = true;
        break;
    }
}

void DemoPass::initGLResources()
{
    if (glReady_) return;

    // ---- 背景 ----
    bgShader_.build(kBgVertSrc, kBgFragSrc);

    float bgQuad[] = { -1, -1,  1, -1,  -1, 1,  1, 1 };
    glGenVertexArrays(1, &bgVao_);
    glGenBuffers(1, &bgVbo_);
    glBindVertexArray(bgVao_);
    glBindBuffer(GL_ARRAY_BUFFER, bgVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(bgQuad), bgQuad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindVertexArray(0);

    // ---- 星星 ----
    starShader_.build(kStarVertSrc, kStarFragSrc);

    glGenVertexArrays(1, &starVao_);
    glGenBuffers(1, &starVbo_);
    glBindVertexArray(starVao_);
    glBindBuffer(GL_ARRAY_BUFFER, starVbo_);
    // [x, y, size, r, g, b, a] = 7 floats per star
    constexpr int STRIDE = 7 * sizeof(float);
    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, STRIDE, (void*)0);
    glEnableVertexAttribArray(1); // size
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, STRIDE, (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(2); // color
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, STRIDE, (void*)(3 * sizeof(float)));
    glBindVertexArray(0);

    // ---- 流星 ----
    meteorShader_.build(kMeteorVertSrc, kMeteorFragSrc);

    glGenVertexArrays(1, &meteorVao_);
    glGenBuffers(1, &meteorVbo_);
    glBindVertexArray(meteorVao_);
    glBindBuffer(GL_ARRAY_BUFFER, meteorVbo_);
    // [x, y, size, alpha] = 4 floats per point
    constexpr int MSTRIDE = 4 * sizeof(float);
    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, MSTRIDE, (void*)0);
    glEnableVertexAttribArray(1); // size
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, MSTRIDE, (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(2); // alpha
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, MSTRIDE, (void*)(3 * sizeof(float)));
    glBindVertexArray(0);

    glReady_ = true;
}

} // namespace glex
