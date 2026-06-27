#include "binding/BindingRegistry.hpp"

#include "lua/LuaScriptHost.hpp"

// Events - world.afterEvents / world.beforeEvents / system.afterEvents.
//
// Fully functional and offset-free. Each event name resolves lazily to a signal
// object exposing subscribe(fn)/unsubscribe(fn), backed by the host's listener
// tables and driven by LuaEventBus from the engine/fallback hooks. Using a
// metatable means *any* event name works (worldLoad, playerJoin, chatSend, ...)
// without enumerating them, matching the open-ended JS API.

namespace bedrocklua::binding {

namespace {
sol::table makeSignal(lua::LuaScriptHost& host, lua::EventPhase phase, std::string name) {
    sol::state& lua = host.state().sol();
    sol::table signal = lua.create_table();
    signal.set_function("subscribe",
                        [&host, phase, name](sol::protected_function fn) -> sol::protected_function {
                            host.subscribe(phase, name, fn);
                            return fn;  // handle, mirrors the JS API
                        });
    signal.set_function("unsubscribe",
                        [&host, phase, name](sol::protected_function fn) {
                            host.unsubscribe(phase, name, fn);
                        });
    return signal;
}

sol::table makeEventHolder(lua::LuaScriptHost& host, lua::EventPhase phase) {
    sol::state& lua = host.state().sol();
    sol::table holder = lua.create_table();
    sol::table cache = lua.create_table();

    sol::table mt = lua.create_table();
    mt.set_function(sol::meta_function::index,
                    [&host, phase, cache](sol::table, const std::string& key) mutable -> sol::object {
                        sol::object existing = cache[key];
                        if (existing.is<sol::table>()) return existing;
                        sol::table signal = makeSignal(host, phase, key);
                        cache[key] = signal;
                        return signal;
                    });
    holder[sol::metatable_key] = mt;
    return holder;
}
}  // namespace

void installEvents(lua::LuaScriptHost& host, sol::table& server) {
    sol::table world = server["world"];
    if (!world.valid()) {
        // World should already be installed; guard defensively.
        world = host.state().sol().create_table();
        server["world"] = world;
    }
    world["afterEvents"] = makeEventHolder(host, lua::EventPhase::After);
    world["beforeEvents"] = makeEventHolder(host, lua::EventPhase::Before);

    sol::table system = server["system"];
    if (system.valid()) {
        system["afterEvents"] = makeEventHolder(host, lua::EventPhase::After);
        system["beforeEvents"] = makeEventHolder(host, lua::EventPhase::Before);
    }
}

}  // namespace bedrocklua::binding
