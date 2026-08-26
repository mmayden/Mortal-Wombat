// The training-mode readout: what each fighter is doing, in words, on screen.
//
// It exists because a playtester pressed all six attack buttons and reported
// back: "I dont know the differece in the inputs or attacks so I dont know
// whats happening. All the buttons do something though."
//
// Half of that was the renderer drawing an identical limb for every attack,
// which is fixed. The other half is that a fighting game is unreadable without
// numbers: startup, active and recovery are the entire vocabulary of the genre
// (docs/MECHANICS.md) and none of them were visible. Frame data you cannot see
// is frame data you cannot learn.
//
// Two of the three training-mode lines in DESIGN.md 6 live here -- the frame
// data readout and the input display. The hitbox overlay was already built.
//
// Bound to F1 alongside the boxes, so one key turns the whole diagnostic layer
// on and off.
#pragma once

#include <SDL3/SDL.h>

#include "render/input_history.h"
#include "sim/framedata.h"
#include "sim/input.h"
#include "sim/state.h"

namespace ds::render {

// Draws both fighters' state, current move and frame, and held buttons.
//
// `inputs` comes from the platform layer rather than GameState: what a player
// is holding is not part of the match state (ARCHITECTURE.md 3), and putting it
// there to make it drawable would be exactly the mistake the boundary exists to
// prevent.
void draw_readout(SDL_Renderer* renderer, const ds::sim::MatchData& data,
                  const ds::sim::GameState& state, ds::sim::InputPair inputs,
                  const InputHistory (&history)[2]);

}  // namespace ds::render
