#include "binding/BindingRegistry.hpp"

#include "lua/LuaScriptHost.hpp"

// @minecraft/server-ui - ActionFormData / ModalFormData / MessageFormData.
//
// The builder surface (title/body/button/...) is offset-free and fully works:
// it accumulates a form definition table that scripts can inspect and pass
// around. show(player) is the part that needs the engine, so it raises a
// catchable error until the form-request signatures are verified.

namespace bedrocklua::binding {

namespace {
sol::table makeActionForm(lua::LuaScriptHost& host) {
    sol::state& lua = host.state().sol();
    sol::table form = lua.create_table();
    form["__type"] = "ActionFormData";
    form["buttons"] = lua.create_table();
    form.set_function("title", [form](const std::string& t) mutable -> sol::table {
        form["titleText"] = t;
        return form;
    });
    form.set_function("body", [form](const std::string& t) mutable -> sol::table {
        form["bodyText"] = t;
        return form;
    });
    form.set_function("button", [form](const std::string& text, sol::optional<std::string> icon)
                          mutable -> sol::table {
        sol::state_view lua(form.lua_state());
        sol::table buttons = form["buttons"];
        sol::table b = lua.create_table();
        b["text"] = text;
        if (icon) b["icon"] = *icon;
        buttons[static_cast<int>(buttons.size()) + 1] = b;
        return form;
    });
    form.set_function("show", [&host](sol::table, sol::object) {
        unavailable(host, "ActionFormData.show", "ServerPlayer::sendNetworkPacket(ModalFormRequest)");
    });
    return form;
}

sol::table makeMessageForm(lua::LuaScriptHost& host) {
    sol::state& lua = host.state().sol();
    sol::table form = lua.create_table();
    form["__type"] = "MessageFormData";
    form.set_function("title", [form](const std::string& t) mutable -> sol::table {
        form["titleText"] = t;
        return form;
    });
    form.set_function("body", [form](const std::string& t) mutable -> sol::table {
        form["bodyText"] = t;
        return form;
    });
    form.set_function("button1", [form](const std::string& t) mutable -> sol::table {
        form["button1Text"] = t;
        return form;
    });
    form.set_function("button2", [form](const std::string& t) mutable -> sol::table {
        form["button2Text"] = t;
        return form;
    });
    form.set_function("show", [&host](sol::table, sol::object) {
        unavailable(host, "MessageFormData.show", "ServerPlayer::sendNetworkPacket(ModalFormRequest)");
    });
    return form;
}

sol::table makeModalForm(lua::LuaScriptHost& host) {
    sol::state& lua = host.state().sol();
    sol::table form = lua.create_table();
    form["__type"] = "ModalFormData";
    form["controls"] = lua.create_table();
    auto pushControl = [form](sol::table control) {
        sol::table controls = form["controls"];
        controls[static_cast<int>(controls.size()) + 1] = control;
    };
    form.set_function("title", [form](const std::string& t) mutable -> sol::table {
        form["titleText"] = t;
        return form;
    });
    form.set_function("textField", [form, pushControl](const std::string& label,
                                                       const std::string& placeholder,
                                                       sol::optional<std::string> def)
                          mutable -> sol::table {
        sol::state_view lua(form.lua_state());
        sol::table c = lua.create_table();
        c["type"] = "textField";
        c["label"] = label;
        c["placeholder"] = placeholder;
        if (def) c["default"] = *def;
        pushControl(c);
        return form;
    });
    form.set_function("toggle", [form, pushControl](const std::string& label,
                                                    sol::optional<bool> def) mutable -> sol::table {
        sol::state_view lua(form.lua_state());
        sol::table c = lua.create_table();
        c["type"] = "toggle";
        c["label"] = label;
        c["default"] = def.value_or(false);
        pushControl(c);
        return form;
    });
    form.set_function("slider", [form, pushControl](const std::string& label, double min, double max,
                                                    sol::optional<double> step,
                                                    sol::optional<double> def) mutable -> sol::table {
        sol::state_view lua(form.lua_state());
        sol::table c = lua.create_table();
        c["type"] = "slider";
        c["label"] = label;
        c["min"] = min;
        c["max"] = max;
        c["step"] = step.value_or(1.0);
        c["default"] = def.value_or(min);
        pushControl(c);
        return form;
    });
    form.set_function("dropdown", [form, pushControl](const std::string& label, sol::table options,
                                                      sol::optional<int> def) mutable -> sol::table {
        sol::state_view lua(form.lua_state());
        sol::table c = lua.create_table();
        c["type"] = "dropdown";
        c["label"] = label;
        c["options"] = options;
        c["default"] = def.value_or(0);
        pushControl(c);
        return form;
    });
    form.set_function("show", [&host](sol::table, sol::object) {
        unavailable(host, "ModalFormData.show", "ServerPlayer::sendNetworkPacket(ModalFormRequest)");
    });
    return form;
}
}  // namespace

void installServerUi(lua::LuaScriptHost& host, sol::table& serverUi) {
    sol::state& lua = host.state().sol();

    auto makeCtor = [&lua](auto factory) {
        sol::table ctor = lua.create_table();
        sol::table mt = lua.create_table();
        mt.set_function(sol::meta_function::call,
                        [factory](sol::table, sol::this_state ts) { return factory(ts); });
        ctor[sol::metatable_key] = mt;
        return ctor;
    };

    serverUi["ActionFormData"] = makeCtor([&host](sol::this_state) { return makeActionForm(host); });
    serverUi["MessageFormData"] =
        makeCtor([&host](sol::this_state) { return makeMessageForm(host); });
    serverUi["ModalFormData"] = makeCtor([&host](sol::this_state) { return makeModalForm(host); });
}

}  // namespace bedrocklua::binding
