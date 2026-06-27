// bedrocklua - LeviLaunchroid native mod entry point.
//
// PL_REGISTER_MOD generates the exported PLMod_Load/Enable/Disable/Unload
// symbols the preloader looks up, forwarding to our singleton's lifecycle
// methods with exception handling.

#include <pl/cpp/Mod.hpp>
#include <pl/cpp/mod/RegisterHelper.hpp>

#include "mod/BedrockLuaMod.hpp"

PL_REGISTER_MOD(bedrocklua::BedrockLuaMod, bedrocklua::BedrockLuaMod::getInstance());
