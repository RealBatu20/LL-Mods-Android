#include "binding/BindingRegistry.hpp"

#include "lua/LuaScriptHost.hpp"

// Block - @minecraft/server `Block`. Location/type data is offset-free; world
// mutation accessors raise a catchable error until BlockSource signatures are
// verified for the target build.

namespace bedrocklua::binding {

void installBlock(lua::LuaScriptHost& host, sol::table& server) {
    sol::state& lua = host.state().sol();
    sol::table block = lua.create_table();

    block.set_function("getType", [&host](sol::table) {
        unavailable(host, "Block.type", "Block::getName");
    });
    block.set_function("setType", [&host](sol::table, const std::string&) {
        unavailable(host, "Block.setType", "BlockSource::setBlock");
    });
    block.set_function("setPermutation", [&host](sol::table, sol::table) {
        unavailable(host, "Block.setPermutation", "BlockSource::setBlock");
    });
    block.set_function("getComponent", [&host](sol::table, const std::string&) {
        unavailable(host, "Block.getComponent", "BlockActor::getComponent");
    });
    block.set_function("isAir", [&host](sol::table) {
        unavailable(host, "Block.isAir", "Block::isAir");
    });

    // BlockPermutation factory placeholder data type (data-only).
    sol::table permutation = lua.create_table();
    permutation.set_function("resolve", [&lua](const std::string& typeId, sol::object states) {
        sol::table p = lua.create_table();
        p["typeId"] = typeId;
        p["states"] = states.valid() ? states : sol::object(lua.create_table());
        return p;
    });
    server["BlockPermutation"] = permutation;
    server["Block"] = block;
}

}  // namespace bedrocklua::binding
