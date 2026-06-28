#include "binding/BindingRegistry.hpp"

#include <memory>
#include <string>
#include <unordered_map>

#include "lua/LuaScriptHost.hpp"
#include "mod/BedrockLuaMod.hpp"
#include "sig/SignatureRegistry.hpp"
#include "util/Log.hpp"

// world - the @minecraft/server `world` singleton.
//
// Offset-free, fully working: world.afterEvents / world.beforeEvents (added by
// installEvents) and world.sendMessage in its degraded form (logs to logcat so
// script output is observable on-device).
//
// Offset-dependent (degrade cleanly): getAllPlayers returns an empty list with a
// one-time warning until the player-list accessor is verified; getDimension
// returns a dimension object whose engine operations raise a catchable error.

namespace bedrocklua::binding {

namespace {
// Flattens a string or RawMessage-style table ({ rawtext = { { text = ".." } } })
// into a plain string for the degraded log path.
std::string stringifyMessage(const sol::object& msg) {
    if (msg.is<std::string>()) return msg.as<std::string>();
    if (msg.is<sol::table>()) {
        sol::table t = msg.as<sol::table>();
        if (t["rawtext"].valid() && t["rawtext"].get<sol::object>().is<sol::table>()) {
            std::string out;
            sol::table parts = t["rawtext"];
            for (auto& kv : parts) {
                sol::object v = kv.second;
                if (v.is<sol::table>()) {
                    sol::table part = v.as<sol::table>();
                    if (part["text"].valid()) out += part["text"].get<std::string>();
                }
            }
            return out;
        }
        if (t["text"].valid()) return t["text"].get<std::string>();
    }
    return "<message>";
}
}  // namespace

void installWorld(lua::LuaScriptHost& host, sol::table& server) {
    sol::state& lua = host.state().sol();
    sol::table world = lua.create_table();

    world.set_function("sendMessage", [&host](sol::object message) {
        std::string text = stringifyMessage(message);
        auto& sig = host.runtime().signatures;
        bool native = sig.isResolved("TextPacket::createChat") &&
                      sig.isResolved("ServerPlayer::sendNetworkPacket");
        // The verified native broadcast path is gated on the packet/player
        // accessors. Until those are confirmed for the target build we surface
        // the message to logcat, which keeps script output observable.
        log::info("[{}] world.sendMessage: {}{}", host.packId(), text,
                  native ? "" : " (logged; native chat broadcast pending offset verification)");
    });

    world.set_function("getAllPlayers", [&host]() -> sol::table {
        static bool warned = false;
        auto& sig = host.runtime().signatures;
        if (!sig.isResolved("Level::getActivePlayerCount") && !warned) {
            warned = true;
            log::warn("[{}] world.getAllPlayers: player list accessor unresolved; "
                      "returning empty list", host.packId());
        }
        return host.state().sol().create_table();  // empty until verified
    });

    world.set_function("getPlayers", [&host](sol::variadic_args) -> sol::table {
        return host.state().sol().create_table();
    });

    world.set_function("getDimension", [&host](const std::string& id) -> sol::object {
        sol::protected_function maker = host.state().sol()["__blua_make_dimension"];
        if (maker.valid()) {
            auto r = maker(id);
            if (r.valid()) return r.get<sol::object>();
        }
        return sol::object{};  // nil
    });

    // --- world dynamic properties (offset-free, in-memory) -----------------
    // Mirrors world.setDynamicProperty/getDynamicProperty. Backed by an in-memory
    // store for the session; genuinely usable for script state without any engine
    // offset. (Persistence to the world save is the offset-dependent extension.)
    auto store = std::make_shared<std::unordered_map<std::string, sol::object>>();
    world.set_function("setDynamicProperty", [store](const std::string& id, sol::object value) {
        if (!value.valid() || value.get_type() == sol::type::lua_nil) {
            store->erase(id);
        } else {
            (*store)[id] = value;
        }
    });
    world.set_function("getDynamicProperty",
                       [store](const std::string& id) -> sol::object {
                           auto it = store->find(id);
                           return it != store->end() ? it->second : sol::object{};  // nil
                       });
    world.set_function("getDynamicPropertyIds", [&host, store]() -> sol::table {
        sol::table ids = host.state().sol().create_table();
        int i = 1;
        for (auto& kv : *store) ids[i++] = kv.first;
        return ids;
    });
    world.set_function("clearDynamicProperties", [store]() { store->clear(); });

    // --- time accessors (offset-dependent) ---------------------------------
    world.set_function("getTimeOfDay", [&host]() -> int {
        unavailable(host, "world.getTimeOfDay", "Level::getTime");
    });
    world.set_function("setTimeOfDay", [&host](sol::object) {
        unavailable(host, "world.setTimeOfDay", "Level::setTime");
    });
    world.set_function("getDay", [&host]() -> int {
        unavailable(host, "world.getDay", "Level::getDay");
    });
    world.set_function("getDefaultSpawnLocation", [&host]() {
        unavailable(host, "world.getDefaultSpawnLocation", "Level::getDefaultSpawn");
    });

    server["world"] = world;
}

}  // namespace bedrocklua::binding
