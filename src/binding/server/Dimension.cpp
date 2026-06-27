#include "binding/BindingRegistry.hpp"

#include "lua/LuaScriptHost.hpp"

// Dimension - @minecraft/server `Dimension`. A dimension handle carries its id
// (offset-free) and exposes the engine operations as methods that raise a
// catchable error until their signatures are verified.

namespace bedrocklua::binding {

void installDimension(lua::LuaScriptHost& host, sol::table& server) {
    sol::state& lua = host.state().sol();

    // Factory used by world.getDimension to build a dimension handle.
    lua.set_function("__blua_make_dimension",
                     [&host](const std::string& id) -> sol::table {
                         sol::table dim = host.state().sol().create_table();
                         dim["id"] = id;
                         dim.set_function("runCommand", [&host](sol::table, const std::string&) {
                             unavailable(host, "Dimension.runCommand",
                                         "MinecraftCommands::executeCommand");
                         });
                         dim.set_function("runCommandAsync",
                                          [&host](sol::table, const std::string&) {
                                              unavailable(host, "Dimension.runCommandAsync",
                                                          "MinecraftCommands::executeCommand");
                                          });
                         dim.set_function("getBlock", [&host](sol::table, sol::table) {
                             unavailable(host, "Dimension.getBlock", "BlockSource::getBlock");
                         });
                         dim.set_function("spawnEntity", [&host](sol::table, const std::string&,
                                                                 sol::table) {
                             unavailable(host, "Dimension.spawnEntity", "Spawner::spawnMob");
                         });
                         dim.set_function("getEntities", [&host](sol::table, sol::object) {
                             unavailable(host, "Dimension.getEntities", "ActorFetcher::fetch");
                         });
                         return dim;
                     });

    // Convenience: well-known dimension ids.
    sol::table ids = lua.create_table();
    ids["overworld"] = "minecraft:overworld";
    ids["nether"] = "minecraft:nether";
    ids["the_end"] = "minecraft:the_end";
    server["DimensionTypes"] = ids;
}

}  // namespace bedrocklua::binding
