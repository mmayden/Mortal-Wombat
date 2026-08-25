// Divided States — entry point and the fixed-timestep loop.
//
// This file wires platform, sim, and render together and owns the loop. It is
// the only place all three meet.
//
// The loop shape is the one ARCHITECTURE.md section 4 specifies, and it is not
// a stylistic choice: the simulation must advance in whole 60Hz steps with no
// dt, because that is what makes a match reproducible from a seed plus an input
// stream. Rendering interpolates between the two most recent steps so that a
// 60Hz simulation still looks smooth on a display running at some other rate.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include "platform/platform.h"
#include "platform/session_recorder.h"
#include "render/camera.h"
#include "render/renderer.h"
#include "render/sprite.h"
#include "sim/constants.h"
#include "sim/sim.h"
#include "sim/state.h"

#include "data/framedata_loader.h"
#include "ds_log.h"

namespace {

constexpr uint64_t NS_PER_SECOND = 1000000000ull;
constexpr uint64_t FRAME_NS = NS_PER_SECOND / static_cast<uint64_t>(ds::sim::FRAME_RATE);

// If the process stalls — a breakpoint, a window drag, the machine sleeping —
// the accumulator would otherwise hold seconds of owed simulation and try to
// catch up in one go, which takes longer than real time and never recovers.
// This caps the debt at 5 frames and drops the rest.
//
// Dropped frames are a rendering concession, not a simulation one: each step
// that does run is still exactly 1/60s of game time, so the match stays
// reproducible. It just briefly runs in slow motion rather than freezing.
constexpr uint64_t MAX_ACCUMULATED_NS = FRAME_NS * 5;

// The match seed. Fixed for now: a local match does not need entropy, and a
// constant makes a bug reproducible by relaunching rather than only by
// recording. Netplay and daily-seed modes are where this starts coming from
// somewhere else — and that somewhere is always outside the sim (ADR 0002).
constexpr uint64_t DEFAULT_SEED = 20260822u;

// Command line, kept deliberately tiny.
//
// --frames N gives CI a way to boot the *whole binary* rather than only the
// sim. BLUEPRINT.md 1.3 asks the smoke test to boot the game headless and run
// N ticks; until this existed, the smoke tier covered the simulation and left
// the window, renderer, and loop completely untested. Paired with SDL's dummy
// video driver it runs on a machine with no display.
// Prints each player's decoded input whenever it changes.
//
// Exists because the gamepad path could be verified only by someone holding a
// controller: the code compiled, CI passed, and a pad assigned to both players
// at once still looked correct from here. A button that lands on the wrong
// action is invisible to every test in the suite.
//
// DESIGN.md 6 puts an input display in training mode for v1. This is the
// console-shaped ancestor of it.
// Names the buttons held in one frame. Empty string when nothing is held.
std::string decode_input(ds::sim::InputFrame frame) {
    struct Named {
        ds::sim::Button button;
        const char* name;
    };
    constexpr Named NAMES[] = {
        {ds::sim::Button::Up, "Up"},       {ds::sim::Button::Down, "Down"},
        {ds::sim::Button::Left, "Left"},   {ds::sim::Button::Right, "Right"},
        {ds::sim::Button::LowPunch, "LP"}, {ds::sim::Button::HighPunch, "HP"},
        {ds::sim::Button::LowKick, "LK"},  {ds::sim::Button::HighKick, "HK"},
        {ds::sim::Button::Block, "BLOCK"},
    };

    std::string held;
    for (const Named& named : NAMES) {
        if (ds::sim::input_held(frame, named.button)) {
            if (!held.empty()) {
                held += " + ";
            }
            held += named.name;
        }
    }
    return held;
}

// Prints each player's input whenever it changes, tagged with the device that
// produced it.
//
// The tag is the entire point. An input arriving on the wrong player looks the
// same from the sim's side whether the game mis-assigned it or the operating
// system is feeding a controller's d-pad to the keyboard as arrow keys -- and
// those two need opposite fixes. Printing pad and keyboard separately answers
// it in one keypress.
//
// DESIGN.md 6 puts an input display in training mode for v1. This is its
// console-shaped ancestor.
void print_input(int32_t frame, const ds::platform::Platform& platform,
                 const ds::sim::GameState& state) {
    const char* STATE_NAMES[] = {"RoundStart", "Idle",      "WalkFwd",  "WalkBack",
                                 "Crouch",     "JumpStart", "Airborne", "Landing",
                                 "Attack",     "Blocking",  "Hitstun",  "Blockstun",
                                 "Knockdown",  "Wakeup",    "Win",      "Lose"};

    for (int32_t player = 0; player < 2; ++player) {
        const std::string pad = decode_input(platform.pad_input.players[player]);
        const std::string keys = decode_input(platform.keyboard_input.players[player]);
        const ds::sim::Fighter& fighter = state.fighters[player];

        // Position and state are printed alongside the input because "both
        // characters moved" has two very different causes that look the same
        // from the outside: input reaching the wrong player, or pushboxes
        // separating two fighters who are touching. A player whose x changes
        // while its own input line is empty is being pushed, not driven.
        DS_LOG_INFO("f%-6d P%d  pad[%-22s] keys[%-22s] x=%-5d %s", frame, player + 1, pad.c_str(),
                    keys.c_str(), fighter.x.to_int(),
                    STATE_NAMES[static_cast<int32_t>(fighter.state)]);
    }
    DS_LOG_INFO("        gap between fighters: %d units",
                state.fighters[1].x.to_int() - state.fighters[0].x.to_int());
}

struct Options {
    // Counted in SIMULATION frames, not render frames. The loop is uncapped
    // (no vsync -- the fixed timestep owns pacing), so render frames outnumber
    // sim frames by a large and machine-dependent factor. Counting renders
    // would make "--frames 600" mean a different amount of game time on every
    // machine, which is exactly the property this project does not want.
    //
    // 600 sim frames is 10 seconds of game time, matching BLUEPRINT.md 1.3.
    int32_t frames = -1;  // -1 means run until the player quits
    const char* screenshot = nullptr;

