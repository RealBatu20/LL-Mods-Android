#pragma once

// LuaState - owns one sol::state for a single Lua behavior pack.
//
// Responsible for opening a safe subset of the standard library, installing a
// panic + error handler that turns script faults into logged messages (never a
// process abort), and exposing helpers to run files and protected calls.

#include <sol/sol.hpp>

#include <filesystem>
#include <string>

#include "util/Result.hpp"

namespace bedrocklua::lua {

class LuaState {
public:
    LuaState();
    ~LuaState();

    LuaState(const LuaState&) = delete;
    LuaState& operator=(const LuaState&) = delete;

    sol::state& sol() { return state_; }
    lua_State* raw() { return state_.lua_state(); }

    // Runs a script file inside a protected call. Errors (parse or runtime) are
    // returned as a Result failure with a traceback, and also logged.
    Result<void> runFile(const std::filesystem::path& file);

    // Runs a string of Lua source (used for the small bootstrap shim).
    Result<void> runString(const std::string& source, const std::string& chunkName);

    // The shared protected-call error handler (adds a traceback). Exposed so
    // event dispatch can route listener calls through it.
    sol::protected_function_result safeCall(const sol::protected_function& fn);

    template <typename... Args>
    sol::protected_function_result safeCall(const sol::protected_function& fn, Args&&... args) {
        return fn(std::forward<Args>(args)...);
    }

private:
    void openSafeLibraries();
    void installHandlers();

    sol::state state_;
};

}  // namespace bedrocklua::lua
