cmake_minimum_required(VERSION 3.18)
project(bedrocklua LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# bedrocklua - a Lua modding API for Minecraft Bedrock (Android / arm64-v8a)
#
# Re-implementation of the concept behind the archived Imrglop/bedrock.lua
# (Win10 Edition + MinHook) on the Android stack. Self-contained: it links the
# GlossHook inline-hooking library statically and resolves engine symbols at
# runtime, so it needs no separately built libpreloader.so. Produces
# libbedrocklua.so.
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# sol2 + the binding layer rely on exceptions/RTTI.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fvisibility=hidden -ffunction-sections -fdata-sections -fexceptions -frtti -w")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -O2 -fvisibility=hidden -ffunction-sections -fdata-sections -w")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections,--strip-all -s")

if(NOT DEFINED ANDROID_ABI)
    set(ANDROID_ABI "arm64-v8a")
endif()
if(NOT "${ANDROID_ABI}" STREQUAL "arm64-v8a")
    message(WARNING "bedrocklua targets arm64-v8a; GlossHook is vendored for arm64 only (ABI='${ANDROID_ABI}').")
endif()

include(FetchContent)

# --- pure source/header libs (build cleanly under the NDK) -----------------
FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.2.1)

FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3)

# Lua 5.4 with a CMake build (upstream Lua ships no CMake of its own).
FetchContent_Declare(lua
    GIT_REPOSITORY https://github.com/walterschell/Lua.git
    GIT_TAG v5.4.5)
set(LUA_SUPPORT_DL OFF CACHE BOOL "" FORCE)
set(LUA_BUILD_AS_CXX OFF CACHE BOOL "" FORCE)
set(LUA_ENABLE_SHARED OFF CACHE BOOL "" FORCE)
set(LUA_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(LUA_BUILD_COMPILER OFF CACHE BOOL "" FORCE)

# sol2 - C++ <-> Lua binding layer (header only).
FetchContent_Declare(sol2
    GIT_REPOSITORY https://github.com/ThePhd/sol2.git
    GIT_TAG v3.3.0)

FetchContent_MakeAvailable(fmt nlohmann_json lua sol2)

# --- GlossHook (vendored prebuilt ARM64 static lib + header) ---------------
# Run scripts/fetch_deps.sh to populate third_party/glosshook. CI does this
# before configuring.
set(GLOSSHOOK_DIR "${CMAKE_SOURCE_DIR}/third_party/glosshook"
    CACHE PATH "Location of the vendored GlossHook header + prebuilt ARM64 static lib")
set(_gloss_lib "${GLOSSHOOK_DIR}/lib/arm64/libGlossHook.a")
set(_gloss_inc "${GLOSSHOOK_DIR}/include")
if(NOT EXISTS "${_gloss_lib}")
    message(FATAL_ERROR
        "GlossHook static lib not found at ${_gloss_lib}.\n"
        "Run scripts/fetch_deps.sh first (downloads XMDS/GlossHook), or pass "
        "-DGLOSSHOOK_DIR=...")
endif()

add_library(GlossHook STATIC IMPORTED)
set_target_properties(GlossHook PROPERTIES
    IMPORTED_LOCATION             "${_gloss_lib}"
    INTERFACE_INCLUDE_DIRECTORIES "${_gloss_inc}")

# --- the mod --------------------------------------------------------------
file(GLOB_RECURSE BEDROCKLUA_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_library(bedrocklua SHARED ${BEDROCKLUA_SOURCES})

target_compile_definitions(bedrocklua PRIVATE
    SOL_ALL_SAFETIES_ON=1
    SOL_EXCEPTIONS_SAFE_PROPAGATION=1)

target_include_directories(bedrocklua PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${_gloss_inc})

# GlossHook (inline hooking) + Android log + dl (dlopen/dladdr).
target_link_libraries(bedrocklua
    GlossHook
    sol2::sol2
    lua_static
    fmt::fmt
    nlohmann_json::nlohmann_json
    log
    dl)
