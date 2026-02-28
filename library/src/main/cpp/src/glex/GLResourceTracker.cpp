#include "glex/GLResourceTracker.h"

#include <algorithm>

namespace glex {

GLResourceTracker& GLResourceTracker::Get()
{
    static GLResourceTracker tracker;
    return tracker;
}

void GLResourceTracker::Adjust(std::atomic<int>& counter, int delta)
{
    int value = counter.load();
    while (true) {
        int next = std::max(0, value + delta);
        if (counter.compare_exchange_weak(value, next)) {
            return;
        }
    }
}

void GLResourceTracker::OnCreateProgram(int count)
{
    Adjust(programs_, count);
}

void GLResourceTracker::OnDeleteProgram(int count)
{
    Adjust(programs_, -count);
}

void GLResourceTracker::OnCreateShader(int count)
{
    Adjust(shaders_, count);
}

void GLResourceTracker::OnDeleteShader(int count)
{
    Adjust(shaders_, -count);
}

void GLResourceTracker::OnCreateBuffer(int count)
{
    Adjust(buffers_, count);
}

void GLResourceTracker::OnDeleteBuffer(int count)
{
    Adjust(buffers_, -count);
}

void GLResourceTracker::OnCreateVertexArray(int count)
{
    Adjust(vaos_, count);
}

void GLResourceTracker::OnDeleteVertexArray(int count)
{
    Adjust(vaos_, -count);
}

void GLResourceTracker::OnCreateTexture(int count)
{
    Adjust(textures_, count);
}

void GLResourceTracker::OnDeleteTexture(int count)
{
    Adjust(textures_, -count);
}

GLResourceStats GLResourceTracker::GetStats() const
{
    GLResourceStats stats;
    stats.programs = programs_.load();
    stats.shaders = shaders_.load();
    stats.buffers = buffers_.load();
    stats.vaos = vaos_.load();
    stats.textures = textures_.load();
    return stats;
}

} // namespace glex
