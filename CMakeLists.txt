cmake_minimum_required(VERSION 3.25)

project(divided_states
    VERSION 0.1.0
    DESCRIPTION "A 2D fighting game with a deterministic fixed-point simulation"
    LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Keep binaries in one predictable place so AGENTS.md can name an exact path.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(DS_ENABLE_TRACY "Build with Tracy profiling instrumentation" OFF)

# ADR 0010 asks for ASan and UBSan in the debug preset. On Clang and GCC that
# is a flag. On MSVC it is not: /fsanitize=address conflicts with the runtime
# checks (/RTC1) that CMake's Debug configuration adds, and UBSan has no MSVC
# equivalent at all. Rather than silently dropping the guarantee or breaking
# the default Windows build, default it off for MSVC and let CI on Linux carry
# the sanitizer coverage — the sim is platform-independent, so a memory bug it
# catches there is the same bug here.
if(MSVC)
    set(DS_SANITIZERS_DEFAULT OFF)
else()
    set(DS_SANITIZERS_DEFAULT ON)
endif()
option(DS_ENABLE_SANITIZERS "Build Debug with AddressSanitizer and UBSan"
       ${DS_SANITIZERS_DEFAULT})

# ---------------------------------------------------------------------------
# Dependencies (ADR 0010)
#
# Adding one here is a serialized hotspot: land it on main by itself before
# branching work that needs it, or three branches conflict on this block.
# AGENTS.md hard rule 4 — ask before adding.
# ---------------------------------------------------------------------------
include(CPM)

# doctest, chosen for compile speed over Catch2 and GoogleTest (ADR 0011).
#
# 2.5.3 and not 2.4.11: the older release declares
# cmake_minimum_required(VERSION 3.0), and CMake 4.x removed compatibility with
# anything below 3.5, so it fails to configure outright on a current toolchain.
# Locally that only warned, because VS bundles CMake 3.30 -- CI on newer runners
# is what turned it into an error. Pin forward rather than papering over it with
# CMAKE_POLICY_VERSION_MINIMUM, which would keep the dead declaration alive.
CPMAddPackage(
    NAME doctest
    GITHUB_REPOSITORY doctest/doctest
    GIT_TAG v2.5.3
    OPTIONS "DOCTEST_WITH_TESTS OFF"
)

# SDL3 — window, event pump, gamepad, audio device. ADR 0003.
#
# Chosen over raylib because a fighting game is judged on latency as a
# competitive property, and SDL3 gives direct control over event polling and
# main-loop timing where higher-level libraries impose a per-frame model.
#
# Statically linked so the game is one file with no DLL to ship beside it.
# SDL is zlib-licensed, which permits static linking without attribution
# obligations that would complicate distribution.
#
# The subsystems disabled below are ones this project will not use. Each one
# removed is compile time we get back on every clean build and every CI runner,
# and ADR 0010 treats the build budget as a hard requirement.
#
# NOTE: nothing links this yet. It lands alone, ahead of the code that needs
# it, because CMakeLists.txt is the shared-config hotspot (BLUEPRINT.md 5.5) —
# two branches each adding a dependency conflict here no matter how well
# isolated their other work is.
CPMAddPackage(
    NAME SDL3
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG release-3.4.14
    OPTIONS
        "SDL_SHARED OFF"
        "SDL_STATIC ON"
        "SDL_TEST_LIBRARY OFF"
        "SDL_EXAMPLES OFF"
        "SDL_INSTALL OFF"
        "SDL_DISABLE_INSTALL ON"
)

# tomlplusplus — frame data, character definitions, move properties. ADR 0009.
#
# ADR 0009 calls this the highest-leverage architectural decision in the
# project: frame data as text is diffable, reviewable, agent-editable, and
# testable, and it makes balance changes data commits rather than code commits.
#
# TOML_EXCEPTIONS=0 because the game binary builds without exceptions
# (ADR 0001), so the parser has to report failure by return value. That suits
# the loader anyway: parsing happens at load time, outside the sim, where
# failure is recoverable and reportable (CONVENTIONS.md 3).
#
# Header-only, and used at load time only -- never inside advance_frame.
CPMAddPackage(
    NAME tomlplusplus
    GITHUB_REPOSITORY marzer/tomlplusplus
    GIT_TAG v3.4.0
    OPTIONS "TOML_EXCEPTIONS 0"
)

# ---------------------------------------------------------------------------
# Warning and language-subset flags
#
# ADR 0001 restricts the codebase to a documented C++ subset. Exceptions and
# RTTI are disabled for the simulation and the shipped game binary, where they
# cost determinism-relevant control flow and code size for nothing. Tests and
# tools keep them, because doctest reports failures through exceptions and
# tools cannot desync anything (ARCHITECTURE.md 2).
# ---------------------------------------------------------------------------
add_library(ds_warnings INTERFACE)
add_library(ds_no_exceptions INTERFACE)
add_library(ds_exceptions INTERFACE)

if(MSVC)
    target_compile_options(ds_warnings INTERFACE
        /W4 /WX
        /permissive-        # conform to the standard; MSVC's default does not
        /Zc:preprocessor)

    # CMake puts /EHsc in the default flags for every target. Leaving it there
    # and appending /EHs-c- for the sim works, but emits a D9025 "overriding"
    # warning on every single sim build -- and build noise that is always
    # present is build noise nobody reads. Remove it from the default and let
    # each target state which it wants.
    string(REPLACE "/EHsc" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(STRIP "${CMAKE_CXX_FLAGS}" CMAKE_CXX_FLAGS)

    target_compile_options(ds_no_exceptions INTERFACE /EHs-c- /GR-)
    target_compile_options(ds_exceptions INTERFACE /EHsc)
else()
    target_compile_options(ds_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Werror
        -Wconversion               # catches silent int32 -> int16 in frame data
        -Wsign-conversion
        -Wshadow
        -Wold-style-cast
        -Wdouble-promotion)        # a stray float below the boundary is a desync
    target_compile_options(ds_no_exceptions INTERFACE -fno-exceptions -fno-rtti)
    target_compile_options(ds_exceptions INTERFACE -fexceptions -frtti)
endif()

# Sanitizers apply to everything, tests included — a memory bug in a test
# harness is as capable of producing a false green as one in the sim.
add_library(ds_sanitizers INTERFACE)
if(DS_ENABLE_SANITIZERS AND NOT MSVC)
    set(DS_SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_compile_options(ds_sanitizers INTERFACE
        "$<$<CONFIG:Debug>:${DS_SANITIZER_FLAGS}>")
    target_link_options(ds_sanitizers INTERFACE
        "$<$<CONFIG:Debug>:${DS_SANITIZER_FLAGS}>")
elseif(DS_ENABLE_SANITIZERS AND MSVC)
    # /RTC1 comes from CMake's Debug defaults and cannot coexist with ASan.
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    target_compile_options(ds_sanitizers INTERFACE "$<$<CONFIG:Debug>:/fsanitize=address>")
endif()
link_libraries(ds_sanitizers)

# ---------------------------------------------------------------------------
# The simulation (ADR 0002, 0005)
#
# Deterministic, fixed-point, no allocation, no I/O, no platform dependency.
# It links nothing. That is not an oversight — it is why the test suite can
# exercise the whole game without booting SDL.
# ---------------------------------------------------------------------------
add_library(ds_sim STATIC
    src/sim/sim.cpp)

target_include_directories(ds_sim PUBLIC src)

# Both PRIVATE: these govern how the sim is compiled, not how it is consumed.
# Propagating them would force every consumer to give up exceptions too, which
# would break doctest (it reports failures by throwing) for no benefit — a
# no-exceptions static library links into an exceptions-enabled binary fine,
# since the sim never throws.
target_link_libraries(ds_sim PRIVATE ds_warnings ds_no_exceptions)

# Where the compiler supports it, forbid floating-point registers in the sim
# outright — this turns ADR 0002's central rule from a review item into a
# compile error. Support is uneven across compilers and targets, so it is a
# bonus guard, not the guarantee: tests/check_sim_boundary.py enforces the same
# rule on every platform and is what CI actually depends on.
if(NOT MSVC)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(-mgeneral-regs-only DS_HAS_GENERAL_REGS_ONLY)
    if(DS_HAS_GENERAL_REGS_ONLY)
        target_compile_options(ds_sim PRIVATE -mgeneral-regs-only)
    endif()
endif()

# ---------------------------------------------------------------------------
# Frame data loading (ADR 0009)
#
# ABOVE the sim boundary. It does file I/O, allocates, and uses std::string --
# all forbidden below the line. The sim receives finished CharacterData and
# never learns a file was involved, which is what CONVENTIONS.md 3 means by
# "the sim never fails": every failure mode is discovered here, at load time,
# where there is somewhere to report it.
# ---------------------------------------------------------------------------
add_library(ds_data STATIC
    src/data/framedata_loader.cpp)

target_include_directories(ds_data PUBLIC src)
target_link_libraries(ds_data
    PUBLIC ds_sim
    PRIVATE ds_warnings ds_no_exceptions tomlplusplus::tomlplusplus)

# Where the shipped character files live, so the game and the tests agree on
# one answer. Passed as a definition rather than resolved relative to the
# working directory, which differs between running from the build tree, from
# an IDE, and from CI.
target_compile_definitions(ds_data PUBLIC
    DS_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

# MSVC warns (C4530) that standard headers contain exception handlers while
# this target is built without unwind semantics. That is the whole point of
# /EHs-c-, and it is the same situation as -fno-exceptions on GCC and Clang,
# which say nothing about it. Nothing here throws: toml++ is configured with
# TOML_EXCEPTIONS=0 and the loader reports failure by return value.
#
# Scoped to this target rather than added globally, so the warning still fires
# if a future target enables it by accident.
if(MSVC)
    target_compile_options(ds_data PRIVATE /wd4530)
endif()

# ---------------------------------------------------------------------------
# The game binary
#
# Everything here is ABOVE the sim boundary: platform, render, and the loop.
# It links ds_sim and reads its state; nothing in src/sim/ knows these files
# exist (ARCHITECTURE.md 1).
#
# Built with exceptions and RTTI disabled, same as the sim -- this is shipped
# code, and ADR 0001 restricts it to the documented C++ subset. Only tests and
# tools get the full language.
# ---------------------------------------------------------------------------
# The camera, alone in the render layer, touches no SDL -- it is pure arithmetic
# over two fighter positions. Splitting it out makes it testable, which the rest
# of the render layer is not.
#
# It is here because a camera defect was reported twice by a human and could not
# be checked by anything: AGENTS.md rule 6 says every behaviour change needs a
# test, and the render layer had quietly been outside that rule.
add_library(ds_render_camera STATIC
    src/render/camera.cpp)

target_link_libraries(ds_render_camera
    PUBLIC ds_sim
    PRIVATE ds_warnings ds_no_exceptions)

# The sprite manifest is the other SDL-free piece of the render layer: it turns
# a fighter into a list of coloured quads and never draws anything itself.
#
# Split out for the same reason as the camera -- a human reported that all six
# attack buttons looked identical, and nothing could check whether they did.
add_library(ds_render_sprite STATIC
    src/render/sprite.cpp)

target_link_libraries(ds_render_sprite
    PUBLIC ds_sim
    PRIVATE ds_warnings ds_no_exceptions)

target_include_directories(ds_render_sprite PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_include_directories(ds_render_camera PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_executable(divided_states
    src/main.cpp
    src/platform/platform.cpp
    src/platform/session_recorder.cpp
    src/render/renderer.cpp)

target_link_libraries(divided_states
    PRIVATE ds_sim ds_data ds_render_camera ds_render_sprite ds_warnings ds_no_exceptions
            SDL3::SDL3)

# WIN32_EXECUTABLE is deliberately OFF: the game logs to stdout and a console
# is wanted during development. Flip it when there is something to ship.
set_target_properties(divided_states PROPERTIES WIN32_EXECUTABLE OFF)

# ---------------------------------------------------------------------------
# tools/framedata_viewer -- draws what the character TOML says.
#
# The stack decision said to build this BEFORE authoring content, and that was
# overrun: every hitbox in data/characters/ was hand-written, and the jump
# attack shipped with its hitbox at standing-punch height where it could not
# touch anyone from any range at any timing. Four plausible integers, invisible
# in review, obvious the moment they are drawn.
#
# Pure tooling: no sim contact, cannot desync anything (tools/README.md).
# ---------------------------------------------------------------------------
add_executable(ds_framedata_viewer
    tools/framedata_viewer/main.cpp)

target_link_libraries(ds_framedata_viewer
    PRIVATE ds_sim ds_data ds_warnings ds_exceptions SDL3::SDL3)

target_include_directories(ds_framedata_viewer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tools")

# Located at top level, before the summary below reports on it. Doing this in
# tests/ meant the summary ran first and always reported it missing -- the
# summary caught that about itself on its first run.
find_package(Python3 COMPONENTS Interpreter)

# ---------------------------------------------------------------------------
# Configuration summary
#
# Printed because a guarantee nobody can observe is not a guarantee. Ninja does
# not echo command lines, so "the debug build has sanitizers" was previously
# unverifiable from a CI log -- the only evidence was this file, which is the
# thing you would be trying to check.
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "Divided States ${PROJECT_VERSION}")
message(STATUS "  build type    : ${CMAKE_BUILD_TYPE}")
message(STATUS "  compiler      : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

if(DS_ENABLE_SANITIZERS)
    if(MSVC)
        message(STATUS "  sanitizers    : ASan (Debug only). MSVC has no UBSan.")
    else()
        message(STATUS "  sanitizers    : ASan + UBSan (Debug only)")
    endif()
else()
    message(STATUS "  sanitizers    : OFF")
endif()

if(DS_HAS_GENERAL_REGS_ONLY)
    message(STATUS "  sim FP guard  : -mgeneral-regs-only (a float in src/sim/ is a compile error)")
else()
    message(STATUS "  sim FP guard  : unsupported here; tests/check_sim_boundary.py covers it")
endif()

if(Python3_Interpreter_FOUND OR Python3_FOUND)
    message(STATUS "  boundary check: enabled")
else()
    message(STATUS "  boundary check: DISABLED -- Python not found")
endif()

message(STATUS "  Tracy         : ${DS_ENABLE_TRACY} (not yet integrated -- ADR 0012)")
message(STATUS "")

enable_testing()
add_subdirectory(tests)
