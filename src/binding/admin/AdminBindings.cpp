#include "binding/BindingRegistry.hpp"

#include "lua/LuaScriptHost.hpp"
#include "util/Log.hpp"

// @bedrocklua-admin - server administration. broadcast/log are offset-free and
// work (to logcat); the privileged operations resolve their engine signatures at
// runtime and degrade to catchable errors until those are verified.

namespace bedrocklua::binding {

void installAdmin(lua::LuaScriptHost& host, sol::table& admin) {
    admin.set_function("broadcast", [&host](const std::string& message) {
        log::info("[{}] admin.broadcast: {}", host.packId(), message);
    });
    admin.set_function("log", [&host](const std::string& message) {
        log::info("[{}] admin.log: {}", host.packId(), message);
    });
    admin.set_function("listPlayers", [&host]() -> sol::table {
        // Offset-dependent enumeration; returns an empty list until verified.
        return host.state().sol().create_table();
    });
    admin.set_function("runConsoleCommand", [&host](const std::string&) {
        unavailable(host, "admin.runConsoleCommand", "MinecraftCommands::executeCommand");
    });
    admin.set_function("kick", [&host](const std::string&, sol::object) {
        unavailable(host, "admin.kick", "ServerNetworkHandler::disconnectClient");
    });
    admin.set_function("op", [&host](const std::string&) {
        unavailable(host, "admin.op", "Player::setPermissions");
    });
    admin.set_function("deop", [&host](const std::string&) {
        unavailable(host, "admin.deop", "Player::setPermissions");
    });
    admin.set_function("setGameRule", [&host](const std::string&, sol::object) {
        unavailable(host, "admin.setGameRule", "GameRules::setRule");
    });
    admin.set_function("getGameRule", [&host](const std::string&) {
        unavailable(host, "admin.getGameRule", "GameRules::getRule");
    });
    admin.set_function("stopServer", [&host]() {
        unavailable(host, "admin.stopServer", "ServerInstance::leaveGameSync");
    });
}

}  // namespace bedrocklua::binding
