// The simulation entry point. ADR 0002, ADR 0007.
//
// advance_frame is the entire game. It takes state and inputs and nothing
// else — no time, no delta, no clock, no access to a file or a device. That
// signature is not a style choice; it is the contract rollback consumes:
//
//   save_state()            -> memcpy GameState
//   load_state()            -> memcpy back
//   advance_frame(inputs)   -> this function
//
// Anything that makes this function impure — a log line, a sound call, a
// wall-clock read — breaks rollback, because rollback runs it repeatedly over
// the same frames and any side effect fires once per re-simulation.
#pragma once

#include "sim/framedata.h"
#include "sim/input.h"
#include "sim/state.h"

namespace ds::sim {

// Advances the match by exactly one 60Hz frame.
//
// `data` is the loaded frame data for both fighters. It is immutable for the
// whole match and deliberately NOT part of GameState (ARCHITECTURE.md 3):
// rollback copies GameState up to 8x per frame, and copying data that cannot
// change would be waste. Passing it as a separate argument keeps advance_frame
// a pure function of (state, config, inputs) -- still exactly reproducible from
// a seed and an input stream, which is what the replay and rollback contracts
// require.
//
// `previous` is last frame's input, used to compute button edges. The sim
// derives edges rather than storing them so that a rollback recomputes them
// correctly instead of restoring a stale flag.
//
// Step order is fixed and documented in ARCHITECTURE.md section 5. Changing it
// is a behavior change: re-record the affected replays in the same commit.
void advance_frame(GameState& state, const MatchData& data, InputPair current, InputPair previous);

// Whether two axis-aligned boxes overlap, in world space.
//
// Exposed for tests and for the training-mode overlay. Touching edges do NOT
// count as overlapping: a hitbox whose right edge exactly meets a hurtbox's
// left edge has not reached it, and treating that as a hit would make every
// move one unit longer than its data says.
bool boxes_overlap(const Box& a, const Box& b);

// Converts a box authored in a fighter's local space into world space,
// mirroring x when the fighter faces left.
//
// Authored data is always written facing right (docs/framedata_schema.md), so
// this is the only place facing is applied to geometry.
Box world_box(const Fighter& fighter, const Box& local);

}  // namespace ds::sim
