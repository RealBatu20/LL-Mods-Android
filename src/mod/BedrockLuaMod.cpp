#include "mod/BedrockLuaMod.hpp"

#include <pl/cpp/Mod.hpp>

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <thread>

#include "hook/HookManager.hpp"
#include "lua/LuaEventBus.hpp"
#include "pack/PackScanner.hpp"
#include "sig/SignatureRegistry.hpp"
#include "util/Log.hpp"

namespace bedrocklua {

struct BedrockLuaMod::Impl {
    SignatureRegistry signatures;
    HookManager hooks;
    lua::LuaEventBus events;
    PackScanner packs;

    std::optional<Runtime> runtime;
    std::uint64_t tickCount = 0;

    // Fallback ticker: drives the Lua scheduler at ~20Hz when no engine hook
    // could be installed (e.g. signatures unresolved on this build), so the Lua
    // runtime and example packs are still exercisable. Only started when zero
    // game hooks are active, so it never races the game thread.
    std::thread fallbackTicker;
    std::atomic<bool> fallbackRunning{false};
    std::mutex tickMutex;
};

BedrockLuaMod::BedrockLuaMod() : impl_(std::make_unique<Impl>()) {}
BedrockLuaMod::~BedrockLuaMod() = default;

BedrockLuaMod& BedrockLuaMod::getInstance() {
    static BedrockLuaMod instance;
    return instance;
}

Runtime& BedrockLuaMod::runtime() { return *impl_->runtime; }
SignatureRegistry& BedrockLuaMod::signatures() { return impl_->signatures; }
HookManager& BedrockLuaMod::hooks() { return impl_->hooks; }
lua::LuaEventBus& BedrockLuaMod::events() { return impl_->events; }
PackScanner& BedrockLuaMod::packs() { return impl_->packs; }

namespace {
// Reads the mod/data directories from the preloader context, tolerating either
// std::filesystem::path or string-like return types.
std::filesystem::path queryModDir() {
    try {
        return std::filesystem::path(pl::mod::NativeMod::current()->getModDir());
    } catch (...) {
        return std::filesystem::current_path();
    }
}
std::filesystem::path queryDataDir() {
    try {
        return std::filesystem::path(pl::mod::NativeMod::current()->getDataDir());
    } catch (...) {
        return std::filesystem::current_path();
    }
}
}  // namespace

bool BedrockLuaMod::load() {
    log::info("bedrocklua loading (v0.1.0)");

    auto modDir = queryModDir();
    auto dataDir = queryDataDir();

    // Build the runtime view that subsystems consume. References are stable for
    // the lifetime of the mod singleton.
    impl_->runtime.emplace(Runtime{
        impl_->signatures,
        impl_->hooks,
        impl_->events,
        impl_->packs,
        modDir,
        dataDir,
    });

    log::info("mod dir: {}", modDir.string());
    return true;
}

bool BedrockLuaMod::enable() {
    if (!impl_->runtime) {
        log::error("enable() called before load(); aborting");
        return false;
    }
    Runtime& rt = *impl_->runtime;

    // 1. Resolve engine signatures against the loaded libminecraftpe.so.
    impl_->signatures.load(rt.modDir);

    // 2. Report which targets resolved (diagnostics mirrored in VERIFICATION.md).
    int resolved = 0;
    int total = 0;
    for (const auto& s : impl_->signatures.snapshot()) {
        ++total;
        if (s.resolved) {
            ++resolved;
            log::info("[sig] {} -> {:#x} ({})", s.name, s.address, s.matchedFrom);
        } else {
            log::warn("[sig] {} UNRESOLVED on this build", s.name);
        }
    }
    log::info("signatures resolved: {}/{}", resolved, total);

    // 3. Install engine hooks (each one no-ops gracefully if its target is 0).
    impl_->hooks.installGameHooks(rt);
    log::info("hooks installed: {}", impl_->hooks.installedCount());

    // 4. Discover and load Lua behavior packs available at startup.
    impl_->packs.start(rt);
    log::info("lua packs loaded at startup: {}", impl_->packs.hostCount());

    enabled_ = true;

    // 5. If we could not install any engine hook, spin up the fallback ticker so
    // the scheduler/events still advance. Safe because there is no game thread
    // calling into us in that case.
    if (impl_->hooks.installedCount() == 0) {
        log::warn("no engine hooks active; starting fallback ticker at ~20Hz");
        impl_->fallbackRunning = true;
        impl_->fallbackTicker = std::thread([this]() {
            bool worldFired = false;
            while (impl_->fallbackRunning.load(std::memory_order_relaxed)) {
                if (!worldFired) {
                    worldFired = true;
                    onWorldLoad();
                }
                onGameTick();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
    }

    log::info("bedrocklua enabled");
    return true;
}

bool BedrockLuaMod::disable() {
    if (!enabled_) return true;
    Runtime& rt = *impl_->runtime;

    // Stop the fallback ticker before tearing down hosts so it cannot tick a
    // destroyed host.
    if (impl_->fallbackRunning.exchange(false)) {
        if (impl_->fallbackTicker.joinable()) impl_->fallbackTicker.join();
    }

    impl_->packs.stopAll(rt);
    impl_->hooks.uninstallAll();
    impl_->events.clear();

    enabled_ = false;
    log::info("bedrocklua disabled");
    return true;
}

bool BedrockLuaMod::unload() {
    log::info("bedrocklua unloaded");
    return true;
}

void BedrockLuaMod::onGameTick() {
    if (!enabled_ || !impl_->runtime) return;
    std::lock_guard<std::mutex> lock(impl_->tickMutex);
    ++impl_->tickCount;
    impl_->packs.tick(*impl_->runtime, impl_->tickCount);
}

void BedrockLuaMod::onWorldLoad() {
    if (!enabled_ || !impl_->runtime) return;
    std::lock_guard<std::mutex> lock(impl_->tickMutex);
    log::info("world load detected; rescanning behavior packs");
    impl_->packs.onWorldLoad(*impl_->runtime);
    impl_->events.fireWorldInitialize();
    impl_->events.fireWorldLoad();
}

}  // namespace bedrocklua
