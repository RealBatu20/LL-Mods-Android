#pragma once

// Minimal SHA-256 for verifying downloaded Lua modules and deriving cache keys.

#include <cstdint>
#include <string>

namespace bedrocklua::net {

// Returns the lowercase hex SHA-256 of the input bytes.
std::string sha256Hex(const void* data, std::size_t size);
inline std::string sha256Hex(const std::string& s) { return sha256Hex(s.data(), s.size()); }

}  // namespace bedrocklua::net
