// Replay tests. ADR 0011.
//
// The single highest-value test tier in a fighting game. Unit tests cannot see
// a behavioral regression: an agent refactors movement, everything compiles,
// every unit test passes, and fighters now travel a fraction of a unit further
// per frame. Only a replay catches that.
//
// A failure here is not automatically a bug. If the behavior change was
// deliberate, re-record with mw_replay_record and commit the new recordings IN
// THE SAME COMMIT as the change (AGENTS.md rule 8).
#include <cstdio>
#include <string>

#include <doctest/doctest.h>

#include "replay/replay_file.h"
#include "replay/replay_runner.h"
#include "replay/scenarios.h"

using namespace mw::test;

namespace {

std::string replay_path(const char* name) {
    return std::string(MW_REPLAY_DIR) + "/" + name + ".replay";
}

}  // namespace

TEST_CASE("Every scenario has a committed recording that still reproduces") {
    for (int32_t i = 0; i < SCENARIO_COUNT; ++i) {
        const Scenario& scenario = SCENARIOS[i];
        CAPTURE(scenario.name);

        const std::string path = replay_path(scenario.name);
        Replay replay;
        std::string error;
        const ReplayIoStatus status = load_replay(path, replay, error);

        // A missing recording must fail rather than skip. A replay tier that
        // silently tests nothing is worse than no replay tier, because it is
        // trusted. Run mw_replay_record to generate them.
        REQUIRE_MESSAGE(status == ReplayIoStatus::Ok, replay_io_status_name(status), ": ", error);

        CHECK(replay.seed == scenario.seed);
        CHECK(replay.frame_count == scenario.frame_count);

        const RunResult result = run_replay(replay);

        SUBCASE("checkpoints match frame by frame") {
            REQUIRE(result.checkpoints.size() == replay.checkpoints.size());

            // Report the FIRST divergence and stop. Every later checkpoint
            // will also differ, and a wall of failures buries the one frame
            // that actually matters.
            for (size_t c = 0; c < replay.checkpoints.size(); ++c) {
                const Checkpoint& expected = replay.checkpoints[c];
                const Checkpoint& actual = result.checkpoints[c];

                REQUIRE(actual.frame == expected.frame);
                if (actual.hash != expected.hash) {
                    char message[256];
                    std::snprintf(message, sizeof(message),
                                  "%s diverged at frame %d: expected 0x%016llX, got 0x%016llX",
                                  scenario.name, expected.frame,
                                  static_cast<unsigned long long>(expected.hash),
                                  static_cast<unsigned long long>(actual.hash));
                    FAIL(message);
                }
            }
        }

        SUBCASE("the final visible state matches") {
            CHECK(result.final_visible_hash == replay.final_visible_hash);
        }
    }
}

TEST_CASE("A recording reproduces from its file, not from its script") {
    // Guards the thing that makes the tier meaningful: the committed file is
    // the artifact under test. If run_replay and run_scenario disagreed, the
    // test would be checking the generator against itself.
    for (int32_t i = 0; i < SCENARIO_COUNT; ++i) {
        const Scenario& scenario = SCENARIOS[i];
        CAPTURE(scenario.name);

        Replay replay;
        std::string error;
        REQUIRE(load_replay(replay_path(scenario.name), replay, error) == ReplayIoStatus::Ok);

        const RunResult from_file = run_replay(replay);
        const RunResult from_script =
            run_scenario(scenario.seed, scenario.frame_count, scenario.script);

        CHECK(from_file.final_visible_hash == from_script.final_visible_hash);
    }
}

