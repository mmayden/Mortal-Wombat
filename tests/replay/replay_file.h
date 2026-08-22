// Replay file format. ADR 0011.
//
// A replay is a seed plus an ordered input stream plus the state hashes that
// stream should produce. Replaying it and comparing hashes is the only test
// that can answer "does the game still play the same?" — unit tests
// structurally cannot see a behavioral regression, and an agent refactor that
// compiles and passes every unit test while changing how the game plays is
// exactly the failure this catches.
//
// The format is text so that it diffs, reviews, and merges like code. A binary
// recording would be one more thing review cannot see into.
//
// This code lives above the sim boundary, so it may allocate and do I/O.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "sim/input.h"

namespace mw::test {

// One input change. Inputs are stored as changes rather than per frame,
// because a recording is mostly a fighter holding a direction — run-length
// form keeps a ten-second recording to a few dozen readable lines.
struct InputChange {
    int32_t frame;
    mw::sim::InputFrame players[2];
};

// A full-state hash at a specific frame. Recorded periodically so a desync
// reports the frame it started on rather than only that the end differed.
struct Checkpoint {
    int32_t frame;
    uint64_t hash;
};

struct Replay {
    std::string name;
    std::string description;
    uint64_t seed = 0;
    int32_t frame_count = 0;
    std::vector<InputChange> changes;
    std::vector<Checkpoint> checkpoints;
    uint64_t final_visible_hash = 0;
};

enum class ReplayIoStatus {
    Ok,
    FileMissing,
    BadVersion,
    Malformed,
    WriteFailed,
};

const char* replay_io_status_name(ReplayIoStatus status);

ReplayIoStatus load_replay(const std::string& path, Replay& out, std::string& error);
ReplayIoStatus save_replay(const std::string& path, const Replay& replay);

// The input in effect on a given frame, resolved from the change list.
mw::sim::InputPair input_at_frame(const Replay& replay, int32_t frame);

}  // namespace mw::test
