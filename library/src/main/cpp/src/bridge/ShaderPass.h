#pragma once

/**
 * @file ShaderPass.h
 * @brief 可编程 Shader 渲染阶段
 *
 * 提供可由 ArkTS 侧传入的自定义顶点/片元着色器，
 * 通过 RenderPass 接入渲染管线，用于替代内置 DemoPass。
 */

#include <string>
#include <unordered_map>
#include <vector>

#include <GLES3/gl3.h>

#include "glex/RenderPass.h"
#include "glex/ShaderProgram.h"

namespace glex {

class ShaderPass : public RenderPass {
public:
    ShaderPass() : RenderPass("ShaderPass") {}

    void setShaderSources(const std::string& vert, const std::string& frag);

    void setUniform(const std::string& name, const std::vector<float>& values);

protected:
    void onInitialize(int width, int height) override;
    void onResize(int width, int height) override;
    void onUpdate(float deltaTime) override;
    void onRender() override;
    void onDestroy() override;

private:
    void buildProgram();
    void applyUniforms();

    std::string vertexSrc_;
    std::string fragmentSrc_;
    bool needsRebuild_ = false;

    ShaderProgram shader_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;

    float time_ = 0.0f;

    std::unordered_map<std::string, std::vector<float>> uniforms_;
};

} // namespace glex
