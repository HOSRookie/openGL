#pragma once

#include <atomic>

namespace glex {

struct GLResourceStats {
    int programs = 0;
    int shaders = 0;
    int buffers = 0;
    int vaos = 0;
    int textures = 0;
};

class GLResourceTracker {
public:
    static GLResourceTracker& Get();

    void OnCreateProgram(int count = 1);
    void OnDeleteProgram(int count = 1);
    void OnCreateShader(int count = 1);
    void OnDeleteShader(int count = 1);
    void OnCreateBuffer(int count = 1);
    void OnDeleteBuffer(int count = 1);
    void OnCreateVertexArray(int count = 1);
    void OnDeleteVertexArray(int count = 1);
    void OnCreateTexture(int count = 1);
    void OnDeleteTexture(int count = 1);

    GLResourceStats GetStats() const;

private:
    GLResourceTracker() = default;
    void Adjust(std::atomic<int>& counter, int delta);

    std::atomic<int> programs_{0};
    std::atomic<int> shaders_{0};
    std::atomic<int> buffers_{0};
    std::atomic<int> vaos_{0};
    std::atomic<int> textures_{0};
};

} // namespace glex
