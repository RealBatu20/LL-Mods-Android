#include "lua/LuaEventBus.hpp"

#include <algorithm>

#include "lua/LuaScriptHost.hpp"

namespace bedrocklua::lua {

void LuaEventBus::registerHost(LuaScriptHost* host) {
    if (std::find(hosts_.begin(), hosts_.end(), host) == hosts_.end()) {
        hosts_.push_back(host);
    }
}

void LuaEventBus::unregisterHost(LuaScriptHost* host) {
    hosts_.erase(std::remove(hosts_.begin(), hosts_.end(), host), hosts_.end());
}

void LuaEventBus::clear() { hosts_.clear(); }

void LuaEventBus::fireWorldLoad() {
    for (auto* host : hosts_) {
        host->dispatch(EventPhase::After, "worldLoad",
                       [](LuaScriptHost& h) -> sol::object {
                           return sol::make_object(h.state().sol(),
                                                   h.state().sol().create_table());
                       });
    }
}

void LuaEventBus::fireWorldInitialize() {
    for (auto* host : hosts_) {
        host->dispatch(EventPhase::After, "worldInitialize",
                       [](LuaScriptHost& h) -> sol::object {
                           return sol::make_object(h.state().sol(),
                                                   h.state().sol().create_table());
                       });
    }
}

void LuaEventBus::firePlayerJoin(const std::string& playerName, std::uint64_t entityId) {
    for (auto* host : hosts_) {
        host->dispatch(EventPhase::After, "playerJoin",
                       [&](LuaScriptHost& h) -> sol::object {
                           auto t = h.state().sol().create_table();
                           t["playerName"] = playerName;
                           t["playerId"] = std::to_string(entityId);
                           return sol::make_object(h.state().sol(), t);
                       });
    }
}

void LuaEventBus::firePlayerLeave(const std::string& playerName, std::uint64_t entityId) {
    for (auto* host : hosts_) {
        host->dispatch(EventPhase::After, "playerLeave",
                       [&](LuaScriptHost& h) -> sol::object {
                           auto t = h.state().sol().create_table();
                           t["playerName"] = playerName;
                           t["playerId"] = std::to_string(entityId);
                           return sol::make_object(h.state().sol(), t);
                       });
    }
}

bool LuaEventBus::fireChatSend(const std::string& senderName, std::uint64_t senderId,
                               const std::string& message) {
    bool cancelled = false;
    for (auto* host : hosts_) {
        sol::object payload = host->dispatch(
            EventPhase::Before, "chatSend", [&](LuaScriptHost& h) -> sol::object {
                auto t = h.state().sol().create_table();
                auto sender = h.state().sol().create_table();
                sender["name"] = senderName;
                sender["id"] = std::to_string(senderId);
                t["sender"] = sender;
                t["message"] = message;
                t["cancel"] = false;
                return sol::make_object(h.state().sol(), t);
            });
        if (payload.is<sol::table>()) {
            sol::table t = payload.as<sol::table>();
            if (t["cancel"].valid() && t["cancel"].get<bool>()) {
                cancelled = true;
            }
        }
    }
    return cancelled;
}

void LuaEventBus::fireChatReceive(const std::string& senderName, const std::string& message) {
    for (auto* host : hosts_) {
        host->dispatch(EventPhase::After, "chatSend",
                       [&](LuaScriptHost& h) -> sol::object {
                           auto t = h.state().sol().create_table();
                           auto sender = h.state().sol().create_table();
                           sender["name"] = senderName;
                           t["sender"] = sender;
                           t["message"] = message;
                           return sol::make_object(h.state().sol(), t);
                       });
    }
}

}  // namespace bedrocklua::lua
