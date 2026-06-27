#include "binding/BindingRegistry.hpp"

#include <cmath>

#include "lua/LuaScriptHost.hpp"

// Vector helpers (@minecraft/server `Vector`/`Vector3`). Offset-free: pure value
// math over {x=, y=, z=} tables, matching how the JS API passes locations.

namespace bedrocklua::binding {

namespace {
struct V3 {
    double x = 0, y = 0, z = 0;
};

V3 fromTable(const sol::table& t) {
    V3 v;
    if (t["x"].valid()) v.x = t["x"].get<double>();
    if (t["y"].valid()) v.y = t["y"].get<double>();
    if (t["z"].valid()) v.z = t["z"].get<double>();
    return v;
}

sol::table toTable(sol::state_view lua, const V3& v) {
    sol::table t = lua.create_table();
    t["x"] = v.x;
    t["y"] = v.y;
    t["z"] = v.z;
    return t;
}
}  // namespace

void installVector3(lua::LuaScriptHost& host, sol::table& server) {
    sol::state& lua = host.state().sol();
    sol::table vec = lua.create_table();

    vec.set_function("create", [&lua](double x, double y, double z) {
        return toTable(lua, V3{x, y, z});
    });
    vec.set_function("add", [&lua](sol::table a, sol::table b) {
        V3 va = fromTable(a), vb = fromTable(b);
        return toTable(lua, V3{va.x + vb.x, va.y + vb.y, va.z + vb.z});
    });
    vec.set_function("subtract", [&lua](sol::table a, sol::table b) {
        V3 va = fromTable(a), vb = fromTable(b);
        return toTable(lua, V3{va.x - vb.x, va.y - vb.y, va.z - vb.z});
    });
    vec.set_function("scale", [&lua](sol::table a, double s) {
        V3 va = fromTable(a);
        return toTable(lua, V3{va.x * s, va.y * s, va.z * s});
    });
    vec.set_function("dot", [](sol::table a, sol::table b) {
        V3 va = fromTable(a), vb = fromTable(b);
        return va.x * vb.x + va.y * vb.y + va.z * vb.z;
    });
    vec.set_function("length", [](sol::table a) {
        V3 v = fromTable(a);
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    });
    vec.set_function("distance", [](sol::table a, sol::table b) {
        V3 va = fromTable(a), vb = fromTable(b);
        double dx = va.x - vb.x, dy = va.y - vb.y, dz = va.z - vb.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    });
    vec.set_function("normalize", [&lua](sol::table a) {
        V3 v = fromTable(a);
        double len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len == 0) return toTable(lua, V3{0, 0, 0});
        return toTable(lua, V3{v.x / len, v.y / len, v.z / len});
    });

    // Common direction constants (mirrors @minecraft/server `Vector` statics).
    vec["up"] = toTable(lua, V3{0, 1, 0});
    vec["down"] = toTable(lua, V3{0, -1, 0});
    vec["forward"] = toTable(lua, V3{0, 0, 1});
    vec["back"] = toTable(lua, V3{0, 0, -1});
    vec["left"] = toTable(lua, V3{-1, 0, 0});
    vec["right"] = toTable(lua, V3{1, 0, 0});
    vec["zero"] = toTable(lua, V3{0, 0, 0});
    vec["one"] = toTable(lua, V3{1, 1, 1});

    server["Vector"] = vec;
    server["Vector3"] = vec;
}

}  // namespace bedrocklua::binding
