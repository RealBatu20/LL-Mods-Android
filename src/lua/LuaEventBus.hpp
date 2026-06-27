#pragma once

// LuaEventBus - fans native engine events out to every loaded Lua host.
//
// Hooks (chat, world load, player join, ...) call the fireXxx methods on the
// game main thread; the bus rebuilds the payload per host (each host has its
// own lua_State) and dispatches through that host's protected-call handler.

#include <cstdint>
#include <string>
#include <vector>

namespace bedrocklua::lua {

class LuaScriptHost;

class LuaEventBus {
public:
    void registerHost(LuaScriptHost* host);
    void unregisterHost(LuaScriptHost* host);
    void clear();

    bool hasHosts() const { return !hosts_.empty(); }

    // --- world afterEvents -------------------------------------------------
    void fireWorldLoad();
    void fireWorldInitialize();
    void firePlayerJoin(const std::string& playerName, std::uint64_t entityId);
    void firePlayerLeave(const std::string& playerName, std::uint64_t entityId);

    // Chat. Returns true if any beforeEvents.chatSend listener cancelled it.
    bool fireChatSend(const std::string& senderName, std::uint64_t senderId,
                      const std::string& message);

    void fireChatReceive(const std::string& senderName, const std::string& message);

private:
    std::vector<LuaScriptHost*> hosts_;
};

}  // namespace bedrocklua::lua