TEST_CASE("The combat recordings actually connect") {
    // Without this, the combat replays could silently degrade into whiffs --
    // an agent shifts a hitbox by a few units, the moves stop reaching, and the
    // recordings still reproduce perfectly because they reproduce the NEW
    // behaviour once re-recorded. A replay only guards behaviour it exercises.
    struct Expectation {
        const char* scenario;
        bool expect_damage;
    };

    const Expectation expectations[] = {
        {"punch_connects", true},
        {"mutual_pressure", true},
        // Blocked hits currently deal no damage, so this one asserts the
        // opposite: the attacks land on a block and take nothing off. If chip
        // damage is ever added, this expectation changes with it.
        {"attack_into_block", false},
    };

    for (const Expectation& expectation : expectations) {
        CAPTURE(expectation.scenario);

        Replay replay;
        std::string error;
        REQUIRE(load_replay(replay_path(expectation.scenario), replay, error) ==
                ReplayIoStatus::Ok);

        const RunResult result = run_replay(replay);
        const int32_t p2_health = result.final_state.fighters[1].health;

        if (expectation.expect_damage) {
            CHECK(p2_health < mw::sim::STARTING_HEALTH);
        } else {
            CHECK(p2_health == mw::sim::STARTING_HEALTH);
        }
    }
}

TEST_CASE("The jump recording actually leaves the ground") {
    // Same guard as the combat recordings: a jump scenario whose input never
    // reaches the airborne state reproduces perfectly and proves nothing. This
    // is the failure that already happened once, when scenarios pressed buttons
    // during the round-start freeze and silently did nothing.
    Replay replay;
    std::string error;
    REQUIRE(load_replay(replay_path("jump_attack"), replay, error) == ReplayIoStatus::Ok);

    mw::sim::GameState state{};
    mw::sim::init_state(state, replay.seed);
    mw::sim::InputPair previous{{mw::sim::InputFrame{0u}, mw::sim::InputFrame{0u}}};

    int32_t airborne_frames = 0;
    int32_t jump_attack_frames = 0;
    for (int32_t frame = 0; frame < replay.frame_count; ++frame) {
        const mw::sim::InputPair current = input_at_frame(replay, frame);
        mw::sim::advance_frame(state, mw::test::shipped_match_data(), current, previous);
        previous = current;

        if (state.fighters[0].state == mw::sim::FighterState::Airborne) {
            ++airborne_frames;
            if (state.fighters[0].move_id == static_cast<int32_t>(mw::sim::MoveId::JumpAttack)) {
                ++jump_attack_frames;
            }
        }
    }

    CHECK(airborne_frames > 0);
    CHECK(jump_attack_frames > 0);

    // And that it CONNECTS. Asserting only that the fighter got airborne and
    // threw the move is what let a completely unusable jump attack -- one that
    // could not hit anybody from any range at any timing -- reproduce
    // perfectly and pass for as long as it existed.
    CHECK(state.fighters[1].health < mw::sim::STARTING_HEALTH);
}

TEST_CASE("Blocking in a recording still costs the defender their turn") {
    // The other half of the blocked-hit contract. No damage, but the defender
    // spent time in blockstun -- otherwise "attack into block" would be
    // indistinguishable from "attack into nothing".
    Replay replay;
    std::string error;
    REQUIRE(load_replay(replay_path("attack_into_block"), replay, error) == ReplayIoStatus::Ok);

    mw::sim::GameState state{};
    mw::sim::init_state(state, replay.seed);
    mw::sim::InputPair previous{{mw::sim::InputFrame{0u}, mw::sim::InputFrame{0u}}};

    int32_t blockstun_frames = 0;
    for (int32_t frame = 0; frame < replay.frame_count; ++frame) {
        const mw::sim::InputPair current = input_at_frame(replay, frame);
        mw::sim::advance_frame(state, mw::test::shipped_match_data(), current, previous);
        previous = current;
        if (state.fighters[1].blockstun_remaining > 0) {
            ++blockstun_frames;
        }
    }

    CHECK(blockstun_frames > 0);
}

TEST_CASE("Replaying the same recording twice gives the same result") {
    // If this fails, the sim depends on something outside GameState -- static
    // storage, uninitialized memory, or address-dependent behavior. That is a
    // desync in its purest form and it would break rollback immediately.
    Replay replay;
    std::string error;
    REQUIRE(load_replay(replay_path("approach_retreat"), replay, error) == ReplayIoStatus::Ok);

    const RunResult first = run_replay(replay);
    const RunResult second = run_replay(replay);

    CHECK(first.final_visible_hash == second.final_visible_hash);
    REQUIRE(first.checkpoints.size() == second.checkpoints.size());
    for (size_t i = 0; i < first.checkpoints.size(); ++i) {
        CHECK(first.checkpoints[i].hash == second.checkpoints[i].hash);
    }
}

