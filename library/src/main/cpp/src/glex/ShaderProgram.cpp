#include "glex/ShaderProgram.h"
#include "glex/GLResourceTracker.h"
#include "glex/Log.h"

namespace glex {

ShaderProgram::~ShaderProgram()
{
    destroy();
}

bool ShaderProgram::build(const std::string& vertexSource, const std::string& fragmentSource)
{
    // 先清理旧的
    destroy();

    GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexSource);
    if (vertex == 0) {
        return false;
    }

    GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (fragment == 0) {
        glDeleteShader(vertex);
        GLResourceTracker::Get().OnDeleteShader();
        return false;
    }

    program_ = glCreateProgram();
    if (program_ == 0) {
        GLEX_LOGE("Failed to create shader program");
        glDeleteShader(vertex);
        GLResourceTracker::Get().OnDeleteShader();
        glDeleteShader(fragment);
        GLResourceTracker::Get().OnDeleteShader();
        return false;
    }
    GLResourceTracker::Get().OnCreateProgram();

    glAttachShader(program_, vertex);
    glAttachShader(program_, fragment);
    glLinkProgram(program_);

    GLint linked = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint infoLen = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 0) {
            std::string info(static_cast<size_t>(infoLen), '\0');
            glGetProgramInfoLog(program_, infoLen, nullptr, info.data());
            GLEX_LOGE("Shader link error: %{public}s", info.c_str());
        }
        glDeleteShader(vertex);
        GLResourceTracker::Get().OnDeleteShader();
        glDeleteShader(fragment);
        GLResourceTracker::Get().OnDeleteShader();
        glDeleteProgram(program_);
        GLResourceTracker::Get().OnDeleteProgram();
        program_ = 0;
        return false;
    }

    glDeleteShader(vertex);
    GLResourceTracker::Get().OnDeleteShader();
    glDeleteShader(fragment);
    GLResourceTracker::Get().OnDeleteShader();

    uniformCache_.clear();
    GLEX_LOGI("Shader program built: id=%{public}u", program_);
    return true;
}

void ShaderProgram::use() const
{
    if (program_ != 0) {
        glUseProgram(program_);
    }
}

void ShaderProgram::destroy()
{
    if (program_ != 0) {
        glDeleteProgram(program_);
        GLResourceTracker::Get().OnDeleteProgram();
        program_ = 0;
    }
    uniformCache_.clear();
}

// ============================================================
// Uniform 操作
// ============================================================

GLint ShaderProgram::getUniformLocation(const std::string& name)
{
    auto it = uniformCache_.find(name);
    if (it != uniformCache_.end()) {
        return it->second;
    }
    GLint loc = glGetUniformLocation(program_, name.c_str());
    uniformCache_[name] = loc;
    return loc;
}

void ShaderProgram::setUniform1i(const std::string& name, int value)
{
    GLint loc = getUniformLocation(name);
    if (loc >= 0) {
        glUniform1i(loc, value);
    }
}

void ShaderProgram::setUniform1f(const std::string& name, float value)
{
    GLint loc = getUniformLocation(name);
    if (loc >= 0) {
        glUniform1f(loc, value);
    }
}

void ShaderProgram::setUniform2f(const std::string& name, float v0, float v1)
{
    GLint loc = getUniformLocation(name);
    if (loc >= 0) {
        glUniform2f(loc, v0, v1);
    }
}

void ShaderProgram::setUniform3f(const std::string& name, float v0, float v1, float v2)
{
    GLint loc = getUniformLocation(name);
    if (loc >= 0) {
        glUniform3f(loc, v0, v1, v2);
    }
}

void ShaderProgram::setUniform4f(const std::string& name, float v0, float v1, float v2, float v3)
{
    GLint loc = getUniformLocation(name);
    if (loc >= 0) {
        glUniform4f(loc, v0, v1, v2, v3);
    }
}

void ShaderProgram::setUniformMatrix4fv(const std::string& name, const float* value, bool transpose)
{
    GLint loc = getUniformLocation(name);
    if (loc >= 0) {
        glUniformMatrix4fv(loc, 1, transpose ? GL_TRUE : GL_FALSE, value);
    }
}

GLint ShaderProgram::getAttribLocation(const std::string& name) const
{
    return glGetAttribLocation(program_, name.c_str());
}

// ============================================================
// 内部方法
// ============================================================

GLuint ShaderProgram::compileShader(GLenum type, const std::string& source)
{
    if (source.empty()) {
        GLEX_LOGE("Shader source is empty");
        return 0;
    }

    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        GLEX_LOGE("Failed to create shader (type=%{public}d)", type);
        return 0;
    }
    GLResourceTracker::Get().OnCreateShader();

    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 0) {
            std::string info(static_cast<size_t>(infoLen), '\0');
            glGetShaderInfoLog(shader, infoLen, nullptr, info.data());
            const char* typeName = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
            GLEX_LOGE("[%{public}s] Compile error: %{public}s", typeName, info.c_str());
        }
        glDeleteShader(shader);
        GLResourceTracker::Get().OnDeleteShader();
        return 0;
    }

    return shader;
}

} // namespace glex
