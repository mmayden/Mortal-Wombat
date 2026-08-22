cmake_minimum_required(VERSION 3.25)

project(mortal_wombat
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

option(MW_ENABLE_TRACY "Build with Tracy profiling instrumentation" OFF)

# ADR 0010 asks for ASan and UBSan in the debug preset. On Clang and GCC that
# is a flag. On MSVC it is not: /fsanitize=address conflicts with the runtime
# checks (/RTC1) that CMake's Debug configuration adds, and UBSan has no MSVC
# equivalent at all. Rather than silently dropping the guarantee or breaking
# the default Windows build, default it off for MSVC and let CI on Linux carry
# the sanitizer coverage — the sim is platform-independent, so a memory bug it
# catches there is the same bug here.
if(MSVC)
    set(MW_SANITIZERS_DEFAULT OFF)
else()
    set(MW_SANITIZERS_DEFAULT ON)
endif()
option(MW_ENABLE_SANITIZERS "Build Debug with AddressSanitizer and UBSan"
       ${MW_SANITIZERS_DEFAULT})

# ---------------------------------------------------------------------------
# Dependencies (ADR 0010)
#
# Adding one here is a serialized hotspot: land it on main by itself before
# branching work that needs it, or three branches conflict on this block.
# AGENTS.md hard rule 4 — ask before adding.
# ---------------------------------------------------------------------------
include(CPM)

CPMAddPackage(
    NAME doctest
    GITHUB_REPOSITORY doctest/doctest
    GIT_TAG v2.4.11
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

# ---------------------------------------------------------------------------
# Warning and language-subset flags
#
# ADR 0001 restricts the codebase to a documented C++ subset. Exceptions and
# RTTI are disabled for the simulation and the shipped game binary, where they
# cost determinism-relevant control flow and code size for nothing. Tests and
# tools keep them, because doctest reports failures through exceptions and
# tools cannot desync anything (ARCHITECTURE.md 2).
# ---------------------------------------------------------------------------
add_library(mw_warnings INTERFACE)
add_library(mw_no_exceptions INTERFACE)
add_library(mw_exceptions INTERFACE)

if(MSVC)
    target_compile_options(mw_warnings INTERFACE
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

    target_compile_options(mw_no_exceptions INTERFACE /EHs-c- /GR-)
    target_compile_options(mw_exceptions INTERFACE /EHsc)
else()
    target_compile_options(mw_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Werror
        -Wconversion               # catches silent int32 -> int16 in frame data
        -Wsign-conversion
        -Wshadow
        -Wold-style-cast
        -Wdouble-promotion)        # a stray float below the boundary is a desync
    target_compile_options(mw_no_exceptions INTERFACE -fno-exceptions -fno-rtti)
    target_compile_options(mw_exceptions INTERFACE -fexceptions -frtti)
endif()

# Sanitizers apply to everything, tests included — a memory bug in a test
# harness is as capable of producing a false green as one in the sim.
add_library(mw_sanitizers INTERFACE)
if(MW_ENABLE_SANITIZERS AND NOT MSVC)
    set(MW_SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_compile_options(mw_sanitizers INTERFACE
        "$<$<CONFIG:Debug>:${MW_SANITIZER_FLAGS}>")
    target_link_options(mw_sanitizers INTERFACE
        "$<$<CONFIG:Debug>:${MW_SANITIZER_FLAGS}>")
elseif(MW_ENABLE_SANITIZERS AND MSVC)
    # /RTC1 comes from CMake's Debug defaults and cannot coexist with ASan.
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    target_compile_options(mw_sanitizers INTERFACE "$<$<CONFIG:Debug>:/fsanitize=address>")
endif()
link_libraries(mw_sanitizers)

# ---------------------------------------------------------------------------
# The simulation (ADR 0002, 0005)
#
# Deterministic, fixed-point, no allocation, no I/O, no platform dependency.
# It links nothing. That is not an oversight — it is why the test suite can
# exercise the whole game without booting SDL.
# ---------------------------------------------------------------------------
add_library(mw_sim STATIC
    src/sim/sim.cpp)

target_include_directories(mw_sim PUBLIC src)

# Both PRIVATE: these govern how the sim is compiled, not how it is consumed.
# Propagating them would force every consumer to give up exceptions too, which
# would break doctest (it reports failures by throwing) for no benefit — a
# no-exceptions static library links into an exceptions-enabled binary fine,
# since the sim never throws.
target_link_libraries(mw_sim PRIVATE mw_warnings mw_no_exceptions)

# Where the compiler supports it, forbid floating-point registers in the sim
# outright — this turns ADR 0002's central rule from a review item into a
# compile error. Support is uneven across compilers and targets, so it is a
# bonus guard, not the guarantee: tests/check_sim_boundary.py enforces the same
# rule on every platform and is what CI actually depends on.
if(NOT MSVC)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(-mgeneral-regs-only MW_HAS_GENERAL_REGS_ONLY)
    if(MW_HAS_GENERAL_REGS_ONLY)
        target_compile_options(mw_sim PRIVATE -mgeneral-regs-only)
    endif()
endif()

enable_testing()
add_subdirectory(tests)
