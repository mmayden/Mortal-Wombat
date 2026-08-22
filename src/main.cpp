// Mortal Wombat — entry point and the fixed-timestep loop.
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
#include "render/renderer.h"
#include "render/sprite.h"
#include "sim/constants.h"
#include "sim/sim.h"
#include "sim/state.h"

#include "data/framedata_loader.h"
#include "log.h"

namespace {

constexpr uint64_t NS_PER_SECOND = 1000000000ull;
constexpr uint64_t FRAME_NS = NS_PER_SECOND / static_cast<uint64_t>(mw::sim::FRAME_RATE);

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
};

Options parse_options(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            options.frames = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            options.screenshot = argv[++i];
        } else {
            MW_LOG_WARN("ignoring unrecognized argument: %s", argv[i]);
        }
    }
    return options;
}

}  // namespace

int main(int argc, char** argv) {
    const Options options = parse_options(argc, argv);

    mw::platform::Platform platform{};
    if (!mw::platform::init(platform, "Mortal Wombat")) {
        return 1;
    }

    // Frame data first: a bad character file should fail before a window
    // appears, not after. Everything the sim needs is validated here, at load
    // time, which is what lets advance_frame have no error path at all
    // (CONVENTIONS.md 3).
    mw::sim::MatchData match_data{};
    {
        const std::string data_dir = std::string(MW_DATA_DIR) + "/characters/";
        std::string error;
        const mw::data::LoadResult result = mw::data::load_match(
            data_dir + "frenchy.toml", data_dir + "wisdom.toml", match_data, error);
        if (result != mw::data::LoadResult::Ok) {
            MW_LOG_ERROR("could not load frame data (%s): %s", mw::data::load_result_name(result),
                         error.c_str());
            mw::platform::shutdown(platform);
            return 1;
        }
        MW_LOG_INFO("loaded %s vs %s", match_data.characters[0].display_name,
                    match_data.characters[1].display_name);
    }

    const mw::render::PlaceholderManifest manifest;

    mw::sim::GameState state{};
    mw::sim::init_state(state, DEFAULT_SEED);

    // The state one simulation step behind, kept solely so the renderer can
    // interpolate. It is a copy, never a reference: the sim owns `state` and
    // rewrites it in place.
    mw::sim::GameState previous_state = state;

    mw::sim::InputPair current_input{};
    mw::sim::InputPair previous_input{};

    MW_LOG_INFO("running at a fixed %d Hz; ESC quits, F1 toggles debug", mw::sim::FRAME_RATE);

    uint64_t last_time = mw::platform::now_ns();
    uint64_t accumulator = 0;
    int32_t frames_rendered = 0;

    while (!platform.should_quit) {
        const uint64_t now = mw::platform::now_ns();
        uint64_t elapsed = now - last_time;
        last_time = now;

        if (elapsed > MAX_ACCUMULATED_NS) {
            elapsed = MAX_ACCUMULATED_NS;
        }
        accumulator += elapsed;

        mw::platform::poll(platform);

        // Advance in whole frames. Input is sampled per step rather than per
        // render frame, so that a slow render frame that owes two simulation
        // steps still feeds each of them an input — the sim's view of time is
        // never anything but a sequence of 60Hz frames.
        while (accumulator >= FRAME_NS) {
            previous_state = state;
            previous_input = current_input;
            current_input = mw::platform::current_input(platform);

            mw::sim::advance_frame(state, current_input, previous_input);

            accumulator -= FRAME_NS;
        }

        // How far between the last two simulation steps we are, in [0, 1).
        // This is the only place a fractional notion of time exists, and it
        // never crosses the boundary (ARCHITECTURE.md 4).
        const float alpha = static_cast<float>(accumulator) / static_cast<float>(FRAME_NS);

        mw::render::draw_frame(platform.renderer, manifest, previous_state, state, alpha,
                               platform.show_debug);
        mw::platform::present(platform);
        ++frames_rendered;

        if (options.frames >= 0 && state.frame >= options.frames) {
            platform.should_quit = true;
        }
    }

    // Captured after the loop so the screenshot shows the final frame, which is
    // the one --frames was asked to reach.
    if (options.screenshot != nullptr) {
        if (!mw::platform::save_screenshot(platform, options.screenshot)) {
            mw::platform::shutdown(platform);
            return 1;
        }
        MW_LOG_INFO("wrote %s", options.screenshot);
    }

    MW_LOG_INFO("ran %d render frames, %d simulation frames", frames_rendered, state.frame);

    mw::platform::shutdown(platform);
    return 0;
}
