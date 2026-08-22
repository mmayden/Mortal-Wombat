#include "platform/platform.h"

#include <SDL3/SDL.h>

#include "sim/constants.h"

#include "mw_log.h"

namespace mw::platform {
namespace {

using mw::sim::Button;
using mw::sim::InputFrame;

// Keyboard bindings for local versus. DESIGN.md 6 puts two gamepads in the v1
// definition of done; keyboard comes first because it is what makes this
// demonstrable today, and gamepad support is additive here rather than a
// rewrite — SDL3 reports both through the same InputFrame.
//
// Five buttons plus four directions each (DESIGN.md 4.1). Block is a button,
// not hold-back, so nothing here has to disambiguate walking away from
// defending.
struct Binding {
    SDL_Scancode scancode;
    Button button;
};

constexpr Binding P1_BINDINGS[] = {
    {SDL_SCANCODE_W, Button::Up},       {SDL_SCANCODE_S, Button::Down},
    {SDL_SCANCODE_A, Button::Left},     {SDL_SCANCODE_D, Button::Right},
    {SDL_SCANCODE_F, Button::LowPunch}, {SDL_SCANCODE_G, Button::HighPunch},
    {SDL_SCANCODE_C, Button::LowKick},  {SDL_SCANCODE_V, Button::HighKick},
    {SDL_SCANCODE_B, Button::Block},
};

constexpr Binding P2_BINDINGS[] = {
    {SDL_SCANCODE_UP, Button::Up},         {SDL_SCANCODE_DOWN, Button::Down},
    {SDL_SCANCODE_LEFT, Button::Left},     {SDL_SCANCODE_RIGHT, Button::Right},
    {SDL_SCANCODE_KP_4, Button::LowPunch}, {SDL_SCANCODE_KP_5, Button::HighPunch},
    {SDL_SCANCODE_KP_1, Button::LowKick},  {SDL_SCANCODE_KP_2, Button::HighKick},
    {SDL_SCANCODE_KP_0, Button::Block},
};

// Gamepad bindings. DESIGN.md 6 puts local versus on two gamepads in the v1
// definition of done, and a fighting game is judged on inputs feeling direct.
//
// Face buttons follow the fighting-game convention rather than the console one:
// the punches sit on the left pair and the kicks on the right, so a player who
// has touched an arcade stick finds them where they expect. Block goes on a
// shoulder because it is held, and a held face button fights the punch buttons
// for thumb position.
struct GamepadBinding {
    SDL_GamepadButton button;
    Button action;
};

constexpr GamepadBinding GAMEPAD_BINDINGS[] = {
    {SDL_GAMEPAD_BUTTON_DPAD_UP, Button::Up},
    {SDL_GAMEPAD_BUTTON_DPAD_DOWN, Button::Down},
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, Button::Left},
    {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, Button::Right},
    {SDL_GAMEPAD_BUTTON_WEST, Button::LowPunch},    // X on Xbox layout
    {SDL_GAMEPAD_BUTTON_NORTH, Button::HighPunch},  // Y
    {SDL_GAMEPAD_BUTTON_SOUTH, Button::LowKick},    // A
    {SDL_GAMEPAD_BUTTON_EAST, Button::HighKick},    // B
    {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, Button::Block},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, Button::Block},
};

// Past this, a stick counts as pressed. SDL reports axes over the full int16
// range; roughly half deflection avoids a worn stick drifting into a walk while
// staying well short of requiring a full push.
//
// The stick is quantised to the same four directions as the d-pad rather than
// producing an analogue value. DESIGN.md 4.3 has fixed jump arcs and no run
// button, so there is nothing an analogue magnitude could mean -- and the sim
// takes a bitfield (ADR 0002), which has nowhere to put one.
constexpr int16_t STICK_THRESHOLD = 16000;

