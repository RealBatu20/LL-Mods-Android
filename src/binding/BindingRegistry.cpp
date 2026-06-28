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

// bedrocklua's module API version. Each module table carries this as `.version`,
// mirroring how the vanilla @minecraft/* packages are versioned.
static constexpr const char* kModuleVersion = "0.1.0";

void installAll(lua::LuaScriptHost& host) {
    sol::state& lua = host.state().sol();

    // @bedrocklua - the core engine API (the equivalent of @minecraft/server).
    sol::table server = lua.create_table();
    installSystem(host, server);
    installConstants(host, server);
    installVector3(host, server);
    installWorld(host, server);     // must run before installEvents (adds world)
    installDimension(host, server);
    installPlayer(host, server);
    installEntity(host, server);
    installBlock(host, server);
    installItemStack(host, server);
    installEvents(host, server);
    server["version"] = kModuleVersion;

    // @bedrocklua-ui - forms (the equivalent of @minecraft/server-ui).
    sol::table ui = lua.create_table();
    installServerUi(host, ui);
    ui["version"] = kModuleVersion;

    // @bedrocklua-admin - server administration.
    sol::table admin = lua.create_table();
    installAdmin(host, admin);
    admin["version"] = kModuleVersion;

    // @bedrocklua-net - networking (HTTP fetch, hashing).
    sol::table net = lua.create_table();
    installNet(host, net);
    net["version"] = kModuleVersion;

    // Expose the modules through Lua's require()/import() via package.preload.
    // bedrocklua uses its own namespace - NOT the vanilla @minecraft/* names.
    sol::table package = lua["package"];
    sol::table preload = package["preload"];
    preload["@bedrocklua"] = [server](sol::this_state) -> sol::table { return server; };
    preload["@bedrocklua-ui"] = [ui](sol::this_state) -> sol::table { return ui; };
    preload["@bedrocklua-admin"] = [admin](sol::this_state) -> sol::table { return admin; };
    preload["@bedrocklua-net"] = [net](sol::this_state) -> sol::table { return net; };

    lua.set_function("import", [&lua](const std::string& name) -> sol::object {
        sol::protected_function require = lua["require"];
        auto r = require(name);
        if (!r.valid()) {
            sol::error err = r;
            throw std::runtime_error(err.what());
        }
        return r;
    });

    log::debug("[{}] registered @bedrocklua, -ui, -admin, -net v{}", host.packId(),
               kModuleVersion);
}

}  // namespace bedrocklua::binding
