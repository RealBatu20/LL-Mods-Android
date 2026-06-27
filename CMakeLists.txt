cmake_minimum_required(VERSION 3.18)
project(bedrocklua LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# bedrocklua - a Lua modding API for Minecraft Bedrock (Android / LeviLaunchroid)
#
# Re-implementation of the concept behind the archived Imrglop/bedrock.lua
# (which targeted Win10 Edition + MinHook) on the Android / ARM64 stack using
# LeviLaunchroid's preloader-android + Dobby. Produces libbedrocklua.so.
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# sol2 + the binding layer rely on exceptions/RTTI. Keep the size-oriented flags
# from the existing project but re-enable exceptions/RTTI for our translation
# units (the base preloader template strips them).
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fvisibility=hidden -ffunction-sections -fdata-sections -fexceptions -frtti -w")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -O2 -fvisibility=hidden -ffunction-sections -fdata-sections -w")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections,--strip-all -s")

include(FetchContent)

# --- core libs ------------------------------------------------------------
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8)

FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.2.1)

FetchContent_Declare(json
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

FetchContent_MakeAvailable(fmt glm json lua sol2)

# --- LeviLaunchroid preloader (provides pl::mod / pl::hook / pl::signature) -
FetchContent_Declare(preloader_android
    GIT_REPOSITORY https://github.com/liteLDev/preloader-android.git
    GIT_TAG main)
FetchContent_GetProperties(preloader_android)
if(NOT preloader_android_POPULATED)
    FetchContent_Populate(preloader_android)

    # The preloader template assumes libstdc++ <format>; the Android NDK we
    # build with does not ship it, so route std::format through fmt instead.
    file(READ "${preloader_android_SOURCE_DIR}/src/pl/Logger.h" LOGGER_CONTENT)
    string(REPLACE "#include <format>" "#include <fmt/format.h>" LOGGER_CONTENT "${LOGGER_CONTENT}")
    string(REPLACE "std::vformat" "fmt::vformat" LOGGER_CONTENT "${LOGGER_CONTENT}")
    string(REPLACE "std::make_format_args" "fmt::make_format_args" LOGGER_CONTENT "${LOGGER_CONTENT}")
    file(WRITE "${preloader_android_SOURCE_DIR}/src/pl/Logger.h" "${LOGGER_CONTENT}")

    add_subdirectory(${preloader_android_SOURCE_DIR} ${preloader_android_BINARY_DIR})
endif()

target_link_libraries(preloader PUBLIC fmt::fmt glm::glm)

# --- Dobby (inline ARM64 hooking backend) ---------------------------------
add_library(dobby STATIC IMPORTED)
set_target_properties(dobby PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/Dobby/${ANDROID_ABI}/libdobby.a)

# --- the mod --------------------------------------------------------------
file(GLOB_RECURSE BEDROCKLUA_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_library(bedrocklua SHARED ${BEDROCKLUA_SOURCES})

# Embed the version-keyed signature table next to the sources so it can be read
# at runtime from the mod directory; also keep a compiled copy as a fallback.
target_compile_definitions(bedrocklua PRIVATE
    SOL_ALL_SAFETIES_ON=1
    SOL_EXCEPTIONS_SAFE_PROPAGATION=1)

target_include_directories(bedrocklua PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/Dobby)

target_link_libraries(bedrocklua
    preloader
    dobby
    sol2::sol2
    lua_static
    fmt::fmt
    glm::glm
    nlohmann_json::nlohmann_json
    log)
