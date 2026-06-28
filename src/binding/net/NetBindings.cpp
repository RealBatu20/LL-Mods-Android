#include "binding/BindingRegistry.hpp"

#include <thread>
#include <tuple>

#include "lua/LuaScriptHost.hpp"
#include "net/Http.hpp"
#include "net/Sha256.hpp"
#include "util/Log.hpp"

// @bedrocklua-net - networking for Lua scripts. sha256 is offset-free; HTTP(S)
// goes through the process JVM (see net/Http.cpp). fetch/get are synchronous
// (they block the caller), fetchAsync runs off-thread and delivers the result on
// the game thread via the host mailbox.

namespace bedrocklua::binding {

namespace {
sol::table toResult(sol::state_view lua, const bedrocklua::net::HttpResult& r) {
    sol::table t = lua.create_table();
    t["ok"] = r.ok;
    t["status"] = static_cast<int>(r.status);
    t["body"] = r.body;
    t["error"] = r.error;
    return t;
}
}  // namespace

void installNet(lua::LuaScriptHost& host, sol::table& net) {
    net.set_function("sha256", [](const std::string& s) {
        return bedrocklua::net::sha256Hex(s);
    });

    // fetch(url) -> { ok, status, body, error }. Synchronous: blocks the caller.
    net.set_function("fetch", [&host](const std::string& url) -> sol::table {
        auto r = bedrocklua::net::httpGet(url);
        return toResult(host.state().sol(), r);
    });

    // get(url) -> body | (nil, error). Synchronous convenience wrapper.
    net.set_function("get",
                     [&host](const std::string& url) -> std::tuple<sol::object, sol::object> {
                         auto r = bedrocklua::net::httpGet(url);
                         sol::state_view L = host.state().sol();
                         if (r.ok) {
                             return std::make_tuple(sol::make_object(L, r.body), sol::object{});
                         }
                         return std::make_tuple(sol::object{}, sol::make_object(L, r.error));
                     });

    // fetchAsync(url, callback): runs the request on a worker thread, then calls
    // callback({ ok, status, body, error }) on the game thread. Non-blocking.
    net.set_function("fetchAsync", [&host](const std::string& url, sol::protected_function cb) {
        auto mailbox = host.mailbox();
        std::thread([&host, mailbox, url, cb]() mutable {
            auto r = bedrocklua::net::httpGet(url);
            mailbox->post([&host, cb, r]() mutable {
                sol::table t = toResult(host.state().sol(), r);
                auto res = cb(t);
                if (!res.valid()) {
                    sol::error e = res;
                    log::error("net.fetchAsync callback error: {}", e.what());
                }
            });
        }).detach();
    });
}

}  // namespace bedrocklua::binding
