#include "BuiltinPassRegistry.h"

#include <mutex>

#include "glex/PassRegistry.h"
#include "AttackPass.h"
#include "DemoPass.h"
#include "ShaderPass.h"

namespace glex {
namespace bridge {

void RegisterBuiltinPasses()
{
    static std::once_flag flag;
    std::call_once(flag, []() {
        RegisterPass("DemoPass", []() { return std::make_shared<DemoPass>(); });
        RegisterPass("AttackPass", []() { return std::make_shared<AttackPass>(); });
        RegisterPass("ShaderPass", []() { return std::make_shared<ShaderPass>(); });
    });
}

} // namespace bridge
} // namespace glex
