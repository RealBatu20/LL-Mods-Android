#pragma once

// BindingRegistry - assembles the @bedrocklua module family for a host
// (bedrocklua's own namespace; the vanilla @minecraft/* names are intentionally
// not used).
//
// Modules are exposed through Lua's package system so scripts can write either
//
//     local mc = require("@bedrocklua")
//
// or use the convenience global:
//
//     local mc = import("@bedrocklua")
//
// Each area (system, world, player, entity, block, item, events, ui) registers
// its slice of the module table. Bindings that need engine offsets resolve them
// lazily through the SignatureRegistry and raise a clean Lua error when the
// target is unavailable on the running build.

#include <sol/sol.hpp>

namespace bedrocklua::lua {
class LuaScriptHost;
}

namespace bedrocklua::binding {

// Installs every module into the host and wires up require()/import().
void installAll(lua::LuaScriptHost& host);

// Module installers - each fills in part of the @minecraft/server table.
// (Declared here so BindingRegistry.cpp can call across translation units.)
void installSystem(lua::LuaScriptHost& host, sol::table& server);
void installConstants(lua::LuaScriptHost& host, sol::table& server);
void installVector3(lua::LuaScriptHost& host, sol::table& server);
void installWorld(lua::LuaScriptHost& host, sol::table& server);
void installDimension(lua::LuaScriptHost& host, sol::table& server);
void installPlayer(lua::LuaScriptHost& host, sol::table& server);
void installEntity(lua::LuaScriptHost& host, sol::table& server);
void installBlock(lua::LuaScriptHost& host, sol::table& server);
void installItemStack(lua::LuaScriptHost& host, sol::table& server);
void installEvents(lua::LuaScriptHost& host, sol::table& server);

// @bedrocklua-ui form module.
void installServerUi(lua::LuaScriptHost& host, sol::table& serverUi);

// @bedrocklua-admin (server administration) + @bedrocklua-net (networking).
void installAdmin(lua::LuaScriptHost& host, sol::table& admin);
void installNet(lua::LuaScriptHost& host, sol::table& net);

// Helper used by offset-dependent bindings: raises a catchable Lua error that
// names the missing signature, used when an engine accessor is unavailable on
// the running version.
[[noreturn]] void unavailable(lua::LuaScriptHost& host, const char* api,
                              const char* signatureName);

}  // namespace bedrocklua::binding
