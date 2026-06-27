#include "net/Http.hpp"

#include <dlfcn.h>
#include <jni.h>

#include "util/Log.hpp"

namespace bedrocklua::net {

namespace {

using GetVmsFn = jint (*)(JavaVM**, jsize, jsize*);

// Finds the process JavaVM by resolving JNI_GetCreatedJavaVMs from whichever
// runtime library exports it on this Android version.
JavaVM* findJavaVm() {
    static const char* kLibs[] = {"libnativehelper.so", "libart.so",
                                  "libandroid_runtime.so"};
    for (const char* lib : kLibs) {
        void* h = dlopen(lib, RTLD_NOW | RTLD_NOLOAD);
        if (h == nullptr) h = dlopen(lib, RTLD_NOW);
        if (h == nullptr) continue;
        auto fn = reinterpret_cast<GetVmsFn>(dlsym(h, "JNI_GetCreatedJavaVMs"));
        if (fn == nullptr) continue;
        JavaVM* vm = nullptr;
        jsize count = 0;
        if (fn(&vm, 1, &count) == JNI_OK && count > 0 && vm != nullptr) {
            return vm;
        }
    }
    return nullptr;
}

struct ScopedEnv {
    JavaVM* vm = nullptr;
    JNIEnv* env = nullptr;
    bool attached = false;

    explicit ScopedEnv(JavaVM* v) : vm(v) {
        if (vm == nullptr) return;
        jint r = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
        if (r == JNI_EDETACHED) {
            if (vm->AttachCurrentThread(&env, nullptr) == JNI_OK) attached = true;
            else env = nullptr;
        } else if (r != JNI_OK) {
            env = nullptr;
        }
    }
    ~ScopedEnv() {
        if (attached && vm != nullptr) vm->DetachCurrentThread();
    }
};

// Clears any pending JNI exception and returns true if one was present.
bool clearException(JNIEnv* env) {
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return true;
    }
    return false;
}

}  // namespace

HttpResult httpGet(const std::string& url) {
    HttpResult result;

    JavaVM* vm = findJavaVm();
    if (vm == nullptr) {
        result.error = "no JavaVM available for HTTP(S) (cannot reach the network)";
        return result;
    }
    ScopedEnv scoped(vm);
    JNIEnv* env = scoped.env;
    if (env == nullptr) {
        result.error = "could not attach to JavaVM";
        return result;
    }

    auto fail = [&](const std::string& msg) -> HttpResult {
        clearException(env);
        result.ok = false;
        result.error = msg;
        return result;
    };

    jclass urlClass = env->FindClass("java/net/URL");
    if (urlClass == nullptr) return fail("java/net/URL not found");
    jmethodID urlCtor = env->GetMethodID(urlClass, "<init>", "(Ljava/lang/String;)V");
    jmethodID openConn =
        env->GetMethodID(urlClass, "openConnection", "()Ljava/net/URLConnection;");
    if (urlCtor == nullptr || openConn == nullptr) return fail("URL methods not found");

    jstring jurl = env->NewStringUTF(url.c_str());
    jobject urlObj = env->NewObject(urlClass, urlCtor, jurl);
    if (urlObj == nullptr || clearException(env)) return fail("bad URL: " + url);

    jobject conn = env->CallObjectMethod(urlObj, openConn);
    if (conn == nullptr || clearException(env)) return fail("openConnection failed");

    jclass httpConn = env->FindClass("java/net/HttpURLConnection");
    jclass urlConn = env->FindClass("java/net/URLConnection");
    jmethodID setConnectTimeout = env->GetMethodID(urlConn, "setConnectTimeout", "(I)V");
    jmethodID setReadTimeout = env->GetMethodID(urlConn, "setReadTimeout", "(I)V");
    jmethodID setReqProp =
        env->GetMethodID(urlConn, "setRequestProperty", "(Ljava/lang/String;Ljava/lang/String;)V");
    jmethodID getInput =
        env->GetMethodID(urlConn, "getInputStream", "()Ljava/io/InputStream;");
    jmethodID getRespCode = env->GetMethodID(httpConn, "getResponseCode", "()I");

    if (setConnectTimeout) env->CallVoidMethod(conn, setConnectTimeout, 15000);
    if (setReadTimeout) env->CallVoidMethod(conn, setReadTimeout, 15000);
    if (setReqProp) {
        jstring k = env->NewStringUTF("User-Agent");
        jstring v = env->NewStringUTF("bedrocklua/1.0");
        env->CallVoidMethod(conn, setReqProp, k, v);
        env->DeleteLocalRef(k);
        env->DeleteLocalRef(v);
    }

    if (getRespCode != nullptr) {
        jint code = env->CallIntMethod(conn, getRespCode);
        if (clearException(env)) return fail("getResponseCode failed");
        result.status = code;
        if (code < 200 || code >= 300) {
            return fail("HTTP status " + std::to_string(code));
        }
    }

    jobject stream = env->CallObjectMethod(conn, getInput);
    if (stream == nullptr || clearException(env)) return fail("getInputStream failed");

    jclass inputStream = env->FindClass("java/io/InputStream");
    jmethodID readM = env->GetMethodID(inputStream, "read", "([B)I");
    jmethodID closeM = env->GetMethodID(inputStream, "close", "()V");
    jbyteArray buffer = env->NewByteArray(8192);
    if (readM == nullptr || buffer == nullptr) return fail("InputStream read unavailable");

    std::string body;
    while (true) {
        jint n = env->CallIntMethod(stream, readM, buffer);
        if (clearException(env)) {
            if (closeM) env->CallVoidMethod(stream, closeM);
            return fail("stream read error");
        }
        if (n < 0) break;
        if (n > 0) {
            jbyte* elems = env->GetByteArrayElements(buffer, nullptr);
            body.append(reinterpret_cast<const char*>(elems), static_cast<size_t>(n));
            env->ReleaseByteArrayElements(buffer, elems, JNI_ABORT);
        }
        if (body.size() > 16u * 1024 * 1024) {  // 16 MiB sanity cap
            if (closeM) env->CallVoidMethod(stream, closeM);
            return fail("response too large");
        }
    }
    if (closeM) env->CallVoidMethod(stream, closeM);
    clearException(env);

    result.ok = true;
    result.body = std::move(body);
    return result;
}

}  // namespace bedrocklua::net
