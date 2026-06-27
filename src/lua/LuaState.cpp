#include "lua/LuaState.hpp"

#include "util/Log.hpp"

namespace bedrocklua::lua {

namespace {
// Panic handler: sol2 already converts Lua panics into C++ exceptions when
// safeties are on, but we install our own so an uncaught panic logs instead of
// calling abort() and taking the game down.
int onPanic(lua_State* L) {
    const char* msg = lua_tostring(L, -1);
    log::error("Lua panic: {}", msg ? msg : "(unknown)");
    return 0;  // returning lets Lua perform a longjmp rather than abort
}
}  // namespace

LuaState::LuaState() {
    lua_atpanic(state_.lua_state(), &onPanic);
    openSafeLibraries();
    installHandlers();
}

LuaState::~LuaState() = default;

void LuaState::openSafeLibraries() {
    // Note: io is deliberately *not* opened, and os is trimmed below. Scripts
    // run with file/process access removed so a behavior pack cannot reach
    // outside the sandbox.
    state_.open_libraries(sol::lib::base, sol::lib::coroutine, sol::lib::string,
                          sol::lib::table, sol::lib::math, sol::lib::utf8,
                          sol::lib::os, sol::lib::package);

    sol::table os = state_["os"];
    if (os.valid()) {
        // Keep only the harmless time helpers.
        os["execute"] = sol::nil;
        os["exit"] = sol::nil;
        os["remove"] = sol::nil;
        os["rename"] = sol::nil;
        os["tmpname"] = sol::nil;
        os["getenv"] = sol::nil;
        os["setlocale"] = sol::nil;
    }

    // dofile/loadfile reach the filesystem; drop them. require stays so packs
    // can split into multiple lua files via package.path (set per host).
    state_["dofile"] = sol::nil;
    state_["loadfile"] = sol::nil;

    sol::table package = state_["package"];
    if (package.valid()) {
        package["loadlib"] = sol::nil;
    }
}

void LuaState::installHandlers() {
    // A reusable traceback handler exposed to event dispatch. Uses Lua's own
    // debug.traceback so script stacks survive into the log.
    state_.script(R"(
        function __blua_traceback(err)
            local tb = debug and debug.traceback
            if tb then return tb(tostring(err), 2) end
            return tostring(err)
        end
    )");
}

sol::protected_function_result LuaState::safeCall(const sol::protected_function& fn) {
    sol::protected_function call = fn;
    sol::protected_function handler = state_["__blua_traceback"];
    if (handler.valid()) call.error_handler = handler;
    return call();
}

Result<void> LuaState::runFile(const std::filesystem::path& file) {
    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) {
        return Result<void>::fail("entry script not found: " + file.string());
    }
    sol::protected_function_result r =
        state_.safe_script_file(file.string(), sol::script_pass_on_error);
    if (!r.valid()) {
        sol::error err = r;
        log::error("script error in {}: {}", file.string(), err.what());
        return Result<void>::fail(err.what());
    }
    return Result<void>::success();
}

Result<void> LuaState::runString(const std::string& source, const std::string& chunkName) {
    sol::protected_function_result r = state_.safe_script(
        source, sol::script_pass_on_error, chunkName);
    if (!r.valid()) {
        sol::error err = r;
        log::error("script error in {}: {}", chunkName, err.what());
        return Result<void>::fail(err.what());
    }
    return Result<void>::success();
}

}  // namespace bedrocklua::lua
