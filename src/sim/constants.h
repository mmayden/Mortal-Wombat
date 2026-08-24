// Global simulation constants.
//
// This file has two halves and the split matters.
//
// The first half is transcribed from DESIGN.md 4.4. Each value cites its
// source and must not be changed here without changing the design doc.
//
// The second half is PROVISIONAL: values the design docs do not specify, which
// the engine needed in order to run at all. They are engineering placeholders,
// not design decisions, and DESIGN.md 10 is explicit that undecided things must
// not be invented around. They are isolated here so that nobody mistakes one
// for a settled number, and so that the list of what still needs deciding is
// exactly this block rather than a grep through the sim. See ADR 0015.
//
// Per-character values (walk speeds once characters exist, move frame data, box
// dimensions) belong in neither half — those live in data/characters/*.toml per
// ADR 0009, so that balance changes are data commits rather than code commits.
#pragma once

#include <cstdint>

#include "sim/fixed.h"

namespace mw::sim {

// ---------------------------------------------------------------------------
// From DESIGN.md 4.4
//
// That section calls these "starting points to tune, not constants" — expect
// them to change. When one does, re-record the affected replays in the same
// commit (AGENTS.md rule 8).
// ---------------------------------------------------------------------------

// The simulation runs at a fixed 60Hz and has no dt. Every duration in the
// game is an integer count of these frames. ADR 0002.
inline constexpr int32_t FRAME_RATE = 60;

// Logical resolution, integer-scaled to the window by the render layer.
inline constexpr int32_t SCREEN_WIDTH = 480;
inline constexpr int32_t SCREEN_HEIGHT = 270;

// The stage is wider than the screen; the camera tracks the fighters.
inline constexpr int32_t STAGE_WIDTH = 960;
inline constexpr int32_t GROUND_Y = 240;

inline constexpr int32_t STARTING_HEALTH = 100;
inline constexpr int32_t ROUND_TIMER_FRAMES = 5400;  // 90 seconds
inline constexpr int32_t ROUNDS_TO_WIN = 2;

// DESIGN.md 4.4 gives these as 1.2 and 1.0 units per frame. They are written as
// ratios rather than decimals because a float literal below the sim boundary is
// a desync (ADR 0002), and from_ratio is exact and constexpr.
//
// These are global only until the character loader lands; ADR 0009 puts walk
// speed in each character's TOML, at which point this pair is deleted.
inline constexpr Fixed WALK_FORWARD_SPEED = Fixed::from_ratio(12, 10);
inline constexpr Fixed WALK_BACKWARD_SPEED = Fixed::from_ratio(10, 10);

// ---------------------------------------------------------------------------
// PROVISIONAL — not specified by any design document
//
// Every value below was chosen to make the engine runnable, not because a
// design decision produced it. Each is already baked into the committed replay
// checkpoint hashes, so changing one means re-recording — which is the intended
// workflow, not a problem.
//
// Before v1 feel work begins, each of these needs a real answer.
// ---------------------------------------------------------------------------

// How far apart the fighters stand at the opening of a round, centered on the
// stage. Derived from STAGE_WIDTH rather than written as two magic positions,
// so that the single invented number is the separation itself.
//
// NEEDS A DECISION: this is a feel value. It sets how long the opening approach
// takes, which is the first thing a player experiences every round. At the walk
// speeds above, 200 units is roughly 83 frames of forward walk to close.
inline constexpr int32_t ROUND_START_SEPARATION = 200;

inline constexpr int32_t ROUND_START_X_P1 = (STAGE_WIDTH - ROUND_START_SEPARATION) / 2;
inline constexpr int32_t ROUND_START_X_P2 = (STAGE_WIDTH + ROUND_START_SEPARATION) / 2;

// How close a fighter's origin may come to each stage edge.
//
// NEEDS A DECISION: this should not be a constant at all. The correct bound
// derives from the fighter's pushbox half-width, which is per-character data
// (docs/framedata_schema.md, character.boxes.pushbox) and is not loaded yet.
// When the character loader lands, delete this and compute it per fighter.
inline constexpr int32_t STAGE_EDGE_MARGIN = 16;

inline constexpr int32_t STAGE_LEFT_BOUND = STAGE_EDGE_MARGIN;
inline constexpr int32_t STAGE_RIGHT_BOUND = STAGE_WIDTH - STAGE_EDGE_MARGIN;

// The pre-round freeze, and how long the KO pose holds before the next round.
//
// NEEDS A DECISION: both are pure pacing. DESIGN.md 2 specifies that rounds
// exist and how they are won, but says nothing about their rhythm. 90 and 120
// frames are 1.5 and 2 seconds.
inline constexpr int32_t ROUND_START_FREEZE_FRAMES = 90;
inline constexpr int32_t ROUND_END_FREEZE_FRAMES = 120;

// Frames between pressing up and leaving the ground.
//
// NEEDS A DECISION: DESIGN.md 4.4 gives "jump duration 44 frames total" without
// saying whether that includes a startup, and 4.3 does not mention one. Three
// frames is a commitment window -- long enough that a jump is a decision an
// opponent can react to, which is what DESIGN.md 3 asks for, and short enough
// not to feel unresponsive. The 44 frames are counted as airborne time, so a
// jump costs 3 + 44 + 4 frames end to end.
inline constexpr int32_t JUMP_STARTUP_FRAMES = 3;

// Recovery on touching down, during which the fighter cannot act.
//
// This one IS specified: DESIGN.md 4.5 gives the jump attack a recovery of
// "4 (landing)". Applying the same landing recovery to every jump keeps one
// rule rather than making an empty jump cheaper than an attacking one.
inline constexpr int32_t LANDING_FRAMES = 4;

// Horizontal speed of a forward or backward jump, as a multiple of the
// character's walk speed in that direction.
//
// NEEDS A DECISION: unspecified anywhere. Jumping at exactly walk speed makes a
// forward jump strictly worse than walking -- same distance, no ability to
// block, and a landing recovery -- so it has to travel further to be a choice
// at all. This is pure feel and cannot be judged until someone plays it.
inline constexpr Fixed JUMP_HORIZONTAL_SCALE = Fixed::from_ratio(9, 5);  // 1.8x

// Capacity, not a target — a bound chosen so that GameState stays trivially
// copyable and small enough that rollback's memcpy is free (ADR 0005).
//
// NEEDS A DECISION: no move currently spawns a projectile, and whether any
// will is open -- it depends on the specials, which are undecided. If none
// do, this array and the Projectile type come out of GameState entirely,
// which would shrink what rollback copies every frame.
inline constexpr int32_t MAX_PROJECTILES = 8;

}  // namespace mw::sim
