#pragma once

// Downloader - resolves an import "source" to Lua source text.
//
// Sources may be:
//   * http(s):// URL      - fetched via JNI HTTP, cached, optional sha256 verify
//   * file:// URL          - read from disk
//   * absolute path        - read from disk
//   * pack-relative path   - read relative to the behavior pack directory
//
// Network results are cached under cacheDir so subsequent loads work offline; a
// failed network fetch falls back to a previously cached copy when available.

#include <filesystem>
#include <string>

namespace bedrocklua::net {

struct FetchResult {
    bool ok = false;
    std::string data;    // the Lua source on success
    std::string origin;  // human-readable origin (url/path/cache) for logging
    std::string error;
};

FetchResult fetchModule(const std::string& source,
                        const std::filesystem::path& packDir,
                        const std::filesystem::path& cacheDir,
                        const std::string& sha256 = "");

}  // namespace bedrocklua::net
