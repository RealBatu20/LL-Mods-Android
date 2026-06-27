#include "hook/HookManager.hpp"

#include "mod/BedrockLuaMod.hpp"
#include "sig/SignatureRegistry.hpp"
#include "util/Log.hpp"

// ScriptEngineSeam - the phase-2 attachment point for making "lua" a true
// first-class manifest language (alongside QuickJS-backed "javascript").
//
// Today bedrocklua loads Lua packs through its own PackScanner, fully decoupled
// from Minecraft's script subsystem, so this seam is INERT by default: it
// resolves and reports the script-binding initializer but installs no detour.
//
// When the engine integration is built out, the detour here would intercept the
// script module registration / manifest language validation so that a module
// declaring "language": "lua" is accepted by the engine and routed to our VM
// instead of being rejected as an unknown language. Keeping the resolution here
// means that work needs no new plumbing - only a detour body.

namespace bedrocklua::hooks {

bool installScriptEngineSeam(Runtime& rt) {
    std::uintptr_t target = rt.signatures.resolve("ScriptModuleMinecraft::registerBindings");
    if (target != 0) {
        log::info("[seam] script-engine integration point available at {:#x} "
                  "(phase-2; not hooked)", target);
    } else {
        log::debug("[seam] script-engine integration point unresolved (phase-2)");
    }
    // Intentionally installs nothing: the independent loader is the active path.
    return false;
}

}  // namespace bedrocklua::hooks
