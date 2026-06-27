#include "binding/BindingRegistry.hpp"

#include "lua/LuaScriptHost.hpp"

// Player - @minecraft/server `Player` (an Entity specialisation). The data-only
// surface is offset-free; engine accessors raise a catchable error until their
// signatures are verified.

namespace bedrocklua::binding {

void installPlayer(lua::LuaScriptHost& host, sol::table& server) {
    sol::state& lua = host.state().sol();
    sol::table player = lua.create_table();

    player.set_function("sendMessage", [&host](sol::table /*self*/, sol::object /*message*/) {
        // Targeted per-player send is gated on the packet/player accessors; use
        // world.sendMessage for the observable degraded path in the meantime.
        unavailable(host, "Player.sendMessage", "ServerPlayer::sendNetworkPacket");
    });
    player.set_function("getName", [&host](sol::table) {
        unavailable(host, "Player.name", "Actor::getNameTag");
    });
    player.set_function("getGameMode", [&host](sol::table) {
        unavailable(host, "Player.getGameMode", "Player::getPlayerGameType");
    });
    player.set_function("setGameMode", [&host](sol::table, int) {
        unavailable(host, "Player.setGameMode", "Player::setPlayerGameType");
    });
    player.set_function("getInventory", [&host](sol::table) {
        unavailable(host, "Player.getComponent('inventory')", "Player::getInventory");
    });
    player.set_function("runCommand", [&host](sol::table, const std::string&) {
        unavailable(host, "Player.runCommand", "MinecraftCommands::executeCommand");
    });
    player.set_function("kick", [&host](sol::table, sol::object) {
        unavailable(host, "Player.kick", "ServerNetworkHandler::disconnectClient");
    });

    server["Player"] = player;
}

}  // namespace bedrocklua::binding
