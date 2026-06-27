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
    entity.set_function("hasComponent", [&host](sol::table, const std::string&) {
        unavailable(host, "Entity.hasComponent", "Actor::hasComponent");
    });
    entity.set_function("getHealth", [&host](sol::table) {
        unavailable(host, "Entity.getComponent('health')", "Actor::getHealth");
    });
    entity.set_function("applyDamage", [&host](sol::table, double, sol::object) {
        unavailable(host, "Entity.applyDamage", "Actor::hurt");
    });
    entity.set_function("applyImpulse", [&host](sol::table, sol::table) {
        unavailable(host, "Entity.applyImpulse", "Actor::setVelocity");
    });
    entity.set_function("applyKnockback", [&host](sol::table, sol::object, sol::object) {
        unavailable(host, "Entity.applyKnockback", "Actor::knockback");
    });
    entity.set_function("getVelocity", [&host](sol::table) {
        unavailable(host, "Entity.getVelocity", "Actor::getVelocity");
    });
    entity.set_function("getRotation", [&host](sol::table) {
        unavailable(host, "Entity.getRotation", "Actor::getRotation");
    });
    entity.set_function("getViewDirection", [&host](sol::table) {
        unavailable(host, "Entity.getViewDirection", "Actor::getViewDirection");
    });
    entity.set_function("isOnGround", [&host](sol::table) {
        unavailable(host, "Entity.isOnGround", "Actor::isOnGround");
    });
    entity.set_function("triggerEvent", [&host](sol::table, const std::string&) {
        unavailable(host, "Entity.triggerEvent", "Actor::triggerEvent");
    });
    entity.set_function("addTag", [&host](sol::table, const std::string&) {
        unavailable(host, "Entity.addTag", "Actor::addTag");
    });
    entity.set_function("removeTag", [&host](sol::table, const std::string&) {
        unavailable(host, "Entity.removeTag", "Actor::removeTag");
    });
    entity.set_function("hasTag", [&host](sol::table, const std::string&) {
        unavailable(host, "Entity.hasTag", "Actor::hasTag");
    });
    entity.set_function("getTags", [&host](sol::table) {
        unavailable(host, "Entity.getTags", "Actor::getTags");
    });

    server["Entity"] = entity;
}

}  // namespace bedrocklua::binding
