// Global simulation constants. Values come from DESIGN.md 4.4 and are
// deliberately duplicated nowhere else.
//
// Per-character values (walk speeds, move frame data) do NOT belong here —
// those live in data/characters/*.toml, per ADR 0009, so that balance changes
// are data commits rather than code commits. This file holds only what is
// global to every match.
//
// DESIGN.md 4.4 calls these "starting points to tune, not constants". Changing
// one is a behavior change: re-record the affected replays in the same commit.
#pragma once

#include <cstdint>

#include "sim/fixed.h"

namespace mw::sim {

// The simulation runs at a fixed 60Hz and has no dt. Every duration in the
// game is an integer count of these frames. ADR 0002.
inline constexpr int32_t FRAME_RATE = 60;

// Logical resolution, integer-scaled to the window by the render layer.
inline constexpr int32_t SCREEN_WIDTH = 480;
inline constexpr int32_t SCREEN_HEIGHT = 270;

// The stage is wider than the screen; the camera tracks the fighters.
inline constexpr int32_t STAGE_WIDTH = 960;
inline constexpr int32_t GROUND_Y = 240;

// Fighters cannot walk into the wall. Half a pushbox width of margin keeps the
// body on screen rather than the origin point.
inline constexpr int32_t STAGE_LEFT_BOUND = 16;
inline constexpr int32_t STAGE_RIGHT_BOUND = STAGE_WIDTH - 16;

inline constexpr int32_t STARTING_HEALTH = 100;
inline constexpr int32_t ROUND_TIMER_FRAMES = 5400;  // 90 seconds
inline constexpr int32_t ROUNDS_TO_WIN = 2;

// Where the two fighters stand when a round begins, measured from stage left.
inline constexpr int32_t ROUND_START_X_P1 = 380;
inline constexpr int32_t ROUND_START_X_P2 = 580;

// DESIGN.md 4.4 gives these as 1.2 and 1.0 units per frame. They are written
// as ratios rather than decimals because a float literal below the sim
// boundary is a desync (ADR 0002), and from_ratio is exact and constexpr.
//
// These are placeholders until the character TOML loader lands; at that point
// they move into data/characters/*.toml and this pair is deleted.
inline constexpr Fixed WALK_FORWARD_SPEED = Fixed::from_ratio(12, 10);
inline constexpr Fixed WALK_BACKWARD_SPEED = Fixed::from_ratio(10, 10);

// Capacity, not a target. Fixed-size because GameState must stay trivially
// copyable and small enough that rollback's memcpy is free (ADR 0005).
inline constexpr int32_t MAX_PROJECTILES = 8;

}  // namespace mw::sim
