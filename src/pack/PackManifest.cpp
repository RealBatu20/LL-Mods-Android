#include "pack/PackManifest.hpp"

#include <nlohmann/json.hpp>

#include <fstream>

#include "util/Strings.hpp"

namespace bedrocklua {

namespace {
using json = nlohmann::json;

std::array<int, 3> readVersion(const json& v) {
    std::array<int, 3> out{0, 0, 0};
    if (v.is_array()) {
        for (size_t i = 0; i < 3 && i < v.size(); ++i) {
            if (v[i].is_number_integer()) out[i] = v[i].get<int>();
        }
    } else if (v.is_string()) {
        // "1.2.3" form sometimes used by dependencies.
        auto parts = strings::split(v.get<std::string>(), '.');
        for (size_t i = 0; i < 3 && i < parts.size(); ++i) {
            try {
                out[i] = std::stoi(parts[i]);
            } catch (...) {
            }
        }
    }
    return out;
}

Result<PackManifest> parseJson(const json& root) {
    PackManifest mf;

    if (root.contains("header") && root["header"].is_object()) {
        const auto& header = root["header"];
        if (header.contains("name") && header["name"].is_string())
            mf.name = header["name"].get<std::string>();
        if (header.contains("uuid") && header["uuid"].is_string())
            mf.uuid = header["uuid"].get<std::string>();
        if (header.contains("version")) mf.version = readVersion(header["version"]);
        if (header.contains("min_engine_version"))
            mf.minEngineVersion = readVersion(header["min_engine_version"]);
    }

    if (root.contains("modules") && root["modules"].is_array()) {
        for (const auto& m : root["modules"]) {
            if (!m.is_object()) continue;
            std::string type = m.value("type", "");
            std::string language = m.value("language", "");
            if (strings::equalsIgnoreCase(type, "script") &&
                strings::equalsIgnoreCase(language, "lua")) {
                ScriptModule sm;
                sm.uuid = m.value("uuid", "");
                sm.entry = m.value("entry", "");
                sm.language = strings::toLower(language);
                if (m.contains("version")) sm.version = readVersion(m["version"]);
                if (sm.entry.empty()) continue;  // a lua module with no entry is unusable
                mf.luaModules.push_back(std::move(sm));
            }
        }
    }

    if (root.contains("dependencies") && root["dependencies"].is_array()) {
        for (const auto& d : root["dependencies"]) {
            if (d.is_object() && d.contains("module_name") && d["module_name"].is_string()) {
                mf.apiDependencies.push_back(d["module_name"].get<std::string>());
            }
        }
    }

    return Result<PackManifest>(std::move(mf));
}
}  // namespace

Result<PackManifest> PackManifest::parseString(const std::string& text) {
    json root = json::parse(text, nullptr, /*allow_exceptions=*/false,
                            /*ignore_comments=*/true);
    if (root.is_discarded()) {
        return Result<PackManifest>::fail("manifest.json is not valid JSON");
    }
    return parseJson(root);
}

Result<PackManifest> PackManifest::parseFile(const std::filesystem::path& manifestPath) {
    std::ifstream in(manifestPath, std::ios::binary);
    if (!in) {
        return Result<PackManifest>::fail("cannot open " + manifestPath.string());
    }
    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    return parseString(content);
}

}  // namespace bedrocklua
