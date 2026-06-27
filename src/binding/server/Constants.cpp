#include "binding/BindingRegistry.hpp"

#include "lua/LuaScriptHost.hpp"

// Constants / enums from @minecraft/server. All offset-free string/number data,
// so these always work and give scripts the same vocabulary as the JS API.

namespace bedrocklua::binding {

namespace {
sol::table strEnum(sol::state_view lua, std::initializer_list<std::pair<const char*, const char*>> kv) {
    sol::table t = lua.create_table();
    for (auto& p : kv) t[p.first] = p.second;
    return t;
}
}  // namespace

void installConstants(lua::LuaScriptHost& host, sol::table& server) {
    sol::state& lua = host.state().sol();

    server["GameMode"] = strEnum(lua, {
        {"survival", "survival"}, {"creative", "creative"},
        {"adventure", "adventure"}, {"spectator", "spectator"},
    });

    server["MinecraftDimensionTypes"] = strEnum(lua, {
        {"overworld", "minecraft:overworld"},
        {"nether", "minecraft:nether"},
        {"theEnd", "minecraft:the_end"},
    });

    server["EntityComponentTypes"] = strEnum(lua, {
        {"health", "minecraft:health"},
        {"inventory", "minecraft:inventory"},
        {"equippable", "minecraft:equippable"},
        {"rideable", "minecraft:rideable"},
        {"tameable", "minecraft:tameable"},
        {"movement", "minecraft:movement"},
        {"isBaby", "minecraft:is_baby"},
    });

    server["ItemComponentTypes"] = strEnum(lua, {
        {"durability", "minecraft:durability"},
        {"enchantable", "minecraft:enchantable"},
        {"food", "minecraft:food"},
        {"cooldown", "minecraft:cooldown"},
    });

    server["BlockComponentTypes"] = strEnum(lua, {
        {"inventory", "minecraft:inventory"},
        {"sign", "minecraft:sign"},
        {"piston", "minecraft:piston"},
    });

    server["EquipmentSlot"] = strEnum(lua, {
        {"head", "Head"}, {"chest", "Chest"}, {"legs", "Legs"}, {"feet", "Feet"},
        {"mainhand", "Mainhand"}, {"offhand", "Offhand"},
    });

    server["EntityDamageCause"] = strEnum(lua, {
        {"entityAttack", "entityAttack"}, {"fall", "fall"}, {"fire", "fire"},
        {"drowning", "drowning"}, {"lava", "lava"}, {"magic", "magic"},
        {"projectile", "projectile"}, {"void", "void"},
    });

    server["DisplaySlotId"] = strEnum(lua, {
        {"belowName", "BelowName"}, {"list", "List"}, {"sidebar", "Sidebar"},
    });

    // Time / world constants.
    server["TicksPerSecond"] = 20;
    server["TicksPerDay"] = 24000;
}

}  // namespace bedrocklua::binding