TEST_CASE("Run-length compression preserves the input stream exactly") {
    // The file stores input changes rather than per-frame input. If
    // compression lost a frame, recordings would silently stop matching what
    // the scenario described.
    for (int32_t i = 0; i < SCENARIO_COUNT; ++i) {
        const Scenario& scenario = SCENARIOS[i];
        CAPTURE(scenario.name);

        Replay replay;
        replay.changes = compress_script(scenario.frame_count, scenario.script);
        replay.frame_count = scenario.frame_count;

        for (int32_t frame = 0; frame < scenario.frame_count; ++frame) {
            const mw::sim::InputPair expected = scenario.script(frame);
            const mw::sim::InputPair actual = input_at_frame(replay, frame);
            REQUIRE(actual.players[0].buttons == expected.players[0].buttons);
            REQUIRE(actual.players[1].buttons == expected.players[1].buttons);
        }
    }
}

TEST_CASE("The loader rejects a recording that asserts nothing") {
    // A file with no expected hashes would pass regardless of what the sim
    // did. Rejecting it at load time is what stops a truncated or hand-edited
    // recording from quietly becoming a no-op test.
    const std::string path = std::string(MW_TEST_SCRATCH_DIR) + "/__malformed_probe.replay";
    Replay written;
    written.name = "probe";
    written.description = "written by the test, then deliberately stripped";
    written.seed = 1u;
    written.frame_count = 10;
    REQUIRE(save_replay(path, written) == ReplayIoStatus::Ok);

    Replay loaded;
    std::string error;
    const ReplayIoStatus status = load_replay(path, loaded, error);

    CHECK(status == ReplayIoStatus::Malformed);
    CHECK(error.find("asserts nothing") != std::string::npos);

    std::remove(path.c_str());
}

TEST_CASE("The loader reports a missing file rather than passing") {
    Replay replay;
    std::string error;
    const ReplayIoStatus status = load_replay(replay_path("__does_not_exist"), replay, error);

    CHECK(status == ReplayIoStatus::FileMissing);
    CHECK_FALSE(error.empty());
}

TEST_CASE("A recording round-trips through save and load") {
    const std::string path = std::string(MW_TEST_SCRATCH_DIR) + "/__roundtrip_probe.replay";

    const Scenario& scenario = SCENARIOS[1];
    const RunResult result = run_scenario(scenario.seed, scenario.frame_count, scenario.script);

    Replay original;
    original.name = scenario.name;
    original.description = scenario.description;
    original.seed = scenario.seed;
    original.frame_count = scenario.frame_count;
    original.changes = compress_script(scenario.frame_count, scenario.script);
    original.checkpoints = result.checkpoints;
    original.final_visible_hash = result.final_visible_hash;

    REQUIRE(save_replay(path, original) == ReplayIoStatus::Ok);

    Replay loaded;
    std::string error;
    REQUIRE(load_replay(path, loaded, error) == ReplayIoStatus::Ok);

    CHECK(loaded.seed == original.seed);
    CHECK(loaded.frame_count == original.frame_count);
    CHECK(loaded.final_visible_hash == original.final_visible_hash);
    REQUIRE(loaded.changes.size() == original.changes.size());
    for (size_t i = 0; i < loaded.changes.size(); ++i) {
        CHECK(loaded.changes[i].frame == original.changes[i].frame);
        CHECK(loaded.changes[i].players[0].buttons == original.changes[i].players[0].buttons);
        CHECK(loaded.changes[i].players[1].buttons == original.changes[i].players[1].buttons);
    }
    REQUIRE(loaded.checkpoints.size() == original.checkpoints.size());
    for (size_t i = 0; i < loaded.checkpoints.size(); ++i) {
        CHECK(loaded.checkpoints[i].frame == original.checkpoints[i].frame);
        CHECK(loaded.checkpoints[i].hash == original.checkpoints[i].hash);
    }

    std::remove(path.c_str());
}
