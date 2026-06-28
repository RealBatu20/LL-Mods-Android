#pragma once

// PackManifest - parses a Minecraft behavior pack manifest.json and surfaces any
// script module that targets Lua.
//
// The shape mirrors the vanilla Script API exactly; the only difference is the
// module's "language" field:
//
//   {
//     "format_version": 2,
//     "header": { "name": "...", "uuid": "...", "version": [1,0,0],
//                 "min_engine_version": [1,21,0] },
//     "modules": [
//       { "type": "script", "language": "lua", "uuid": "...",
//         "version": [1,0,0], "entry": "scripts/main.lua" }
//     ],
//     "dependencies": [ { "module_name": "@bedrocklua", "version": "0.1.0" } ]
//   }
//
// bedrocklua extension: a pack may import external Lua API modules (from a URL,
// or a local/relative path) that become require()/import()-able by name. This is
// the escape hatch for when the built-in @bedrocklua surface is
// insufficient - bring your own Lua API from anywhere:
//
//   "bedrocklua": {
//     "imports": [
//       { "name": "json",  "source": "https://example.com/json.lua",
//         "sha256": "<hex>", "optional": false },
//       { "name": "mylib", "source": "scripts/lib/mylib.lua" }
//     ]
//   }

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "util/Result.hpp"

namespace bedrocklua {

struct ScriptModule {
    std::string uuid;
    std::string entry;     // e.g. "scripts/main.lua"
    std::string language;  // "lua" for us
    std::array<int, 3> version{0, 0, 0};
};

// An external Lua module a pack wants importable by name.
struct LuaImport {
    std::string name;     // require()/import() key
    std::string source;   // http(s):// URL, file:// URL, or pack-relative/abs path
    std::string sha256;   // optional lowercase hex; verified after fetch when set
    bool optional = false;  // when true, a failed import logs and is skipped
};

struct PackManifest {
    std::string name;
    std::string uuid;            // header uuid
    std::array<int, 3> version{0, 0, 0};
    std::array<int, 3> minEngineVersion{0, 0, 0};
    std::vector<ScriptModule> luaModules;        // script modules with language=lua
    std::vector<std::string> apiDependencies;    // e.g. "@minecraft/server"
    std::vector<LuaImport> imports;              // bedrocklua.imports

    bool hasLua() const { return !luaModules.empty(); }

    // Parses the manifest file. Returns failure on IO/JSON errors. A manifest
    // with no lua module parses successfully but reports hasLua()==false.
    static Result<PackManifest> parseFile(const std::filesystem::path& manifestPath);

    // Parses from an in-memory JSON string (used by tests).
    static Result<PackManifest> parseString(const std::string& json);
};

}  // namespace bedrocklua
