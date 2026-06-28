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
    player.set_function("isOp", [&host](sol::table) {
        unavailable(host, "Player.isOp", "Player::getCommandPermissionLevel");
    });
    player.set_function("playSound", [&host](sol::table, const std::string&, sol::object) {
        unavailable(host, "Player.playSound", "Player::playSound");
    });
    player.set_function("getSpawnPoint", [&host](sol::table) {
        unavailable(host, "Player.getSpawnPoint", "Player::getSpawnPosition");
    });
    player.set_function("setSpawnPoint", [&host](sol::table, sol::object) {
        unavailable(host, "Player.setSpawnPoint", "Player::setRespawnPosition");
    });
    player.set_function("applyKnockback", [&host](sol::table, sol::object, sol::object) {
        unavailable(host, "Player.applyKnockback", "Actor::knockback");
    });
    // onScreenDisplay: returns a sub-object whose methods degrade cleanly.
    player.set_function("onScreenDisplay", [&host](sol::table) -> sol::table {
        sol::table osd = host.state().sol().create_table();
        osd.set_function("setActionBar", [&host](const std::string&) {
            unavailable(host, "Player.onScreenDisplay.setActionBar",
                        "ServerPlayer::sendNetworkPacket(SetTitle)");
        });
        osd.set_function("setTitle", [&host](sol::object, sol::object) {
            unavailable(host, "Player.onScreenDisplay.setTitle",
                        "ServerPlayer::sendNetworkPacket(SetTitle)");
        });
        return osd;
    });

    server["Player"] = player;
}

}  // namespace bedrocklua::binding
