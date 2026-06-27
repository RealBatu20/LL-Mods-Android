// bedrocklua entry point (self-contained native mod).
//
// LeviLauncher dlopen()s the mod's .so; our constructor fires on load and spawns
// a detached thread that waits for libminecraftpe.so to be mapped, then drives
// the mod lifecycle. This mirrors the GlossHook-based reference mods and needs
// no preloader registration. If the game library never appears we still come up
// in "offline" mode, where the fallback ticker keeps the Lua runtime alive.

#include <Gloss.h>

#include <chrono>
#include <thread>

#include "mod/BedrockLuaMod.hpp"
#include "util/Log.hpp"

namespace {

void initThread() {
    using namespace std::chrono_literals;

    bool ready = false;
    for (int attempt = 0; attempt < 480; ++attempt) {  // up to ~120s @ 250ms
        if (GlossGetLibBias("libminecraftpe.so") != 0) {
            ready = true;
            break;
        }
        std::this_thread::sleep_for(250ms);
    }

    auto& mod = bedrocklua::BedrockLuaMod::getInstance();
    if (ready) {
        bedrocklua::log::info("libminecraftpe.so detected; initialising");
    } else {
        bedrocklua::log::warn("libminecraftpe.so not detected after timeout; "
                              "starting in offline mode (fallback ticker)");
    }

    if (!mod.load()) {
        bedrocklua::log::error("bedrocklua load() failed");
        return;
    }
    mod.enable();
}

}  // namespace

__attribute__((constructor)) static void bedrocklua_ctor() {
    bedrocklua::log::info("bedrocklua injected; spawning init thread");
    std::thread(initThread).detach();
}

// No __attribute__((destructor)): teardown (stopping the fallback ticker,
// removing hooks) lives in BedrockLuaMod's own destructor, which the runtime
// destroys with a live `this` on both process exit and dlclose — avoiding the
// static-destruction-order hazard of touching the singleton from a C destructor.
