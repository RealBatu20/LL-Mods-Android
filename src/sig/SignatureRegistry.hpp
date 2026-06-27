#pragma once

// SignatureRegistry - version-keyed resolution of libminecraftpe.so addresses.
//
// Each logical engine symbol (e.g. "Level::tick") maps to a list of candidate
// resolvers: mangled symbol names and/or byte patterns gathered across the
// supported Minecraft versions. At resolve time we try every candidate against
// the actually-loaded module and cache the first hit. A symbol that resolves to
// nothing is *not* an error in the build - it is a documented runtime state:
// any binding that needs it raises a clean, catchable Lua error instead of
// crashing. This is how we ship a version-keyed scaffold without stubs.

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace bedrocklua {

class SignatureRegistry {
public:
    struct Candidate {
        std::string value;  // a mangled symbol name or a byte pattern
        bool isPattern;     // false => symbol name, true => byte pattern
        std::string version;  // informational: which MC version it was derived from
    };

    struct Entry {
        std::string name;
        std::vector<Candidate> candidates;
    };

    struct Status {
        std::string name;
        bool resolved = false;
        std::uintptr_t address = 0;
        std::string matchedFrom;  // which candidate matched
    };

    // Loads signatures.json from the mod directory (falls back to the embedded
    // default table if the file is missing or unparseable).
    bool load(const std::filesystem::path& modDir,
              std::string moduleName = "libminecraftpe.so");

    // Resolves a logical name to an address (0 if unresolved). Cached.
    std::uintptr_t resolve(const std::string& name);

    bool isResolved(const std::string& name) { return resolve(name) != 0; }

    template <typename Fn>
    Fn resolveAs(const std::string& name) {
        return reinterpret_cast<Fn>(resolve(name));
    }

    // A snapshot of every known logical symbol and whether it resolved. Used by
    // the diagnostics dump that runs at enable() and is mirrored in
    // docs/VERIFICATION.md.
    std::vector<Status> snapshot();

    const std::string& moduleName() const { return moduleName_; }

private:
    void loadEmbeddedDefaults();
    bool loadFromFile(const std::filesystem::path& file);

    std::string moduleName_ = "libminecraftpe.so";
    std::vector<Entry> entries_;
    std::unordered_map<std::string, std::uintptr_t> cache_;
    std::unordered_map<std::string, std::string> matchedFrom_;
};

}  // namespace bedrocklua
