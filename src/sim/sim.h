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

#include "sim/input.h"
#include "sim/state.h"

namespace mw::sim {

// Advances the match by exactly one 60Hz frame.
//
// `previous` is last frame's input, used to compute button edges. The sim
// derives edges rather than storing them so that a rollback recomputes them
// correctly instead of restoring a stale flag.
//
// Step order is fixed and documented in ARCHITECTURE.md section 5. Changing it
// is a behavior change: re-record the affected replays in the same commit.
void advance_frame(GameState& state, InputPair current, InputPair previous);

}  // namespace mw::sim
