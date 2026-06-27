#pragma once

// ModuleScanner - byte-pattern search across a loaded module's executable
// memory, using /proc/self/maps to locate the module's r-xp ranges.
//
// Used for signature candidates expressed as byte patterns ("AA BB ?? CC" or
// "AABB??CC"); GlossSymbol handles the symbol-name candidates. Patterns are the
// reliable path for stripped Bedrock internals that export no dynamic symbol.

#include <cstdint>
#include <string>

namespace bedrocklua::scan {

// Returns the absolute address of the first occurrence of `pattern` in any
// executable mapping of `moduleName` (substring match on the maps pathname), or
// 0 if not found / module not mapped. `?` / `??` match any byte.
std::uintptr_t findPattern(const std::string& moduleName, const std::string& pattern);

}  // namespace bedrocklua::scan
