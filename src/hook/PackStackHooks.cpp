#include "hook/HookManager.hpp"

#include "mod/BedrockLuaMod.hpp"
#include "sig/SignatureRegistry.hpp"
#include "util/Log.hpp"

// PackStackHooks observes when the active resource/behavior pack stack changes
// (ResourcePackManager::setStack). Each change triggers a rescan so packs that
// were just enabled for the current world get their Lua hosts created. The hook
// is a notify-and-forward seam: it does not read the stack's internal layout
// (which needs version-specific offsets), it simply prompts a rescan of the
// known behavior-pack roots, which is idempotent thanks to loaded-pack dedup.

namespace bedrocklua::hooks {

namespace {
using SetStackFn = void (*)(void* self, void* stack, int packType, bool b);
SetStackFn g_origSetStack = nullptr;

void SetStack_detour(void* self, void* stack, int packType, bool b) {
    if (g_origSetStack) {
        g_origSetStack(self, stack, packType, b);
    }
    log::debug("[packstack] stack changed; scheduling rescan");
    BedrockLuaMod::getInstance().onWorldLoad();
}
}  // namespace

bool installPackStackHooks(Runtime& rt) {
    std::uintptr_t target = rt.signatures.resolve("ResourcePackManager::setStack");
    return rt.hooks.install("ResourcePackManager::setStack", target,
                            reinterpret_cast<void*>(&SetStack_detour),
                            reinterpret_cast<void**>(&g_origSetStack));
}

}  // namespace bedrocklua::hooks
