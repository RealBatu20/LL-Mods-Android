#include "pack/PackScanner.hpp"

#include <system_error>

#include "lua/LuaScriptHost.hpp"
#include "mod/BedrockLuaMod.hpp"
#include "pack/PackManifest.hpp"
#include "util/Log.hpp"

namespace bedrocklua {

namespace fs = std::filesystem;

PackScanner::PackScanner() { seedDefaultRoots(); }
PackScanner::~PackScanner() = default;

void PackScanner::seedDefaultRoots() {
    // Standard Minecraft Bedrock behavior-pack locations on Android. We probe a
    // few base paths because launchers and storage layouts vary; non-existent
    // roots are silently skipped at scan time.
    static const char* kBases[] = {
        "/storage/emulated/0/Android/data/com.mojang.minecraftpe/files/games/com.mojang",
        "/sdcard/Android/data/com.mojang.minecraftpe/files/games/com.mojang",
    };
    static const char* kSub[] = {"development_behavior_packs", "behavior_packs"};
    for (const char* base : kBases) {
        for (const char* sub : kSub) {
            searchRoots_.emplace_back(fs::path(base) / sub);
        }
    }
}

void PackScanner::addSearchRoot(fs::path root) {
    for (const auto& existing : searchRoots_) {
        if (existing == root) return;
    }
    searchRoots_.push_back(std::move(root));
}

void PackScanner::start(Runtime& rt) { scanRoots(rt); }

void PackScanner::onWorldLoad(Runtime& rt) {
    // A world just became active. Rescan so packs enabled only for this world
    // (and any roots contributed by PackStackHooks) get picked up.
    scanRoots(rt);
}

void PackScanner::scanRoots(Runtime& rt) {
    for (const auto& root : searchRoots_) {
        std::error_code ec;
        if (fs::exists(root, ec) && fs::is_directory(root, ec)) {
            scanRoot(rt, root);
        }
    }
}

void PackScanner::scanRoot(Runtime& rt, const fs::path& root) {
    std::error_code ec;
    for (fs::directory_iterator it(root, ec), end; it != end; it.increment(ec)) {
        if (ec) break;
        if (!it->is_directory(ec)) continue;
        fs::path packDir = it->path();
        fs::path manifestPath = packDir / "manifest.json";
        if (!fs::exists(manifestPath, ec)) continue;

        auto parsed = PackManifest::parseFile(manifestPath);
        if (!parsed) {
            log::warn("skipping {}: {}", packDir.string(), parsed.error());
            continue;
        }
        const PackManifest& manifest = parsed.value();
        if (!manifest.hasLua()) continue;

        loadPack(rt, manifest, packDir);
    }
}

void PackScanner::loadPack(Runtime& rt, const PackManifest& manifest, const fs::path& packDir) {
    std::string packId = !manifest.uuid.empty() ? manifest.uuid : packDir.filename().string();
    if (loadedPackIds_.count(packId)) return;  // already loaded this session

    // Bedrock allows a single script module per pack; load the first lua entry.
    const ScriptModule& module = manifest.luaModules.front();

    log::info("loading lua pack '{}' ({}) entry={}", manifest.name, packId, module.entry);
    auto host = std::make_unique<lua::LuaScriptHost>(rt, packId, packDir);
    bool ok = host->start(module.entry);
    if (!ok) {
        log::warn("lua pack '{}' entry reported an error (host kept alive for events)", packId);
    }
    loadedPackIds_.insert(packId);
    hosts_.push_back(std::move(host));
}

void PackScanner::tick(Runtime&, std::uint64_t currentTick) {
    tickCounter_ = currentTick;
    for (auto& host : hosts_) {
        host->tick(currentTick);
    }
}

void PackScanner::stopAll(Runtime&) {
    // Destroying hosts unregisters them from the event bus and tears down their
    // Lua states.
    hosts_.clear();
    loadedPackIds_.clear();
}

}  // namespace bedrocklua
