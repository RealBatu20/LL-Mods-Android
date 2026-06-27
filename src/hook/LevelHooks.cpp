#include "hook/HookManager.hpp"

#include "mod/BedrockLuaMod.hpp"
#include "sig/SignatureRegistry.hpp"
#include "util/Log.hpp"

// LevelHooks drives the per-tick scheduler and fires the world-load event.
//
// Level::tick runs once per game tick on the main thread, which is exactly the
// cadence the Lua scheduler (system.run / runInterval / runTimeout) needs and
// the only thread it is safe to call Lua from. The first observed tick is
// treated as "world ready" and fires onWorldLoad().

namespace bedrocklua::hooks {

namespace {
using LevelTickFn = void (*)(void* self);
LevelTickFn g_origLevelTick = nullptr;
bool g_firstTick = false;

void Level_tick_detour(void* self) {
    if (g_origLevelTick) {
        g_origLevelTick(self);
    }
    auto& mod = BedrockLuaMod::getInstance();
    if (!g_firstTick) {
        g_firstTick = true;
        mod.onWorldLoad();
    }
    mod.onGameTick();
}
}  // namespace

bool installLevelHooks(Runtime& rt) {
    std::uintptr_t target = rt.signatures.resolve("Level::tick");
    return rt.hooks.install("Level::tick", target,
                            reinterpret_cast<void*>(&Level_tick_detour),
                            reinterpret_cast<void**>(&g_origLevelTick));
}

}  // namespace bedrocklua::hooks
