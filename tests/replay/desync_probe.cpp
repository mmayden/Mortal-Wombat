// Emits per-frame state hashes for every committed recording.
//
// This is half of the desync test. The other half is CI: it runs this binary on
// Linux, Windows, and macOS runners and diffs the three outputs byte for byte.
// Identical output means the simulation is bit-identical across platforms;
// differing output names the exact frame where it stopped being.
//
// ADR 0011 calls this the most important test in the project. Cross-platform
// float divergence is the failure that ships broken, cannot be reproduced on
// the developer's machine, and cannot be debugged from a bug report. Catching
// it on the commit that introduces it is the only workable option.
//
// Output is deliberately plain text, one line per frame, so that a CI diff
// points at a frame number rather than reporting that two binaries differ.
#include <cstdio>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "replay/replay_file.h"
#include "replay/scenarios.h"
#include "sim/hash.h"
#include "sim/sim.h"

#include "match_data.h"

using namespace mw::test;

namespace {

std::string replay_path(const char* name) {
    return std::string(MW_REPLAY_DIR) + "/" + name + ".replay";
}

// Prints every frame rather than the interval checkpoints the replay tier
// uses. Volume is the point: this output is diffed, not read, and a per-frame
// trace turns "these platforms disagree" into "they disagree at frame 214".
int probe_one(const Replay& replay) {
    mw::sim::GameState state;
    mw::sim::init_state(state, replay.seed);

    mw::sim::InputPair previous{{mw::sim::InputFrame{0u}, mw::sim::InputFrame{0u}}};

    std::printf("# scenario %s seed %llu frames %d\n", replay.name.c_str(),
                static_cast<unsigned long long>(replay.seed), replay.frame_count);
    std::printf("%d %016llX\n", 0, static_cast<unsigned long long>(mw::sim::hash_state(state)));

    for (int32_t frame = 0; frame < replay.frame_count; ++frame) {
        const mw::sim::InputPair current = input_at_frame(replay, frame);
        mw::sim::advance_frame(state, mw::test::shipped_match_data(), current, previous);
        previous = current;

        std::printf("%d %016llX\n", frame + 1,
                    static_cast<unsigned long long>(mw::sim::hash_state(state)));
    }

    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    // Write LF, not CRLF, on Windows.
    //
    // CI diffs this output across three platforms byte for byte. In text mode
    // the Windows CRT rewrites every newline in the stream as a carriage
    // return plus a newline. Every one of the 1971 lines then differed from
    // Linux and macOS by a single trailing byte, and the desync check reported
    // a divergence on frame 0 -- of a simulation that was in fact identical.
    //
    // A test that cries wolf is worse than no test: the natural response to a
    // desync failure is to distrust the checker, and that instinct has to stay
    // wrong for this check to be worth having.
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    if (argc > 1 && (std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0)) {
        std::printf(
            "usage: %s [scenario-name ...]\n"
            "\n"
            "Prints per-frame state hashes for the committed recordings.\n"
            "CI runs this on three platforms and diffs the output.\n",
            argv[0]);
        return 0;
    }

    // Sizes are part of the contract being verified. Two platforms laying
    // GameState out differently would produce different hashes for identical
    // behavior, and printing the sizes turns that from a mystery into the
    // first line of the diff.
    std::printf("# sizeof(GameState)=%zu sizeof(Fighter)=%zu sizeof(Projectile)=%zu\n",
                sizeof(mw::sim::GameState), sizeof(mw::sim::Fighter), sizeof(mw::sim::Projectile));

    int failures = 0;

    for (int32_t i = 0; i < SCENARIO_COUNT; ++i) {
        const Scenario& scenario = SCENARIOS[i];

        if (argc > 1) {
            bool requested = false;
            for (int arg = 1; arg < argc; ++arg) {
                if (std::strcmp(argv[arg], scenario.name) == 0) {
                    requested = true;
                    break;
                }
            }
            if (!requested) {
                continue;
            }
        }

        Replay replay;
        std::string error;
        const ReplayIoStatus status = load_replay(replay_path(scenario.name), replay, error);
        if (status != ReplayIoStatus::Ok) {
            // Must fail rather than skip: a probe that silently emits nothing
            // would make three platforms agree on an empty file forever.
            std::fprintf(stderr, "error: %s: %s (%s)\n", scenario.name,
                         replay_io_status_name(status), error.c_str());
            ++failures;
            continue;
        }

        failures += probe_one(replay);
    }

    if (failures != 0) {
        std::fprintf(stderr, "%d scenario(s) could not be probed\n", failures);
        return 1;
    }
    return 0;
}
