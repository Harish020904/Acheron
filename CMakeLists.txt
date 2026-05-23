cmake_minimum_required(VERSION 3.21)

# =============================================================
# Project Acheron — Melbourne Urban Simulation
# Cross-platform C++20 city simulation project
# Supports: Windows (MSVC / MinGW), macOS (AppleClang / Clang)
# =============================================================
project(Acheron
    VERSION 0.7.0
    DESCRIPTION "Melbourne Urban Simulation — C++20 Systems Programming Showcase"
    LANGUAGES CXX
)

cmake_policy(SET CMP0135 NEW)  # FetchContent timestamp policy

# ------------------------------------------------------------------
# C++ Standard
# ------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------------
# Output Directories
# All outputs go into build/ subdirectories regardless of generator
# ------------------------------------------------------------------
set(ACHERON_BIN_DIR   "${CMAKE_SOURCE_DIR}/build/bin")
set(ACHERON_OBJ_DIR   "${CMAKE_SOURCE_DIR}/build/obj")
set(ACHERON_TEMP_DIR  "${CMAKE_SOURCE_DIR}/build/temp")

file(MAKE_DIRECTORY "${ACHERON_BIN_DIR}")
file(MAKE_DIRECTORY "${ACHERON_OBJ_DIR}")
file(MAKE_DIRECTORY "${ACHERON_TEMP_DIR}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ACHERON_BIN_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${ACHERON_OBJ_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${ACHERON_OBJ_DIR}")

# Per-config output dirs (Visual Studio / multi-config generators)
foreach(CONFIG Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${CONFIG}" CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${ACHERON_BIN_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${ACHERON_OBJ_DIR}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${ACHERON_OBJ_DIR}")
endforeach()

# MSVC-specific PDB output (Windows only)
if(MSVC)
    set(CMAKE_PDB_OUTPUT_DIRECTORY         "${ACHERON_TEMP_DIR}")
    set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY "${ACHERON_TEMP_DIR}")
    foreach(CONFIG Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER "${CONFIG}" CONFIG_UPPER)
        set(CMAKE_PDB_OUTPUT_DIRECTORY_${CONFIG_UPPER}         "${ACHERON_TEMP_DIR}")
        set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY_${CONFIG_UPPER} "${ACHERON_TEMP_DIR}")
    endforeach()
endif()

# ------------------------------------------------------------------
# Dependencies — all via FetchContent (no system installs required)
# ------------------------------------------------------------------
include(FetchContent)

# TLS verification — MinGW GnuTLS often lacks root CA bundle, so default OFF
# for maximum portability. On systems with proper CA bundles this is harmless.
# Set -DACHERON_ENABLE_TLS_VERIFY=ON to enforce certificate checks.
option(ACHERON_ENABLE_TLS_VERIFY "Enable TLS certificate verification for FetchContent downloads" OFF)
if(NOT ACHERON_ENABLE_TLS_VERIFY)
    set(CMAKE_TLS_VERIFY OFF CACHE BOOL "" FORCE)
endif()

# Raylib — cross-platform window, input, audio, 2D rendering
set(BUILD_EXAMPLES   OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES      OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
)
FetchContent_MakeAvailable(raylib)

# ------------------------------------------------------------------
# Threading (required on some platforms for std::thread etc.)
# ------------------------------------------------------------------
find_package(Threads REQUIRED)