InputFrame read_gamepad(SDL_Gamepad* pad) {
    InputFrame frame{0u};
    if (pad == nullptr) {
        return frame;
    }

    for (const GamepadBinding& binding : GAMEPAD_BINDINGS) {
        if (SDL_GetGamepadButton(pad, binding.button)) {
            frame = input_with(frame, binding.action);
        }
    }

    const int16_t x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
    const int16_t y = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);

    if (x <= -STICK_THRESHOLD) {
        frame = input_with(frame, Button::Left);
    } else if (x >= STICK_THRESHOLD) {
        frame = input_with(frame, Button::Right);
    }
    if (y <= -STICK_THRESHOLD) {
        frame = input_with(frame, Button::Up);
    } else if (y >= STICK_THRESHOLD) {
        frame = input_with(frame, Button::Down);
    }

    // Triggers as an alternative block, for pads whose shoulders are stiff.
    if (SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) >= STICK_THRESHOLD ||
        SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) >= STICK_THRESHOLD) {
        frame = input_with(frame, Button::Block);
    }

    return frame;
}

// Assigns a newly connected pad to the first free player slot.
//
// Connection order, not device index: SDL device ids are not stable across
// replugs, and "the first pad plugged in is player one" is a rule a person can
// verify by looking at the couch.
void attach_gamepad(Platform& platform, SDL_JoystickID id) {
    for (int32_t i = 0; i < MAX_GAMEPADS; ++i) {
        if (platform.gamepads[i] != nullptr) {
            continue;
        }
        SDL_Gamepad* pad = SDL_OpenGamepad(id);
        if (pad == nullptr) {
            MW_LOG_WARN("could not open gamepad %u: %s", static_cast<unsigned>(id), SDL_GetError());
            return;
        }
        platform.gamepads[i] = pad;
        MW_LOG_INFO("player %d: %s", i + 1, SDL_GetGamepadName(pad));
        return;
    }
    MW_LOG_INFO("ignoring a third gamepad -- this is a two-player game");
}

void detach_gamepad(Platform& platform, SDL_JoystickID id) {
    for (int32_t i = 0; i < MAX_GAMEPADS; ++i) {
        SDL_Gamepad* pad = static_cast<SDL_Gamepad*>(platform.gamepads[i]);
        if (pad == nullptr || SDL_GetGamepadID(pad) != id) {
            continue;
        }
        SDL_CloseGamepad(pad);
        platform.gamepads[i] = nullptr;

        // Falling back to the keyboard rather than freezing that player: a pad
        // yanked mid-match should not make the game unplayable, and the sim
        // cannot tell the difference anyway -- it sees the same bitfield.
        MW_LOG_INFO("player %d gamepad disconnected, falling back to keyboard", i + 1);
        return;
    }
}

InputFrame read_bindings(const bool* keys, const Binding* bindings, int32_t count) {
    InputFrame frame{0u};
    for (int32_t i = 0; i < count; ++i) {
        if (keys[bindings[i].scancode]) {
            frame = input_with(frame, bindings[i].button);
        }
    }
    return frame;
}

}  // namespace

