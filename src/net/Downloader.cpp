#include "net/Downloader.hpp"

#include <fstream>
#include <system_error>

#include "net/Http.hpp"
#include "net/Sha256.hpp"
#include "util/Log.hpp"
#include "util/Strings.hpp"

namespace bedrocklua::net {

namespace {
bool isUrl(const std::string& s) {
    return strings::toLower(s).rfind("http://", 0) == 0 ||
           strings::toLower(s).rfind("https://", 0) == 0;
}

std::string readFile(const std::filesystem::path& p, std::string& err) {
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        err = "cannot open " + p.string();
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void writeFile(const std::filesystem::path& p, const std::string& data) {
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (out) out.write(data.data(), static_cast<std::streamsize>(data.size()));
}

bool shaMatches(const std::string& data, const std::string& expected) {
    if (expected.empty()) return true;  // nothing to verify against
    return sha256Hex(data) == strings::toLower(expected);
}
}  // namespace

FetchResult fetchModule(const std::string& source, const std::filesystem::path& packDir,
                        const std::filesystem::path& cacheDir, const std::string& sha256) {
    FetchResult r;

    // --- local file sources -------------------------------------------------
    if (!isUrl(source)) {
        std::filesystem::path path;
        if (strings::toLower(source).rfind("file://", 0) == 0) {
            path = source.substr(7);
        } else {
            std::filesystem::path p(source);
            path = p.is_absolute() ? p : (packDir / p);
        }
        std::string err;
        std::string data = readFile(path, err);
        if (!err.empty()) {
            r.error = err;
            return r;
        }
        if (!shaMatches(data, sha256)) {
            r.error = "sha256 mismatch for " + path.string();
            return r;
        }
        r.ok = true;
        r.data = std::move(data);
        r.origin = path.string();
        return r;
    }

    // --- URL sources (cache, then network, then stale cache fallback) -------
    std::filesystem::path cacheFile = cacheDir / (sha256Hex(source) + ".lua");

    std::error_code ec;
    if (std::filesystem::exists(cacheFile, ec)) {
        std::string err;
        std::string cached = readFile(cacheFile, err);
        if (err.empty() && shaMatches(cached, sha256)) {
            r.ok = true;
            r.data = std::move(cached);
            r.origin = "cache:" + source;
            return r;
        }
        // Cache present but unverifiable/mismatched -> fall through to refetch.
    }

    HttpResult http = httpGet(source);
    if (http.ok) {
        if (!shaMatches(http.body, sha256)) {
            r.error = "sha256 mismatch for " + source;
            return r;
        }
        writeFile(cacheFile, http.body);
        r.ok = true;
        r.data = std::move(http.body);
        r.origin = source;
        return r;
    }

    // Network failed: use any cached copy (even unverified) as a last resort.
    if (std::filesystem::exists(cacheFile, ec)) {
        std::string err;
        std::string cached = readFile(cacheFile, err);
        if (err.empty() && shaMatches(cached, sha256)) {
            log::warn("[import] network failed ({}); using cached copy of {}", http.error, source);
            r.ok = true;
            r.data = std::move(cached);
            r.origin = "cache(stale):" + source;
            return r;
        }
    }

    r.error = "fetch failed: " + http.error;
    return r;
}

}  // namespace bedrocklua::net
