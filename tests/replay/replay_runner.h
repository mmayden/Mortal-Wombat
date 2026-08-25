// Runs a scenario or a loaded recording through the sim.
//
// Shared by the replay test and the recorder so that the two cannot disagree
// about what "running this replay" means — if they could, re-recording would
// produce a file the test then rejects.
#pragma once

#include <cstdint>
#include <vector>

#include "replay/replay_file.h"
#include "replay/scenarios.h"
#include "sim/hash.h"
#include "sim/sim.h"

#include "match_data.h"

namespace ds::test {

struct RunResult {
    std::vector<Checkpoint> checkpoints;
    uint64_t final_visible_hash = 0;
    ds::sim::GameState final_state{};
};

// Runs `frame_count` frames from `seed`, taking input from `script`.
//
// Input for frame N is script(N), and the previous frame's input is
// script(N-1) — the sim derives button edges rather than storing them, so the
// caller has to supply both. On the first frame the previous input is neutral.
inline RunResult run_scenario(uint64_t seed, int32_t frame_count,
                              ds::sim::InputPair (*script)(int32_t)) {
    RunResult result;
    ds::sim::init_state(result.final_state, seed);

    ds::sim::InputPair previous{{ds::sim::InputFrame{0u}, ds::sim::InputFrame{0u}}};

    for (int32_t frame = 0; frame < frame_count; ++frame) {
        const ds::sim::InputPair current = script(frame);
        ds::sim::advance_frame(result.final_state, ds::test::shipped_match_data(), current,
                               previous);
        previous = current;

        if ((frame + 1) % CHECKPOINT_INTERVAL == 0) {
            result.checkpoints.push_back(
                Checkpoint{frame + 1, ds::sim::hash_state(result.final_state)});
        }
    }

    result.final_visible_hash = ds::sim::hash_state_visible(result.final_state);
    return result;
}

// Runs a loaded recording, taking input from its change list rather than from
// a script. This is the path the test uses, so that the recorded FILE is what
// is under test — not the code that generated it.
inline RunResult run_replay(const Replay& replay) {
    RunResult result;
    ds::sim::init_state(result.final_state, replay.seed);

    ds::sim::InputPair previous{{ds::sim::InputFrame{0u}, ds::sim::InputFrame{0u}}};

    for (int32_t frame = 0; frame < replay.frame_count; ++frame) {
        const ds::sim::InputPair current = input_at_frame(replay, frame);
        ds::sim::advance_frame(result.final_state, ds::test::shipped_match_data(), current,
                               previous);
        previous = current;

        if ((frame + 1) % CHECKPOINT_INTERVAL == 0) {
            result.checkpoints.push_back(
                Checkpoint{frame + 1, ds::sim::hash_state(result.final_state)});
        }
    }

    result.final_visible_hash = ds::sim::hash_state_visible(result.final_state);
    return result;
}

// Compresses a per-frame script into the run-length change list the file
// format stores. A ten-second recording of someone holding a direction becomes
// two lines instead of six hundred.
inline std::vector<InputChange> compress_script(int32_t frame_count,
                                                ds::sim::InputPair (*script)(int32_t)) {
    std::vector<InputChange> changes;
    ds::sim::InputPair previous{{ds::sim::InputFrame{0u}, ds::sim::InputFrame{0u}}};

    for (int32_t frame = 0; frame < frame_count; ++frame) {
        const ds::sim::InputPair current = script(frame);
        const bool changed = frame == 0 ||
                             current.players[0].buttons != previous.players[0].buttons ||
                             current.players[1].buttons != previous.players[1].buttons;
        if (changed) {
            InputChange change{};
            change.frame = frame;
            change.players[0] = current.players[0];
            change.players[1] = current.players[1];
            changes.push_back(change);
        }
        previous = current;
    }
    return changes;
}

}  // namespace ds::test