bool init(Platform& platform, const char* title) {
    platform = Platform{};

    // Video and gamepad. Audio is deliberately not initialized yet: ADR 0012
    // puts playback in miniaudio in the render layer, and there is nothing to
    // play.
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        MW_LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    // Open at 3x the logical resolution so the window is a usable size on a
    // modern display while staying an exact integer multiple — DESIGN.md 4.4
    // specifies 480x270 integer-scaled, and a non-integer scale would blur
    // every edge of a game made of hard-edged boxes.
    constexpr int SCALE = 3;
    const int window_w = mw::sim::SCREEN_WIDTH * SCALE;
    const int window_h = mw::sim::SCREEN_HEIGHT * SCALE;

    if (!SDL_CreateWindowAndRenderer(title, window_w, window_h, SDL_WINDOW_RESIZABLE,
                                     &platform.window, &platform.renderer)) {
        MW_LOG_ERROR("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    // Everything draws in 480x270 logical space and SDL scales it up, letterbox
    // included. This is what keeps the renderer free of any knowledge of the
    // window size.
    //
    // INTEGER_SCALE rather than LETTERBOX so that a resized window never
    // produces half-pixel edges.
    SDL_SetRenderLogicalPresentation(platform.renderer, mw::sim::SCREEN_WIDTH,
                                     mw::sim::SCREEN_HEIGHT,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

    // No vsync wait: the fixed-timestep loop in main owns pacing, and vsync
    // would add a variable stall the accumulator then has to absorb. ADR 0003
    // chose SDL specifically for this kind of control — a fighting game is
    // judged on latency.
    SDL_SetRenderVSync(platform.renderer, 0);

    // Pads already plugged in at launch do not generate ADDED events, so they
    // have to be picked up explicitly.
    int gamepad_count = 0;
    if (SDL_JoystickID* ids = SDL_GetGamepads(&gamepad_count)) {
        for (int i = 0; i < gamepad_count; ++i) {
            attach_gamepad(platform, ids[i]);
        }
        SDL_free(ids);
    }
    if (gamepad_count == 0) {
        MW_LOG_INFO("no gamepads detected; both players on the keyboard");
    }

    MW_LOG_INFO("SDL3 %d.%d.%d, renderer: %s", SDL_MAJOR_VERSION, SDL_MINOR_VERSION,
                SDL_MICRO_VERSION, SDL_GetRendererName(platform.renderer));
    return true;
}

void shutdown(Platform& platform) {
    for (int32_t i = 0; i < MAX_GAMEPADS; ++i) {
        if (platform.gamepads[i] != nullptr) {
            SDL_CloseGamepad(static_cast<SDL_Gamepad*>(platform.gamepads[i]));
            platform.gamepads[i] = nullptr;
        }
    }
    if (platform.renderer != nullptr) {
        SDL_DestroyRenderer(platform.renderer);
        platform.renderer = nullptr;
    }
    if (platform.window != nullptr) {
        SDL_DestroyWindow(platform.window);
        platform.window = nullptr;
    }
    SDL_Quit();
}

void poll(Platform& platform) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                platform.should_quit = true;
                break;

            case SDL_EVENT_GAMEPAD_ADDED:
                attach_gamepad(platform, event.gdevice.which);
                break;

            case SDL_EVENT_GAMEPAD_REMOVED:
                detach_gamepad(platform, event.gdevice.which);
                break;

            case SDL_EVENT_KEY_DOWN:
                if (event.key.repeat) {
                    break;
                }
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    platform.should_quit = true;
                } else if (event.key.scancode == SDL_SCANCODE_F1) {
                    platform.show_debug = !platform.show_debug;
                }
                break;

            default:
                break;
        }
    }

    // Read the keyboard as *state*, not from the events above. Edge detection
    // is the sim's job, computed from two consecutive InputFrames — deriving it
    // there rather than storing it here is what lets a rollback recompute
    // button edges correctly instead of restoring a stale flag.
    const bool* keys = SDL_GetKeyboardState(nullptr);
    platform.input.players[0] =
        read_bindings(keys, P1_BINDINGS, static_cast<int32_t>(SDL_arraysize(P1_BINDINGS)));
    platform.input.players[1] =
        read_bindings(keys, P2_BINDINGS, static_cast<int32_t>(SDL_arraysize(P2_BINDINGS)));

    // Pad input is OR-ed with the keyboard rather than replacing it, so one
    // player can be on a pad and the other on keys without any mode to select.
    // The sim cannot tell the difference: both produce the same bitfield, which
    // is the point of input-as-data (ADR 0002).
    for (int32_t i = 0; i < MAX_GAMEPADS; ++i) {
        const InputFrame pad = read_gamepad(static_cast<SDL_Gamepad*>(platform.gamepads[i]));
        platform.input.players[i] =
            InputFrame{static_cast<uint16_t>(platform.input.players[i].buttons | pad.buttons)};
    }
}

void present(Platform& platform) {
    SDL_RenderPresent(platform.renderer);
}

bool save_screenshot(Platform& platform, const char* path) {
    SDL_Surface* surface = SDL_RenderReadPixels(platform.renderer, nullptr);
    if (surface == nullptr) {
        MW_LOG_ERROR("SDL_RenderReadPixels failed: %s", SDL_GetError());
        return false;
    }

    const bool ok = SDL_SaveBMP(surface, path);
    if (!ok) {
        MW_LOG_ERROR("SDL_SaveBMP failed: %s", SDL_GetError());
    }
    SDL_DestroySurface(surface);
    return ok;
}

mw::sim::InputPair current_input(const Platform& platform) {
    mw::sim::InputPair sanitized{};
    sanitized.players[0] = input_sanitized(platform.input.players[0]);
    sanitized.players[1] = input_sanitized(platform.input.players[1]);
    return sanitized;
}

uint64_t now_ns() {
    return SDL_GetTicksNS();
}

}  // namespace mw::platform