    // Print each player's decoded input whenever it changes. The only way to
    // verify a controller is wired to the actions its label claims.
    bool input_test = false;

    // Write this session to a replay file on quit. The point is to make a
    // report like "the other character slid toward me" reproducible instead of
    // a thing that has to be guessed at from a description.
    const char* record = nullptr;
};

Options parse_options(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            options.frames = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            options.screenshot = argv[++i];
        } else if (std::strcmp(argv[i], "--input-test") == 0) {
            options.input_test = true;
        } else if (std::strcmp(argv[i], "--record") == 0 && i + 1 < argc) {
            options.record = argv[++i];
        } else {
            DS_LOG_WARN("ignoring unrecognized argument: %s", argv[i]);
        }
    }
    return options;
}

}  // namespace

int main(int argc, char** argv) {
    const Options options = parse_options(argc, argv);

    ds::platform::Platform platform{};
    if (!ds::platform::init(platform, "Divided States")) {
        return 1;
    }

    // Frame data first: a bad character file should fail before a window
    // appears, not after. Everything the sim needs is validated here, at load
    // time, which is what lets advance_frame have no error path at all
    // (CONVENTIONS.md 3).
    ds::sim::MatchData match_data{};
    {
        const std::string data_dir = std::string(DS_DATA_DIR) + "/characters/";
        std::string error;
        const ds::data::LoadResult result = ds::data::load_match(
            data_dir + "george.toml", data_dir + "sue.toml", match_data, error);
        if (result != ds::data::LoadResult::Ok) {
            DS_LOG_ERROR("could not load frame data (%s): %s", ds::data::load_result_name(result),
                         error.c_str());
            ds::platform::shutdown(platform);
            return 1;
        }
        DS_LOG_INFO("loaded %s vs %s", match_data.characters[0].display_name,
                    match_data.characters[1].display_name);
    }

    const ds::render::PlaceholderManifest manifest;

    // Render-layer state: the view is not part of GameState and is never
    // rolled back (ARCHITECTURE.md 3).
    ds::render::Camera camera{};

    ds::sim::GameState state{};
    ds::sim::init_state(state, DEFAULT_SEED);

    // The state one simulation step behind, kept solely so the renderer can
    // interpolate. It is a copy, never a reference: the sim owns `state` and
    // rewrites it in place.
    ds::sim::GameState previous_state = state;

    ds::sim::InputPair current_input{};
    ds::sim::InputPair previous_input{};

    // Heap rather than a local: the recorder holds fixed-size arrays for ten
    // minutes of play, which is far too large for the stack. This is the render
    // side of the boundary, so an allocation here is fine -- it would not be
    // twenty lines further down.
    ds::platform::SessionRecorder* recorder = nullptr;
    if (options.record != nullptr) {
        recorder = new ds::platform::SessionRecorder();
        ds::platform::recorder_begin(*recorder, DEFAULT_SEED);
        DS_LOG_INFO("recording this session to %s", options.record);
    }

    DS_LOG_INFO("running at a fixed %d Hz; ESC quits, F1 toggles debug", ds::sim::FRAME_RATE);

    uint64_t last_time = ds::platform::now_ns();
    uint64_t accumulator = 0;
    int32_t frames_rendered = 0;

    while (!platform.should_quit) {
        const uint64_t now = ds::platform::now_ns();
        uint64_t elapsed = now - last_time;
        last_time = now;

        if (elapsed > MAX_ACCUMULATED_NS) {
            elapsed = MAX_ACCUMULATED_NS;
        }
        accumulator += elapsed;

        ds::platform::poll(platform);

        // Advance in whole frames. Input is sampled per step rather than per
        // render frame, so that a slow render frame that owes two simulation
        // steps still feeds each of them an input — the sim's view of time is
        // never anything but a sequence of 60Hz frames.
        while (accumulator >= FRAME_NS) {
            previous_state = state;
            previous_input = current_input;
            current_input = ds::platform::current_input(platform);

            if (options.input_test &&
                (current_input.players[0].buttons != previous_input.players[0].buttons ||
                 current_input.players[1].buttons != previous_input.players[1].buttons)) {
                print_input(state.frame, platform, state);
            }

            ds::sim::advance_frame(state, match_data, current_input, previous_input);

            if (recorder != nullptr) {
                ds::platform::recorder_frame(*recorder, current_input, state);
            }

            accumulator -= FRAME_NS;
        }

        // How far between the last two simulation steps we are, in [0, 1).
        // This is the only place a fractional notion of time exists, and it
        // never crosses the boundary (ARCHITECTURE.md 4).
        const float alpha = static_cast<float>(accumulator) / static_cast<float>(FRAME_NS);

        ds::render::camera_update(camera, previous_state, state, alpha);
        ds::render::draw_frame(platform.renderer, manifest, match_data, camera, previous_state,
                               state, alpha, platform.show_debug);
        ds::platform::present(platform);
        ++frames_rendered;

        if (options.frames >= 0 && state.frame >= options.frames) {
            platform.should_quit = true;
        }
    }

    if (options.screenshot != nullptr) {
        // Redraw before capturing. SDL_RenderReadPixels reads the back buffer,
        // whose contents are undefined after SDL_RenderPresent -- so capturing
        // straight after the loop returned the PREVIOUS frame. That is a
        // particularly bad failure for a debugging tool: the image looked
        // plausible and was one frame stale, which is exactly enough to make a
        // hitbox look inactive on the frame it connected.
        ds::render::camera_update(camera, previous_state, state, 0.0f);
        ds::render::draw_frame(platform.renderer, manifest, match_data, camera, previous_state,
                               state, 0.0f, platform.show_debug);

        if (!ds::platform::save_screenshot(platform, options.screenshot)) {
            ds::platform::shutdown(platform);
            return 1;
        }
        DS_LOG_INFO("wrote %s", options.screenshot);
    }

    DS_LOG_INFO("ran %d render frames, %d simulation frames", frames_rendered, state.frame);

    // Written after the screenshot path, so --record and --screenshot compose.
    if (recorder != nullptr) {
        ds::platform::recorder_write(*recorder, options.record, state);
        delete recorder;
        recorder = nullptr;
    }

    ds::platform::shutdown(platform);
    return 0;
}
