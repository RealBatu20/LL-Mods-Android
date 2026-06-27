#pragma once

// Lightweight logging facade for bedrocklua.
//
// Logs go to Android logcat under the "bedrocklua" tag via __android_log_print,
// which is always available regardless of whether the LeviLaunchroid mod context
// has finished initialising. fmt is used for formatting so the call sites read
// like the rest of the preloader code base.

#include <android/log.h>
#include <fmt/format.h>

#include <string>
#include <string_view>
#include <utility>

namespace bedrocklua::log {

inline constexpr const char* kTag = "bedrocklua";

namespace detail {
inline void write(int priority, const std::string& message) {
    __android_log_write(priority, kTag, message.c_str());
}

template <typename... Args>
inline std::string format(fmt::format_string<Args...> fmt, Args&&... args) {
    try {
        return fmt::format(fmt, std::forward<Args>(args)...);
    } catch (const std::exception& e) {
        return std::string("<format error: ") + e.what() + ">";
    }
}
}  // namespace detail

template <typename... Args>
inline void info(fmt::format_string<Args...> fmt, Args&&... args) {
    detail::write(ANDROID_LOG_INFO, detail::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
inline void warn(fmt::format_string<Args...> fmt, Args&&... args) {
    detail::write(ANDROID_LOG_WARN, detail::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
inline void error(fmt::format_string<Args...> fmt, Args&&... args) {
    detail::write(ANDROID_LOG_ERROR, detail::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
inline void debug(fmt::format_string<Args...> fmt, Args&&... args) {
    detail::write(ANDROID_LOG_DEBUG, detail::format(fmt, std::forward<Args>(args)...));
}

// Non-formatting overloads for plain strings.
inline void info(std::string_view s) { detail::write(ANDROID_LOG_INFO, std::string(s)); }
inline void warn(std::string_view s) { detail::write(ANDROID_LOG_WARN, std::string(s)); }
inline void error(std::string_view s) { detail::write(ANDROID_LOG_ERROR, std::string(s)); }
inline void debug(std::string_view s) { detail::write(ANDROID_LOG_DEBUG, std::string(s)); }

}  // namespace bedrocklua::log
