#pragma once

// Small string helpers shared across the project.

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace bedrocklua::strings {

inline std::string toLower(std::string_view in) {
    std::string out(in);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

inline std::string trim(std::string_view in) {
    const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    auto begin = std::find_if(in.begin(), in.end(), notSpace);
    auto end = std::find_if(in.rbegin(), in.rend(), notSpace).base();
    if (begin >= end) return {};
    return std::string(begin, end);
}

inline bool equalsIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

inline bool endsWith(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Convert a Minecraft-style version triple/quad to a comparable string for
// matching against the manifest's "minecraft_versions" wildcards.
inline std::vector<std::string> split(std::string_view s, char delim) {
    std::vector<std::string> out;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == delim) {
            out.emplace_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    return out;
}

}  // namespace bedrocklua::strings
