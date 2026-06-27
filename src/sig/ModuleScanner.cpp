#include "sig/ModuleScanner.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

#include "util/Log.hpp"

namespace bedrocklua::scan {

namespace {
struct Pattern {
    std::vector<std::uint8_t> bytes;
    std::vector<bool> wildcard;
    bool valid = false;
};

// Parses "AA BB ?? CC" or "AABB??CC" into bytes + wildcard mask. A token
// containing '?' is a full-byte wildcard.
Pattern parsePattern(const std::string& in) {
    Pattern p;
    std::string compact;
    compact.reserve(in.size());
    for (char c : in) {
        if (!std::isspace(static_cast<unsigned char>(c))) compact.push_back(c);
    }
    if (compact.empty() || compact.size() % 2 != 0) return p;  // malformed

    for (size_t i = 0; i + 1 < compact.size(); i += 2) {
        char hi = compact[i];
        char lo = compact[i + 1];
        if (hi == '?' || lo == '?') {
            p.bytes.push_back(0);
            p.wildcard.push_back(true);
            continue;
        }
        auto nibble = [](char ch, bool& ok) -> int {
            if (ch >= '0' && ch <= '9') return ch - '0';
            if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
            if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
            ok = false;
            return 0;
        };
        bool ok = true;
        int b = (nibble(hi, ok) << 4) | nibble(lo, ok);
        if (!ok) return p;
        p.bytes.push_back(static_cast<std::uint8_t>(b));
        p.wildcard.push_back(false);
    }
    p.valid = !p.bytes.empty();
    return p;
}

struct Range {
    std::uintptr_t start;
    std::uintptr_t end;
};

// Collects executable (r-x) mappings whose pathname contains moduleName.
std::vector<Range> execRanges(const std::string& moduleName) {
    std::vector<Range> ranges;
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        // Format: start-end perms offset dev inode pathname
        if (line.find(moduleName) == std::string::npos) continue;
        std::uintptr_t start = 0, end = 0;
        char perms[5] = {0};
        if (std::sscanf(line.c_str(), "%zx-%zx %4s", &start, &end, perms) != 3) continue;
        if (perms[0] != 'r' || perms[2] != 'x') continue;  // need read + exec
        if (end > start) ranges.push_back({start, end});
    }
    return ranges;
}

std::uintptr_t scanRange(const Range& r, const Pattern& p) {
    const size_t n = p.bytes.size();
    if (n == 0 || r.end - r.start < n) return 0;
    const auto* base = reinterpret_cast<const std::uint8_t*>(r.start);
    const size_t limit = (r.end - r.start) - n;
    for (size_t off = 0; off <= limit; ++off) {
        bool match = true;
        for (size_t i = 0; i < n; ++i) {
            if (!p.wildcard[i] && base[off + i] != p.bytes[i]) {
                match = false;
                break;
            }
        }
        if (match) return r.start + off;
    }
    return 0;
}
}  // namespace

std::uintptr_t findPattern(const std::string& moduleName, const std::string& pattern) {
    Pattern p = parsePattern(pattern);
    if (!p.valid) {
        log::warn("[scan] malformed pattern '{}'", pattern);
        return 0;
    }
    for (const auto& r : execRanges(moduleName)) {
        if (std::uintptr_t hit = scanRange(r, p)) return hit;
    }
    return 0;
}

}  // namespace bedrocklua::scan
