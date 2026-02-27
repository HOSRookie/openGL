#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "glex/RenderPass.h"

namespace glex {

using PassFactory = std::function<std::shared_ptr<RenderPass>()>;

bool RegisterPass(const std::string& name, PassFactory factory);
bool UnregisterPass(const std::string& name);
bool IsPassRegistered(const std::string& name);
std::shared_ptr<RenderPass> CreatePass(const std::string& name);
std::vector<std::string> ListRegisteredPasses();

} // namespace glex
