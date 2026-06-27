#pragma once

// Helpers for reading/owning libc++ (__ndk1) std::string instances out of the
// game's memory. Minecraft for Android is built with the NDK's libc++, whose
// std::string uses the standard short-string-optimised layout:
//
//   long form (is_long bit set in the capacity word's LSB):
//     [0..7]  capacity (LSB = 1)
//     [8..15] size
//     [16..23] char* data
//
//   short form (is_long bit clear):
//     [0]     size, stored as (size << 1)   (LSB = 0)
//     [1..]   inline character data
//
// These are only used by bindings/hooks once the relevant field offset has been
// resolved; callers are responsible for passing a valid pointer.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace bedrocklua::abi {

// Reads a libc++ std::string located at `p` into an owned std::string.
inline std::string readString(const void* p) {
    if (!p) return {};
    const auto* base = reinterpret_cast<const std::uint8_t*>(p);
    bool isLong = (base[0] & 0x1u) != 0;
    if (isLong) {
        std::size_t size = 0;
        const char* data = nullptr;
        std::memcpy(&size, base + sizeof(std::size_t), sizeof(std::size_t));
        std::memcpy(&data, base + 2 * sizeof(std::size_t), sizeof(const char*));
        if (!data) return {};
        return std::string(data, size);
    }
    std::size_t size = static_cast<std::size_t>(base[0] >> 1);
    return std::string(reinterpret_cast<const char*>(base + 1), size);
}

}  // namespace bedrocklua::abi
