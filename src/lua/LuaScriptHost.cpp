#include "lua/LuaScriptHost.hpp"

#include <algorithm>

#include "binding/BindingRegistry.hpp"
#include "lua/LuaEventBus.hpp"
#include "mod/BedrockLuaMod.hpp"
#include "util/Log.hpp"

namespace bedrocklua::lua {

LuaScriptHost::LuaScriptHost(Runtime& rt, std::string packId, std::filesystem::path packDir)
    : rt_(rt), packId_(std::move(packId)), packDir_(std::move(packDir)) {}

LuaScriptHost::~LuaScriptHost() {
    rt_.events.unregisterHost(this);
}

bool LuaScriptHost::start(const std::filesystem::path& entryRelative) {
    // Let `require` find sibling lua files inside the pack.
    auto scriptsRoot = (packDir_ / entryRelative).parent_path();
    std::string searchPath = (scriptsRoot / "?.lua").string() + ";" +
                             (scriptsRoot / "?" / "init.lua").string();
    sol::table package = state_.sol()["package"];
    if (package.valid()) {
        package["path"] = searchPath;
    }

    // Build the @minecraft/server API surface for this host.
    binding::installAll(*this);

    // Register with the event bus before running so the script can subscribe and
    // immediately start receiving events.
    rt_.events.registerHost(this);

    auto entry = packDir_ / entryRelative;
    log::info("[{}] running entry {}", packId_, entry.string());
    auto result = state_.runFile(entry);
    if (!result) {
        log::error("[{}] entry script failed: {}", packId_, result.error());
        // Keep the host registered: subscriptions made before the error are
        // still valid, and the scheduler can keep running survivors.
    }
    started_ = true;
    return result.ok();
}

void LuaScriptHost::tick(std::uint64_t currentTick) {
    currentTick_ = currentTick;
    if (tasks_.empty()) return;

    // Snapshot the count: callbacks may schedule new tasks (appended at the end
    // with a future run tick) which must not run this tick.
    const size_t count = tasks_.size();
    for (size_t i = 0; i < count; ++i) {
        // Re-fetch by index each iteration; the vector may have grown (pointers
        // would be invalidated, indices stay valid for existing slots).
        if (tasks_[i].cancelled) continue;
        if (tasks_[i].nextRunTick > currentTick) continue;

        sol::protected_function fn = tasks_[i].fn;  // copy before user code runs
        bool oneShot = tasks_[i].periodTicks == 0;
        if (oneShot) {
            tasks_[i].cancelled = true;
        } else {
            tasks_[i].nextRunTick = currentTick + tasks_[i].periodTicks;
        }

        auto r = state_.safeCall(fn);
        if (!r.valid()) {
            sol::error err = r;
            log::error("[{}] scheduled task error: {}", packId_, err.what());
            // A throwing interval would spam every period; cancel it.
            if (!oneShot) {
                if (i < tasks_.size()) tasks_[i].cancelled = true;
            }
        }
    }

    tasks_.erase(std::remove_if(tasks_.begin(), tasks_.end(),
                                [](const ScheduledTask& t) { return t.cancelled; }),
                 tasks_.end());
}

int LuaScriptHost::scheduleRun(sol::protected_function fn) {
    int handle = nextHandle_++;
    tasks_.push_back(ScheduledTask{handle, std::move(fn), currentTick_ + 1, 0});
    return handle;
}

int LuaScriptHost::scheduleTimeout(sol::protected_function fn, std::uint32_t delayTicks) {
    int handle = nextHandle_++;
    std::uint32_t delay = delayTicks == 0 ? 1 : delayTicks;
    tasks_.push_back(ScheduledTask{handle, std::move(fn), currentTick_ + delay, 0});
    return handle;
}

int LuaScriptHost::scheduleInterval(sol::protected_function fn, std::uint32_t periodTicks) {
    int handle = nextHandle_++;
    std::uint32_t period = periodTicks == 0 ? 1 : periodTicks;
    tasks_.push_back(ScheduledTask{handle, std::move(fn), currentTick_ + period, period});
    return handle;
}

void LuaScriptHost::clearRun(int handle) {
    for (auto& t : tasks_) {
        if (t.handle == handle) t.cancelled = true;
    }
}

LuaScriptHost::ListenerMap& LuaScriptHost::listenersFor(EventPhase phase) {
    return phase == EventPhase::After ? afterListeners_ : beforeListeners_;
}

void LuaScriptHost::subscribe(EventPhase phase, const std::string& eventName,
                              sol::protected_function fn) {
    listenersFor(phase)[eventName].push_back(std::move(fn));
}

void LuaScriptHost::unsubscribe(EventPhase phase, const std::string& eventName,
                                sol::protected_function fn) {
    auto& vec = listenersFor(phase)[eventName];
    vec.erase(std::remove_if(vec.begin(), vec.end(),
                             [&](const sol::protected_function& f) { return f == fn; }),
              vec.end());
}

bool LuaScriptHost::hasListeners(EventPhase phase, const std::string& eventName) {
    auto& map = listenersFor(phase);
    auto it = map.find(eventName);
    return it != map.end() && !it->second.empty();
}

sol::object LuaScriptHost::dispatch(EventPhase phase, const std::string& eventName,
                                    const PayloadFactory& factory) {
    auto& map = listenersFor(phase);
    auto it = map.find(eventName);
    if (it == map.end() || it->second.empty()) {
        return sol::object{};  // nil
    }

    sol::object payload = factory(*this);

    // Copy the listener list so a handler that subscribes/unsubscribes during
    // dispatch does not invalidate our iteration.
    std::vector<sol::protected_function> listeners = it->second;
    for (auto& fn : listeners) {
        sol::protected_function call = fn;
        sol::protected_function handler = state_.sol()["__blua_traceback"];
        if (handler.valid()) call.error_handler = handler;
        auto r = call(payload);
        if (!r.valid()) {
            sol::error err = r;
            log::error("[{}] event '{}' listener error: {}", packId_, eventName, err.what());
        }
    }
    return payload;
}

}  // namespace bedrocklua::lua
