#pragma once

// Http - minimal HTTP(S) GET for fetching external Lua modules.
//
// Android native code has no portable TLS stack, so this routes the request
// through the process's JVM (java.net.URL / HttpURLConnection) via JNI, reusing
// Android's networking + TLS. If no JavaVM is reachable (e.g. running outside a
// normal app process) it fails gracefully and the caller skips the import.

#include <string>

namespace bedrocklua::net {

struct HttpResult {
    bool ok = false;
    long status = 0;
    std::string body;
    std::string error;
};

// Performs an HTTP(S) GET. Follows redirects (Java default). Thread-safe:
// attaches to the JVM as needed.
HttpResult httpGet(const std::string& url);

}  // namespace bedrocklua::net
