#include "lua/ModuleImporter.hpp"

#include <stdexcept>
#include <string>

#include "lua/LuaScriptHost.hpp"
#include "net/Downloader.hpp"
#include "util/Log.hpp"

namespace bedrocklua::lua::importer {

int apply(LuaScriptHost& host, const std::vector<LuaImport>& imports,
          const std::filesystem::path& packDir, const std::filesystem::path& cacheDir) {
    if (imports.empty()) return 0;

    sol::state& lua = host.state().sol();
    sol::table package = lua["package"];
    if (!package.valid()) return 0;
    sol::table preload = package["preload"];

    int registered = 0;
    for (const auto& imp : imports) {
        auto fetched = net::fetchModule(imp.source, packDir, cacheDir, imp.sha256);

        if (!fetched.ok) {
            if (imp.optional) {
                log::warn("[import] '{}' skipped (optional): {}", imp.name, fetched.error);
                continue;
            }
            log::error("[import] '{}' failed: {}", imp.name, fetched.error);
            // Register a loader that surfaces the failure as a catchable Lua
            // error when the script actually require()s it.
            std::string name = imp.name;
            std::string err = fetched.error;
            preload[name] = [name, err](sol::this_state) -> sol::object {
                throw std::runtime_error("module '" + name + "' import failed: " + err);
            };
            continue;
        }

        std::string name = imp.name;
        std::string src = std::move(fetched.data);
        std::string chunk = "@blua-import:" + name;
        // Lua require() calls this loader once and caches the result.
        preload[name] = [name, src, chunk](sol::this_state ts) -> sol::object {
            sol::state_view L(ts);
            auto res = L.safe_script(src, sol::script_pass_on_error, chunk);
            if (!res.valid()) {
                sol::error e = res;
                throw std::runtime_error("module '" + name + "' error: " + e.what());
            }
            return res.get<sol::object>();
        };
        ++registered;
        log::info("[import] registered '{}' from {}", name, fetched.origin);
    }
    return registered;
}

}  // namespace bedrocklua::lua::importer
