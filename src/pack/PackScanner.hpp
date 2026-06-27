#pragma once

// PackScanner - discovers behavior packs that declare a Lua script module and
// owns the LuaScriptHost created for each one.
//
// Discovery sources:
//   1. Standard Android behavior-pack locations under the game's external data
//      dir (development_behavior_packs + behavior_packs).
//   2. The live pack stack captured by PackStackHooks once a world is loaded
//      (phase 2), which narrows loading to the packs actually enabled for the
//      current world.

#include <cstdint>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace bedrocklua {

struct Runtime;
struct PackManifest;

namespace lua {
class LuaScriptHost;
}

class PackScanner {
public:
    PackScanner();
    ~PackScanner();

    // Initial discovery + load (called from enable()).
    void start(Runtime& rt);

    // Re-scan when a world becomes ready / the pack stack is known.
    void onWorldLoad(Runtime& rt);

    // Drives every host's scheduler once per game tick.
    void tick(Runtime& rt, std::uint64_t currentTick);

    // Registers an additional search root (used by PackStackHooks to add the
    // resolved active-pack directories).
    void addSearchRoot(std::filesystem::path root);

    void stopAll(Runtime& rt);

    size_t hostCount() const { return hosts_.size(); }

private:
    void scanRoots(Runtime& rt);
    void scanRoot(Runtime& rt, const std::filesystem::path& root);
    void loadPack(Runtime& rt, const PackManifest& manifest,
                  const std::filesystem::path& packDir);
    void seedDefaultRoots();

    std::vector<std::filesystem::path> searchRoots_;
    std::set<std::string> loadedPackIds_;
    std::vector<std::unique_ptr<lua::LuaScriptHost>> hosts_;
    std::uint64_t tickCounter_ = 0;
};

}  // namespace bedrocklua
