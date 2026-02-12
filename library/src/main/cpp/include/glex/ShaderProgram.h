#pragma once

/**
 * @file ShaderProgram.h
 * @brief OpenGL ES 着色器程序管理器
 *
 * 封装着色器编译、链接、Uniform 管理等操作。
 * 提供便捷的 Uniform 设置方法，自动缓存 location。
 *
 * 用法：
 *   ShaderProgram shader;
 *   shader.build(vertexSrc, fragmentSrc);
 *   shader.use();
 *   shader.setUniform1f("u_time", time);
 */

#include <GLES3/gl3.h>
#include <string>
#include <unordered_map>

namespace glex {

class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    /**
     * 编译并链接着色器程序
     * @param vertexSource 顶点着色器 GLSL 源码
     * @param fragmentSource 片段着色器 GLSL 源码
     * @return 成功返回 true
     */
    bool build(const std::string& vertexSource, const std::string& fragmentSource);

    /** 绑定此着色器程序 */
    void use() const;

    /** 获取程序 ID */
    GLuint getId() const { return program_; }

    /** 是否已成功构建 */
    bool isValid() const { return program_ != 0; }

    /** 销毁着色器程序 */
    void destroy();

    // ============================================================
    // Uniform 操作（自动缓存 location）
    // ============================================================

    /** 获取 Uniform 位置（带缓存） */
    GLint getUniformLocation(const std::string& name);

    void setUniform1i(const std::string& name, int value);
    void setUniform1f(const std::string& name, float value);
    void setUniform2f(const std::string& name, float v0, float v1);
    void setUniform3f(const std::string& name, float v0, float v1, float v2);
    void setUniform4f(const std::string& name, float v0, float v1, float v2, float v3);
    void setUniformMatrix4fv(const std::string& name, const float* value, bool transpose = false);

    // ============================================================
    // Attribute 操作
    // ============================================================

    /** 获取 Attribute 位置 */
    GLint getAttribLocation(const std::string& name) const;

private:
    GLuint compileShader(GLenum type, const std::string& source);

    GLuint program_ = 0;
    std::unordered_map<std::string, GLint> uniformCache_;
};

} // namespace glex
