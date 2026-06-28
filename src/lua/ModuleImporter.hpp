#pragma once

// ModuleImporter - registers a pack's declared external Lua modules so scripts
// can require()/import() them by name. Each module's source is resolved by the
// Downloader (URL or local path) and installed into package.preload.

#include <filesystem>
#include <vector>

#include "pack/PackManifest.hpp"

namespace bedrocklua::lua {

class LuaScriptHost;

namespace importer {

// Resolves and registers every import. Failures on non-optional imports register
// a preload that raises a clear error on require(); optional ones are skipped.
// Returns the number of modules successfully registered.
int apply(LuaScriptHost& host, const std::vector<LuaImport>& imports,
          const std::filesystem::path& packDir, const std::filesystem::path& cacheDir);

}  // namespace importer
}  // namespace bedrocklua::lua
