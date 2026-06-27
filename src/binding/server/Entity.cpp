#include "binding/BindingRegistry.hpp"

#include "lua/LuaScriptHost.hpp"

// Entity - @minecraft/server `Entity`. The class table documents the surface;
// instances are produced by the engine. Accessors raise a catchable error until
// the corresponding Actor signatures are verified for the target build.

namespace bedrocklua::binding {

void installEntity(lua::LuaScriptHost& host, sol::table& server) {
    sol::state& lua = host.state().sol();
    sol::table entity = lua.create_table();

    entity.set_function("getNameTag", [&host](sol::table) {
        unavailable(host, "Entity.nameTag", "Actor::getNameTag");
    });
    entity.set_function("getLocation", [&host](sol::table) {
        unavailable(host, "Entity.location", "Actor::getPosition");
    });
    entity.set_function("kill", [&host](sol::table) {
        unavailable(host, "Entity.kill", "Actor::kill");
    });
    entity.set_function("remove", [&host](sol::table) {
        unavailable(host, "Entity.remove", "Actor::remove");
    });
    entity.set_function("teleport", [&host](sol::table, sol::table) {
        unavailable(host, "Entity.teleport", "Actor::teleport");
    });
    entity.set_function("addEffect", [&host](sol::table, const std::string&, int) {
        unavailable(host, "Entity.addEffect", "Actor::addEffect");
    });
    entity.set_function("getComponent", [&host](sol::table, const std::string&) {
        unavailable(host, "Entity.getComponent", "Actor::getComponent");
    });

    server["Entity"] = entity;
}

}  // namespace bedrocklua::binding
