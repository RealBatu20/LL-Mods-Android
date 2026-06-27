#include "binding/BindingRegistry.hpp"

#include <fmt/format.h>

#include <stdexcept>

#include "lua/LuaScriptHost.hpp"
#include "util/Log.hpp"

namespace bedrocklua::binding {

void unavailable(lua::LuaScriptHost& /*host*/, const char* api, const char* signatureName) {
    // Thrown as a C++ exception; sol2 (with safeties on) converts it into a
    // catchable Lua error, so a script can pcall around offset-dependent APIs.
    throw std::runtime_error(
        fmt::format("{} unavailable: engine signature '{}' is unresolved on this build",
                    api, signatureName));
}

void installAll(lua::LuaScriptHost& host) {
    sol::state& lua = host.state().sol();

    // @minecraft/server
    sol::table server = lua.create_table();
    installSystem(host, server);
    installVector3(host, server);
    installWorld(host, server);     // must run before installEvents (adds world)
    installDimension(host, server);
    installPlayer(host, server);
    installEntity(host, server);
    installBlock(host, server);
    installItemStack(host, server);
    installEvents(host, server);

    // @minecraft/server-ui
    sol::table serverUi = lua.create_table();
    installServerUi(host, serverUi);

    // Expose the modules through Lua's require() via package.preload, plus an
    // import() convenience that mirrors the JS Script API ergonomics.
    sol::table package = lua["package"];
    sol::table preload = package["preload"];
    preload["@minecraft/server"] = [server](sol::this_state) -> sol::table { return server; };
    preload["@minecraft/server-ui"] = [serverUi](sol::this_state) -> sol::table {
        return serverUi;
    };

    lua.set_function("import", [&lua](const std::string& name) -> sol::object {
        sol::protected_function require = lua["require"];
        auto r = require(name);
        if (!r.valid()) {
            sol::error err = r;
            throw std::runtime_error(err.what());
        }
        return r;
    });

    log::debug("[{}] @minecraft/server + @minecraft/server-ui registered", host.packId());
}

}  // namespace bedrocklua::binding
