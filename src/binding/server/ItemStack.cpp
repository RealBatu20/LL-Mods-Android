#include "binding/BindingRegistry.hpp"

#include "lua/LuaScriptHost.hpp"

// ItemStack - @minecraft/server `ItemStack`. Unlike the world accessors, an
// ItemStack is a value type in the Script API (you `new ItemStack(...)`), so we
// implement it fully and offset-free as a Lua data object. Applying a stack to a
// container is the part that needs the engine (see Player.getInventory).

namespace bedrocklua::binding {

namespace {
sol::table makeItemStack(sol::state_view lua, const std::string& typeId, int amount) {
    sol::table item = lua.create_table();
    item["typeId"] = typeId;
    item["amount"] = amount <= 0 ? 1 : amount;
    item["nameTag"] = sol::lua_nil;
    item["lore"] = lua.create_table();

    item.set_function("setLore", [item](sol::table lines) mutable {
        item["lore"] = lines;
    });
    item.set_function("getLore", [item]() -> sol::table {
        return item["lore"];
    });
    item.set_function("setAmount", [item](int n) mutable {
        item["amount"] = n <= 0 ? 1 : n;
    });
    item.set_function("getAmount", [item]() -> int { return item["amount"]; });
    item.set_function("clone", [lua, item]() -> sol::table {
        sol::table copy = makeItemStack(lua, item["typeId"], item["amount"]);
        copy["nameTag"] = item["nameTag"];
        // shallow-copy lore
        sol::table lore = item["lore"];
        sol::table newLore = lua.create_table();
        int i = 1;
        for (auto& kv : lore) {
            newLore[i++] = kv.second;
        }
        copy["lore"] = newLore;
        return copy;
    });
    return item;
}
}  // namespace

void installItemStack(lua::LuaScriptHost& host, sol::table& server) {
    sol::state& lua = host.state().sol();
    sol::table ctor = lua.create_table();

    ctor.set_function("new", [&lua](const std::string& typeId, sol::optional<int> amount) {
        return makeItemStack(lua, typeId, amount.value_or(1));
    });

    // Allow `ItemStack(typeId, amount)` call syntax too.
    sol::table mt = lua.create_table();
    mt.set_function(sol::meta_function::call,
                    [&lua](sol::table, const std::string& typeId, sol::optional<int> amount) {
                        return makeItemStack(lua, typeId, amount.value_or(1));
                    });
    ctor[sol::metatable_key] = mt;

    server["ItemStack"] = ctor;
}

}  // namespace bedrocklua::binding
