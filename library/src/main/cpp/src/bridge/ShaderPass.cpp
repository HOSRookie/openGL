#include "ShaderPass.h"
#include "glex/GLResourceTracker.h"
#include "glex/Log.h"

namespace glex {

static const char* kDefaultVert = R"(#version 300 es
layout(location = 0) in vec2 a_position;
out vec2 v_uv;
void main() {
    v_uv = a_position * 0.5 + 0.5;
    gl_Position = vec4(a_position, 0.0, 1.0);
}
)";

static const char* kDefaultFrag = R"(#version 300 es
precision highp float;
in vec2 v_uv;
out vec4 fragColor;
uniform float u_time;
uniform vec2 u_resolution;
void main() {
    // 默认输出一个轻微渐变，避免纯黑
    vec3 top = vec3(0.05, 0.07, 0.12);
    vec3 bottom = vec3(0.02, 0.02, 0.04);
    vec3 color = mix(bottom, top, v_uv.y);
    color += 0.03 * sin(u_time + v_uv.xyx * 12.0);
    fragColor = vec4(color, 1.0);
}
)";

void ShaderPass::setShaderSources(const std::string& vert, const std::string& frag)
{
    vertexSrc_ = vert;
    fragmentSrc_ = frag;
    needsRebuild_ = true;
}

void ShaderPass::setUniform(const std::string& name, const std::vector<float>& values)
{
    if (name.empty() || values.empty()) {
        return;
    }
    uniforms_[name] = values;
}

void ShaderPass::onInitialize(int width, int height)
{
    (void)width;
    (void)height;

    // 初始化全屏四边形
    float quad[] = { -1, -1,  1, -1,  -1,  1,  1,  1 };
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    GLResourceTracker::Get().OnCreateVertexArray();
    GLResourceTracker::Get().OnCreateBuffer();
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glBindVertexArray(0);

    buildProgram();
    GLEX_LOGI("ShaderPass initialized");
}

void ShaderPass::onResize(int width, int height)
{
    (void)width;
    (void)height;
}

void ShaderPass::onUpdate(float deltaTime)
{
    time_ += deltaTime;
}

void ShaderPass::onRender()
{
    if (needsRebuild_) {
        buildProgram();
        needsRebuild_ = false;
    }

    if (!shader_.isValid()) {
        return;
    }

    shader_.use();
    shader_.setUniform1f("u_time", time_);
    shader_.setUniform2f("u_resolution", static_cast<float>(width_), static_cast<float>(height_));

    applyUniforms();

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void ShaderPass::onDestroy()
{
    shader_.destroy();
    if (vbo_) {
        glDeleteBuffers(1, &vbo_);
        GLResourceTracker::Get().OnDeleteBuffer();
        vbo_ = 0;
    }
    if (vao_) {
        glDeleteVertexArrays(1, &vao_);
        GLResourceTracker::Get().OnDeleteVertexArray();
        vao_ = 0;
    }
}

void ShaderPass::buildProgram()
{
    std::string vert = vertexSrc_.empty() ? kDefaultVert : vertexSrc_;
    std::string frag = fragmentSrc_.empty() ? kDefaultFrag : fragmentSrc_;

    if (!shader_.build(vert, frag)) {
        GLEX_LOGE("ShaderPass: shader build failed");
    }
}

void ShaderPass::applyUniforms()
{
    for (const auto& item : uniforms_) {
        const std::string& name = item.first;
        const std::vector<float>& v = item.second;
        if (v.size() == 1) {
            shader_.setUniform1f(name, v[0]);
        } else if (v.size() == 2) {
            shader_.setUniform2f(name, v[0], v[1]);
        } else if (v.size() == 3) {
            shader_.setUniform3f(name, v[0], v[1], v[2]);
        } else if (v.size() == 4) {
            shader_.setUniform4f(name, v[0], v[1], v[2], v[3]);
        } else if (v.size() == 16) {
            shader_.setUniformMatrix4fv(name, v.data(), false);
        }
    }
}

} // namespace glex
