#pragma once

// HookManager - thin bookkeeping layer over pl::hook (GlossHook backend).
//
// Hook modules (LevelHooks, ChatHooks, PackStackHooks, ...) install their
// detours through here so that disable()/unload() can tear everything down in
// reverse order. A failed or unresolved target is logged and skipped, never
// fatal.

#include <cstdint>
#include <string>
#include <vector>

namespace bedrocklua {

struct Runtime;

class HookManager {
public:
    // Installs a single detour. `target` may be 0 (unresolved signature): the
    // call is then a no-op that logs and returns false. `original` receives the
    // trampoline to the next-in-chain implementation.
    bool install(const std::string& label, std::uintptr_t target, void* detour,
                 void** original);

    // Installs every game hook module. Safe to call once from enable().
    void installGameHooks(Runtime& rt);

    // Removes all installed detours (reverse order).
    void uninstallAll();

    size_t installedCount() const { return entries_.size(); }

private:
    struct Entry {
        std::string label;
        void* target;
        void* detour;
    };
    std::vector<Entry> entries_;
};

// Implemented in the individual hook translation units. Each returns true if it
// installed at least one detour.
namespace hooks {
bool installLevelHooks(Runtime& rt);
bool installChatHooks(Runtime& rt);
bool installPackStackHooks(Runtime& rt);
bool installScriptEngineSeam(Runtime& rt);  // phase-2 seam, inert by default
}  // namespace hooks

}  // namespace bedrocklua
