#include "glex/PassRegistry.h"

#include <mutex>
#include <unordered_map>

namespace glex {

namespace {
std::mutex g_registryMutex;
std::unordered_map<std::string, PassFactory> g_factories;
} // namespace

bool RegisterPass(const std::string& name, PassFactory factory)
{
    if (name.empty() || !factory) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_registryMutex);
    if (g_factories.find(name) != g_factories.end()) {
        return false;
    }
    g_factories[name] = std::move(factory);
    return true;
}

bool UnregisterPass(const std::string& name)
{
    std::lock_guard<std::mutex> lock(g_registryMutex);
    return g_factories.erase(name) > 0;
}

bool IsPassRegistered(const std::string& name)
{
    std::lock_guard<std::mutex> lock(g_registryMutex);
    return g_factories.find(name) != g_factories.end();
}

std::shared_ptr<RenderPass> CreatePass(const std::string& name)
{
    PassFactory factory;
    {
        std::lock_guard<std::mutex> lock(g_registryMutex);
        auto it = g_factories.find(name);
        if (it == g_factories.end()) {
            return nullptr;
        }
        factory = it->second;
    }
    return factory ? factory() : nullptr;
}

std::vector<std::string> ListRegisteredPasses()
{
    std::vector<std::string> names;
    std::lock_guard<std::mutex> lock(g_registryMutex);
    names.reserve(g_factories.size());
    for (const auto& item : g_factories) {
        names.push_back(item.first);
    }
    return names;
}

} // namespace glex