# ------------------------------------------------------------------
# Source Files
# ------------------------------------------------------------------
file(GLOB_RECURSE CORE_SOURCES            CONFIGURE_DEPENDS src/core/*.cpp)
file(GLOB_RECURSE ECS_SOURCES             CONFIGURE_DEPENDS src/ecs/*.cpp)
file(GLOB_RECURSE THREADING_SOURCES       CONFIGURE_DEPENDS src/threading/*.cpp)
file(GLOB_RECURSE SCHEDULER_SOURCES       CONFIGURE_DEPENDS src/simulation/scheduler/*.cpp)
file(GLOB_RECURSE TRAFFIC_SOURCES         CONFIGURE_DEPENDS src/simulation/traffic/*.cpp)
file(GLOB_RECURSE ECONOMY_SOURCES         CONFIGURE_DEPENDS src/simulation/economy/*.cpp)
file(GLOB_RECURSE CITIZEN_SOURCES         CONFIGURE_DEPENDS src/simulation/citizens/*.cpp)
file(GLOB_RECURSE GAMEPLAY_SOURCES        CONFIGURE_DEPENDS src/gameplay/*.cpp src/gameplay/*/*.cpp)
file(GLOB_RECURSE RENDERER_SOURCES        CONFIGURE_DEPENDS src/engine/renderer/*.cpp)
file(GLOB_RECURSE AUDIO_SOURCES           CONFIGURE_DEPENDS src/engine/audio/*.cpp)
file(GLOB_RECURSE UTILITY_SOURCES         CONFIGURE_DEPENDS src/utilities/*.cpp)
file(GLOB_RECURSE UTILITY_MEMORY_SOURCES  CONFIGURE_DEPENDS src/utilities/memory/*.cpp)
file(GLOB_RECURSE UI_SOURCES              CONFIGURE_DEPENDS src/ui/*.cpp)
file(GLOB_RECURSE WORLD_SOURCES           CONFIGURE_DEPENDS src/world/*.cpp)

# ------------------------------------------------------------------
# Executable Target
# ------------------------------------------------------------------
add_executable(acheron
    src/main.cpp
    ${CORE_SOURCES}
    ${ECS_SOURCES}
    ${THREADING_SOURCES}
    ${SCHEDULER_SOURCES}
    ${TRAFFIC_SOURCES}
    ${ECONOMY_SOURCES}
    ${CITIZEN_SOURCES}
    ${GAMEPLAY_SOURCES}
    ${RENDERER_SOURCES}
    ${AUDIO_SOURCES}
    ${UTILITY_SOURCES}
    ${UTILITY_MEMORY_SOURCES}
    ${UI_SOURCES}
    ${WORLD_SOURCES}
)

set_target_properties(acheron PROPERTIES OUTPUT_NAME "acheron")

# ------------------------------------------------------------------
# Include Paths (target-scoped, not global)
# ------------------------------------------------------------------
target_include_directories(acheron PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}"
)

# ------------------------------------------------------------------
# Link Libraries
# ------------------------------------------------------------------
target_link_libraries(acheron PRIVATE
    raylib
    Threads::Threads
)

# macOS requires explicit framework links for OpenGL/system APIs
if(APPLE)
    target_link_libraries(acheron PRIVATE
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreAudio"
        "-framework AudioToolbox"
    )
endif()

# Windows: link system libs needed by raylib/OpenGL
if(WIN32 AND NOT MSVC)
    target_link_libraries(acheron PRIVATE
        opengl32 glu32 winmm
    )
endif()

# ------------------------------------------------------------------
# Compiler Warnings
# Strict but not fatal — different compiler versions emit different
# warnings; -Werror/-WX would break portability across environments.
# ------------------------------------------------------------------
if(MSVC)
    target_compile_options(acheron PRIVATE
        /W4
        /permissive-
        /Zc:__cplusplus
        /wd4100   # unreferenced formal parameter (common in callbacks)
        /wd4201   # nonstandard extension: nameless struct/union
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
    target_compile_options(acheron PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(acheron PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )
endif()

# ------------------------------------------------------------------
# macOS: minimum deployment target for AppleClang
# ------------------------------------------------------------------
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "macOS minimum deployment target")
    target_compile_options(acheron PRIVATE -mmacosx-version-min=11.0)
endif()

# ------------------------------------------------------------------
# Version definitions
# ------------------------------------------------------------------
target_compile_definitions(acheron PRIVATE
    ACHERON_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    ACHERON_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    ACHERON_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# ------------------------------------------------------------------
# Unit Tests
# ------------------------------------------------------------------
enable_testing()

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS tests/unit/*.cpp)
if(TEST_SOURCES)
    add_executable(acheron_unit_tests
        ${TEST_SOURCES}
        ${CORE_SOURCES}
        ${ECS_SOURCES}
        ${THREADING_SOURCES}
        ${SCHEDULER_SOURCES}
        ${UTILITY_SOURCES}
        ${UTILITY_MEMORY_SOURCES}
    )

    target_include_directories(acheron_unit_tests PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}"
    )

    target_link_libraries(acheron_unit_tests PRIVATE Threads::Threads)

    target_compile_definitions(acheron_unit_tests PRIVATE ACHERON_UNIT_TEST)

    if(MSVC)
        target_compile_options(acheron_unit_tests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(acheron_unit_tests PRIVATE -Wall -Wextra -Wno-unused-parameter)
    endif()

    add_test(NAME AllUnitTests COMMAND acheron_unit_tests)
endif()

# ------------------------------------------------------------------
# CPack — simple ZIP distribution for showcase
# ------------------------------------------------------------------
set(CPACK_PACKAGE_NAME              "Acheron")
set(CPACK_PACKAGE_VERSION           "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR            "University of Melbourne — CS Showcase")
set(CPACK_PACKAGE_DESCRIPTION_SHORT "Melbourne Urban Simulation")
set(CPACK_GENERATOR                 "ZIP")
set(CPACK_PACKAGE_DIRECTORY         "${CMAKE_SOURCE_DIR}/build/dist")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)

install(TARGETS acheron
    RUNTIME DESTINATION bin
    BUNDLE  DESTINATION bin
)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/"
    DESTINATION assets
    OPTIONAL
)

include(CPack)
