#include "binding/BindingRegistry.hpp"

#include "lua/LuaScriptHost.hpp"

// system - the scheduler surface (@minecraft/server `system`). Fully functional
// and offset-free: it is backed entirely by the host's tick-driven scheduler.

namespace bedrocklua::binding {

void installSystem(lua::LuaScriptHost& host, sol::table& server) {
    sol::state& lua = host.state().sol();
    sol::table system = lua.create_table();

    system.set_function("run", [&host](sol::protected_function fn) {
        return host.scheduleRun(std::move(fn));
    });
    system.set_function("runTimeout", [&host](sol::protected_function fn, std::uint32_t ticks) {
        return host.scheduleTimeout(std::move(fn), ticks);
    });
    system.set_function("runInterval", [&host](sol::protected_function fn, std::uint32_t ticks) {
        return host.scheduleInterval(std::move(fn), ticks);
    });
    system.set_function("clearRun", [&host](int handle) { host.clearRun(handle); });

    // currentTick is a property in the JS API; expose it both as a function and
    // as a live read via the metatable __index below.
    system.set_function("getCurrentTick", [&host]() { return host.currentTick(); });

    sol::table mt = lua.create_table();
    mt.set_function(sol::meta_function::index,
                    [&host](sol::table /*self*/, const std::string& key) -> sol::object {
                        if (key == "currentTick") {
                            return sol::make_object(host.state().sol(), host.currentTick());
                        }
                        return sol::object{};  // nil
                    });
    system[sol::metatable_key] = mt;

    // system.afterEvents (e.g. scriptEventReceive) shares the generic event
    // machinery; populated by installEvents once world exists.
    server["system"] = system;
}

}  // namespace bedrocklua::binding
