#pragma once

// BedrockLuaMod - the LeviLaunchroid native-mod entry object.
//
// It owns every long-lived subsystem (signature table, hook manager, the Lua
// event bus and the behavior-pack scanner) and exposes a singleton so the C
// hook trampolines installed in src/hook/* can reach back into the runtime.

#include <filesystem>
#include <memory>
#include <string>

namespace bedrocklua {

class SignatureRegistry;
class HookManager;
class PackScanner;

namespace lua {
class LuaEventBus;
}

// Bundles references to the live subsystems so bindings and script hosts can be
// constructed without depending on the mod singleton directly.
struct Runtime {
    SignatureRegistry& signatures;
    HookManager& hooks;
    lua::LuaEventBus& events;
    PackScanner& packs;
    std::filesystem::path modDir;
    std::filesystem::path dataDir;
};

class BedrockLuaMod {
public:
    static BedrockLuaMod& getInstance();

    // LeviLaunchroid lifecycle (see PL_REGISTER_MOD).
    bool load();
    bool enable();
    bool disable();
    bool unload();

    bool isEnabled() const { return enabled_; }

    Runtime& runtime();

    SignatureRegistry& signatures();
    HookManager& hooks();
    lua::LuaEventBus& events();
    PackScanner& packs();

    // Invoked from the engine hooks.
    void onGameTick();   // once per Level tick
    void onWorldLoad();  // when a level finishes loading / pack stack is ready

private:
    BedrockLuaMod();
    ~BedrockLuaMod();

    BedrockLuaMod(const BedrockLuaMod&) = delete;
    BedrockLuaMod& operator=(const BedrockLuaMod&) = delete;

    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool enabled_ = false;
};

}  // namespace bedrocklua
