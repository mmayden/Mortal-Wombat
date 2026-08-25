// Records a played session as a replay file.
//
// This exists because a camera defect was reported twice in plain English --
// "if I move right, the other character moves left towards me" -- and both
// times the only way to chase it was to guess what the player had been doing.
// The first attempt cost two wrong theories. The second could not reproduce the
// reported case at all and had to ask.
//
// A recording removes the guessing: the player plays, quits, and the file
// replays their exact session frame by frame, forever, on any machine. That is
// the same guarantee the replay test tier already relies on (ADR 0011), and it
// works for the same reason -- a match is a seed plus an input stream and
// nothing else.
//
// The file is written in the format tests/replay/replay_file.h reads, so a
// recorded session can be dropped straight into tests/replays/ and become a
// regression test. A bug a human found once then cannot come back unnoticed.
//
// This is platform-layer code, above the sim boundary. It writes with <cstdio>
// rather than <fstream> because the game binary is built without exceptions and
// ADR 0001 keeps iostreams out of it; the test-side reader has no such limit.
#pragma once

#include <cstdint>

#include "sim/input.h"

namespace ds::sim {
struct GameState;
}

namespace ds::platform {

// Ten minutes of input changes at worst. A change is only stored when a button
// actually changes, so ordinary play produces a few hundred -- someone mashing
// every frame for ten minutes would produce 36000 and be truncated, which the
// file reports rather than hiding.
inline constexpr int32_t MAX_RECORDED_CHANGES = 36000;

// One checkpoint every 30 frames for ten minutes.
inline constexpr int32_t MAX_RECORDED_CHECKPOINTS = 1200;

// Must match CHECKPOINT_INTERVAL in tests/replay/scenarios.h, or a recording
// made here will not line up with what the replay runner expects.
inline constexpr int32_t RECORDER_CHECKPOINT_INTERVAL = 30;

struct RecordedChange {
    int32_t frame;
    uint16_t buttons[2];
};

struct RecordedCheckpoint {
    int32_t frame;
    uint64_t hash;
};

// Fixed-size on purpose: no allocation in the frame loop, and the cost is
// visible in one place rather than growing quietly during a long session.
struct SessionRecorder {
    bool active = false;
    bool truncated = false;
    uint64_t seed = 0;
    int32_t frame_count = 0;
    int32_t change_count = 0;
    int32_t checkpoint_count = 0;
    RecordedChange changes[MAX_RECORDED_CHANGES];
    RecordedCheckpoint checkpoints[MAX_RECORDED_CHECKPOINTS];
};

void recorder_begin(SessionRecorder& recorder, uint64_t seed);

// Call once per simulation frame, immediately AFTER advance_frame.
//
// `input` is the input that was just fed to the sim, and `state` is the result.
// The ordering matters: the replay runner hashes after advancing, so a
// checkpoint taken before would never match.
void recorder_frame(SessionRecorder& recorder, ds::sim::InputPair input,
                    const ds::sim::GameState& state);

// Writes the recording. Returns false if the file could not be written, having
// already explained why on stderr.
bool recorder_write(const SessionRecorder& recorder, const char* path,
                    const ds::sim::GameState& final_state);

}  // namespace ds::platform
