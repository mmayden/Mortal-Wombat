// Draws GameState. Never writes it.
//
// ARCHITECTURE.md 1: the dependency direction is one-way. This reads sim state
// and nothing in src/sim/ knows this file exists. There is no callback from the
// sim into here — not for sound, not for effects. Render learns that something
// happened by comparing the state it drew last frame against the state it is
// drawing now.
//
// Everything here is above the sim boundary, so floats, allocation, and
// std::string are all fine.
#pragma once

#include "render/sprite.h"
#include "sim/state.h"

struct SDL_Renderer;

namespace mw::render {

// Interpolates between two sim frames and draws the result.
//
// `previous` and `current` are consecutive sim states; `alpha` is the
// fractional position between them, from the leftover in the fixed-timestep
// accumulator. Interpolating is what lets a 60Hz simulation look smooth on a
// 144Hz display without the simulation knowing the display exists.
//
// `alpha` appears in this signature and nowhere below the boundary
// (ARCHITECTURE.md 4). The sim has no dt.
void draw_frame(SDL_Renderer* renderer, const SpriteManifest& manifest,
                const mw::sim::GameState& previous, const mw::sim::GameState& current,
                float alpha, bool show_debug);

}  // namespace mw::render
