#include "hook/HookManager.hpp"

#include <pl/cpp/Hook.hpp>

#include "mod/BedrockLuaMod.hpp"
#include "util/Log.hpp"

namespace bedrocklua {

bool HookManager::install(const std::string& label, std::uintptr_t target, void* detour,
                          void** original) {
    if (target == 0) {
        log::warn("[hook] {} skipped: target signature unresolved", label);
        return false;
    }
    pl::hook::hook(reinterpret_cast<void*>(target), detour, original,
                   pl::hook::PriorityNormal);
    entries_.push_back({label, reinterpret_cast<void*>(target), detour});
    log::info("[hook] {} installed at {:#x}", label, target);
    return true;
}

void HookManager::installGameHooks(Runtime& rt) {
    hooks::installLevelHooks(rt);
    hooks::installChatHooks(rt);
    hooks::installPackStackHooks(rt);
    hooks::installScriptEngineSeam(rt);
}

void HookManager::uninstallAll() {
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        pl::hook::unhook(it->target, it->detour);
    }
    entries_.clear();
}

}  // namespace bedrocklua
