#include "hook/HookManager.hpp"

#include <Gloss.h>

#include "mod/BedrockLuaMod.hpp"
#include "util/Log.hpp"

namespace bedrocklua {

bool HookManager::install(const std::string& label, std::uintptr_t target, void* detour,
                          void** original) {
    if (target == 0) {
        log::warn("[hook] {} skipped: target signature unresolved", label);
        return false;
    }
    // GlossHook installs an inline hook and writes the trampoline (call the
    // original/next implementation) into *original. It auto-detects the
    // instruction set from the target address.
    GHook handle = GlossHook(reinterpret_cast<void*>(target), detour, original);
    if (handle == nullptr) {
        log::warn("[hook] {} failed: GlossHook returned null for {:#x}", label, target);
        return false;
    }
    entries_.push_back({label, handle});
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
        if (it->handle) GlossHookDelete(static_cast<GHook>(it->handle));
    }
    entries_.clear();
}

}  // namespace bedrocklua
