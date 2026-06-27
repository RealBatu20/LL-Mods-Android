#pragma once

// LuaScriptHost - a loaded Lua behavior pack: its VM, scheduler and event
// listener tables. One host per pack that declares a `language: "lua"` script
// module.

#include <sol/sol.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "lua/LuaState.hpp"

namespace bedrocklua {
struct Runtime;
}

namespace bedrocklua::lua {

// Event categories mirror the @minecraft/server split.
enum class EventPhase { Before, After };

class LuaScriptHost {
public:
    LuaScriptHost(Runtime& rt, std::string packId, std::filesystem::path packDir);
    ~LuaScriptHost();

    LuaScriptHost(const LuaScriptHost&) = delete;
    LuaScriptHost& operator=(const LuaScriptHost&) = delete;

    // Builds the binding surface and runs the entry script.
    bool start(const std::filesystem::path& entryRelative);

    // Advances the scheduler by one game tick (drives system.run* callbacks).
    void tick(std::uint64_t currentTick);

    const std::string& packId() const { return packId_; }
    LuaState& state() { return state_; }
    Runtime& runtime() { return rt_; }
    std::uint64_t currentTick() const { return currentTick_; }

    // --- scheduler (backs system.run / runInterval / runTimeout) -----------
    int scheduleRun(sol::protected_function fn);                       // next tick
    int scheduleTimeout(sol::protected_function fn, std::uint32_t delayTicks);
    int scheduleInterval(sol::protected_function fn, std::uint32_t periodTicks);
    void clearRun(int handle);

    // --- events (backs world.afterEvents / world.beforeEvents) -------------
    void subscribe(EventPhase phase, const std::string& eventName, sol::protected_function fn);
    void unsubscribe(EventPhase phase, const std::string& eventName, sol::protected_function fn);

    // Dispatches an event to every subscriber. The factory builds the payload
    // table lazily on this host's own lua_State. Returns the payload object so
    // callers can inspect mutations such as a `cancel` flag set by a
    // beforeEvents listener.
    using PayloadFactory = std::function<sol::object(LuaScriptHost&)>;
    sol::object dispatch(EventPhase phase, const std::string& eventName,
                         const PayloadFactory& factory);

    bool hasListeners(EventPhase phase, const std::string& eventName);

private:
    struct ScheduledTask {
        int handle;
        sol::protected_function fn;
        std::uint64_t nextRunTick;
        std::uint32_t periodTicks;  // 0 => one-shot
        bool cancelled = false;
    };

    using ListenerMap = std::unordered_map<std::string, std::vector<sol::protected_function>>;
    ListenerMap& listenersFor(EventPhase phase);

    Runtime& rt_;
    std::string packId_;
    std::filesystem::path packDir_;
    LuaState state_;

    std::uint64_t currentTick_ = 0;
    int nextHandle_ = 1;
    std::vector<ScheduledTask> tasks_;

    ListenerMap afterListeners_;
    ListenerMap beforeListeners_;

    bool started_ = false;
};

}  // namespace bedrocklua::lua
