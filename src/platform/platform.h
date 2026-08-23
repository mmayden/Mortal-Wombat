// SDL3: window, renderer, event pump, and input translation. ADR 0003.
//
// This is the only file that knows SDL exists on the game side. Its job is to
// turn devices into `InputFrame` bitfields and hand them to the sim as data
// (ADR 0002) — the sim never polls anything.
#pragma once

#include <cstdint>

#include "sim/input.h"

struct SDL_Window;
struct SDL_Renderer;

namespace mw::platform {

// How many gamepads the game tracks. DESIGN.md 6 puts local versus on two
// gamepads in the v1 definition of done, and the game is two-player, so a third
// pad has nowhere to go.
inline constexpr int32_t MAX_GAMEPADS = 2;

struct Platform {
    SDL_Window* window;
    SDL_Renderer* renderer;

    // Open gamepads, indexed by player. Null means that player is on the
    // keyboard. Assigned in connection order: first pad plugged in is player
    // one. SDL_Gamepad is an opaque handle, so this is a void* to keep SDL out
    // of this header's interface.
    void* gamepads[MAX_GAMEPADS];

    bool should_quit;
    bool show_debug;

    // The live device state, rebuilt each poll. Not sanitized until it is
    // handed out — see current_input().
    mw::sim::InputPair input;

    // The same frame, split by where it came from. Kept separately so that
    // --input-test can say WHICH device produced an input rather than only
    // that one arrived: a controller whose d-pad is also emulating arrow keys
    // looks identical to a game bug from the sim's side, and the two need
    // opposite fixes.
    mw::sim::InputPair keyboard_input;
    mw::sim::InputPair pad_input;
};

bool init(Platform& platform, const char* title);
void shutdown(Platform& platform);

// Drains the SDL event queue and refreshes `platform.input`.
void poll(Platform& platform);

// Puts the finished frame on screen.
//
// Exists so that main.cpp never calls SDL directly -- this header is the
// game side's whole SDL surface, which is what keeps swapping the backend a
// change to two files rather than a search through the codebase.
void present(Platform& platform);

// Saves what was last drawn to a BMP. Used by --screenshot so that "does it
// render correctly" is answerable from a file rather than by scraping the
// screen, which is fragile and cannot run in CI.
//
// BMP because SDL writes it without SDL_image -- one fewer dependency for a
// debugging affordance.
bool save_screenshot(Platform& platform, const char* path);

// The input to feed the sim this frame, with reserved bits stripped.
//
// Sanitizing here rather than trusting the caller means a device or driver
// setting a bit we do not use cannot alter a state hash and read as a desync
// on a frame where nothing actually diverged.
mw::sim::InputPair current_input(const Platform& platform);

// Wall-clock nanoseconds, for the fixed-timestep accumulator in main.
//
// This is the only clock read in the whole program, and it lives above the sim
// boundary. Nothing in src/sim/ may call it (ADR 0002).
uint64_t now_ns();

}  // namespace mw::platform
