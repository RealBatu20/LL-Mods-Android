cmake_minimum_required(VERSION 3.18)
project(AdvancedStatsMod LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -fvisibility=hidden -ffunction-sections -fdata-sections -w")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -fvisibility=hidden -ffunction-sections -fdata-sections -w")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections,--strip-all -s")

include(FetchContent)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.2.1
)
FetchContent_MakeAvailable(fmt glm)

FetchContent_Declare(
    preloader_android
    GIT_REPOSITORY https://github.com/liteLDev/preloader-android.git
    GIT_TAG main
)
FetchContent_GetProperties(preloader_android)
if(NOT preloader_android_POPULATED)
    FetchContent_Populate(preloader_android)
    
    file(READ "${preloader_android_SOURCE_DIR}/src/pl/Logger.h" LOGGER_CONTENT)
    string(REPLACE "#include <format>" "#include <fmt/format.h>" LOGGER_CONTENT "${LOGGER_CONTENT}")
    string(REPLACE "std::vformat" "fmt::vformat" LOGGER_CONTENT "${LOGGER_CONTENT}")
    string(REPLACE "std::make_format_args" "fmt::make_format_args" LOGGER_CONTENT "${LOGGER_CONTENT}")
    file(WRITE "${preloader_android_SOURCE_DIR}/src/pl/Logger.h" "${LOGGER_CONTENT}")
    
    file(READ "${CMAKE_SOURCE_DIR}/src/util/Logger.hpp" LOGGER_CONTENT)
    string(REPLACE "#include <format>" "#include <fmt/format.h>" LOGGER_CONTENT "${LOGGER_CONTENT}")
    string(REPLACE "std::vformat" "fmt::vformat" LOGGER_CONTENT "${LOGGER_CONTENT}")
    string(REPLACE "std::make_format_args" "fmt::make_format_args" LOGGER_CONTENT "${LOGGER_CONTENT}")
    file(WRITE "${CMAKE_SOURCE_DIR}/src/util/Logger.hpp" "${LOGGER_CONTENT}")
    
    add_subdirectory(${preloader_android_SOURCE_DIR} ${preloader_android_BINARY_DIR})
endif()

target_link_libraries(preloader PUBLIC fmt::fmt glm::glm)

add_library(dobby STATIC IMPORTED)
set_target_properties(dobby PROPERTIES
    IMPORTED_LOCATION
    ${CMAKE_SOURCE_DIR}/Dobby/${ANDROID_ABI}/libdobby.a
)

add_library(AdvancedStatsMod SHARED
    src/main.cpp
    src/world/item/ItemStackBase.cpp
    src/world/item/Item.cpp
    src/world/item/registry/ItemRegistryRef.cpp
    src/world/actor/player/PlayerInventory.cpp
)

target_include_directories(AdvancedStatsMod PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/Dobby
)

target_link_libraries(AdvancedStatsMod
    preloader
    dobby
    log
)
