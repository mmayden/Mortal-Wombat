# Test tiers. Each gets a CTest label so `ctest -L unit` works and CI can run
# the tiers separately — see docs/CONVENTIONS.md section 6.
#
# Tests link ds_sim directly and never boot SDL. That is only possible because
# the sim has no platform dependency, and it is the practical payoff of the
# architecture's one-way rule.
#
# Unlike the sim, tests build WITH exceptions and RTTI: doctest reports
# failures by throwing, and a test binary cannot desync anything.

# Scratch directory for tests that need to write a file.
#
# Inside the build tree, never the source tree. Tests were writing probe files
# into data/characters/, which pollutes the repo, races if ctest runs in
# parallel, and leaves droppings behind when a test aborts. A test that dirties
# the working copy is a test people learn to run less often.
set(DS_TEST_SCRATCH_DIR "${CMAKE_CURRENT_BINARY_DIR}/scratch")
file(MAKE_DIRECTORY "${DS_TEST_SCRATCH_DIR}")

add_library(ds_test_main STATIC doctest_main.cpp)
# ds_exceptions is PUBLIC: doctest reports failures by throwing, so every
# test tier that links this needs exceptions on. The sim itself is built
# without them (ADR 0001); a no-exceptions library links into an
# exceptions-enabled binary fine, since the sim never throws.
target_link_libraries(ds_test_main PUBLIC doctest::doctest ds_exceptions)
target_include_directories(ds_test_main PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# Helper: one executable per test tier, labelled for ctest -L.
function(ds_add_test_tier tier)
    set(target "ds_test_${tier}")
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE ds_test_main ds_sim ds_data ds_warnings)
    target_compile_definitions(${target} PRIVATE DS_TEST_SCRATCH_DIR="${DS_TEST_SCRATCH_DIR}")

    add_test(NAME ${tier} COMMAND ${target})
    set_tests_properties(${tier} PROPERTIES LABELS "${tier}")
endfunction()

# Tier 1 — pure logic, no engine. Target: under 1 second.
ds_add_test_tier(unit
    unit/test_fixed.cpp
    unit/test_rng.cpp
    unit/test_input.cpp
    unit/test_state.cpp
    unit/test_hash.cpp
    unit/test_framedata.cpp
    unit/test_combat.cpp
    unit/test_jump.cpp
    unit/test_input_isolation.cpp
    unit/test_jump_attack_reach.cpp
    unit/test_camera.cpp
    unit/test_reach.cpp
    unit/test_sprite.cpp)

# The camera is render-layer code, so the unit tier needs it linked explicitly.
# Everything else in this tier reaches the sim through ds_sim alone.
target_link_libraries(ds_test_unit PRIVATE ds_render_camera ds_render_sprite)

# Tier 3 — smoke: boot, run 600 ticks, assert no crash and no runaway state.
ds_add_test_tier(smoke
    smoke/test_smoke.cpp)

# Tier 4 — replay: recorded seed + inputs must reproduce a recorded hash.
# This is the regression net that unit tests structurally cannot provide.
ds_add_test_tier(replay
    replay/test_replay.cpp
    replay/replay_file.cpp)

# The replay tier reads recordings from disk, so it needs to know where the
# repo is. Passed as a compile definition rather than resolved at runtime,
# because a test that silently finds zero recordings passes forever.
target_compile_definitions(ds_test_replay PRIVATE
    DS_REPLAY_DIR="${CMAKE_CURRENT_SOURCE_DIR}/replays")

# Regenerates the committed recordings. Not a test — it is the tool you run
# when a deliberate behavior change invalidates the replay tier, and its output
# gets committed alongside that change (AGENTS.md rule 8).
add_executable(ds_replay_record
    replay/record_replay.cpp
    replay/replay_file.cpp)
target_link_libraries(ds_replay_record PRIVATE ds_sim ds_data ds_warnings ds_exceptions)
target_include_directories(ds_replay_record PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(ds_replay_record PRIVATE
    DS_REPLAY_DIR="${CMAKE_CURRENT_SOURCE_DIR}/replays")

# Emits per-frame state hashes for one scenario. CI runs this on Linux,
# Windows, and macOS and diffs the output — see .github/workflows/ci.yml.
#
# ADR 0011 calls the desync check the most important test in the project:
# cross-platform divergence is the failure that ships broken and cannot be
# debugged after release.
add_executable(ds_desync_probe
    replay/desync_probe.cpp
    replay/replay_file.cpp)
target_link_libraries(ds_desync_probe PRIVATE ds_sim ds_data ds_warnings ds_exceptions)
target_include_directories(ds_desync_probe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(ds_desync_probe PRIVATE
    DS_REPLAY_DIR="${CMAKE_CURRENT_SOURCE_DIR}/replays")

# The sim boundary check. Every rule it enforces is a desync that compiles and
# passes every other test — see tests/check_sim_boundary.py.
# Python3 is located at top level so the configuration summary can report it.
if(Python3_FOUND)
    add_test(NAME sim_boundary
        COMMAND ${Python3_EXECUTABLE}
                "${CMAKE_CURRENT_SOURCE_DIR}/check_sim_boundary.py"
                "${CMAKE_SOURCE_DIR}/src/sim")
    set_tests_properties(sim_boundary PROPERTIES LABELS "unit;boundary")
else()
    message(WARNING
        "Python3 not found: the sim boundary check will not run. This check "
        "catches determinism violations that compile and pass every other "
        "test. CI must not be configured this way.")
endif()
