// Regenerates the committed replay recordings.
//
// Run this when a deliberate behavior or balance change invalidates the replay
// tier, and commit the regenerated files IN THE SAME COMMIT as the change
// (AGENTS.md rule 8). A separate "fix tests" commit destroys the signal that
// makes replays worth having: a reviewer can no longer tell whether the
// recordings changed because behavior changed on purpose, or because someone
// made the red go away.
//
// Because this is the tool that makes it easy to erase a failure, it prints
// the hash diff for every recording it rewrites. Regenerating should feel like
// reviewing a diff, not like clearing a warning.
#include <cstdio>
#include <cstring>
#include <string>

#include "replay/replay_file.h"
#include "replay/replay_runner.h"
#include "replay/scenarios.h"

using namespace ds::test;

namespace {

std::string replay_path(const char* name) {
    return std::string(DS_REPLAY_DIR) + "/" + name + ".replay";
}

void print_usage(const char* argv0) {
    std::printf(
        "usage: %s [scenario-name ...]\n"
        "\n"
        "Regenerates replay recordings in %s.\n"
        "With no arguments, regenerates all %d scenarios.\n"
        "\n"
        "Scenarios:\n",
        argv0, DS_REPLAY_DIR, SCENARIO_COUNT);
    for (int32_t i = 0; i < SCENARIO_COUNT; ++i) {
        std::printf("  %-20s %s\n", SCENARIOS[i].name, SCENARIOS[i].description);
    }
}

// Reports what changed, so regenerating is a reviewed act rather than a reflex.
void report_difference(const Scenario& scenario, const RunResult& result) {
    Replay existing;
    std::string error;
    if (load_replay(replay_path(scenario.name), existing, error) != ReplayIoStatus::Ok) {
        std::printf("  %-20s NEW (no previous recording)\n", scenario.name);
        return;
    }

    if (existing.final_visible_hash == result.final_visible_hash) {
        std::printf("  %-20s unchanged\n", scenario.name);
        return;
    }

    int32_t first_divergent_frame = -1;
    const size_t count = existing.checkpoints.size() < result.checkpoints.size()
                             ? existing.checkpoints.size()
                             : result.checkpoints.size();
    for (size_t i = 0; i < count; ++i) {
        if (existing.checkpoints[i].hash != result.checkpoints[i].hash) {
            first_divergent_frame = existing.checkpoints[i].frame;
            break;
        }
    }

    std::printf("  %-20s CHANGED  final 0x%016llX -> 0x%016llX", scenario.name,
                static_cast<unsigned long long>(existing.final_visible_hash),
                static_cast<unsigned long long>(result.final_visible_hash));
    if (first_divergent_frame >= 0) {
        std::printf("  (first divergence by frame %d)", first_divergent_frame);
    }
    std::printf("\n");
}

int record_one(const Scenario& scenario) {
    const RunResult result = run_scenario(scenario.seed, scenario.frame_count, scenario.script);

    report_difference(scenario, result);

    Replay replay;
    replay.name = scenario.name;
    replay.description = scenario.description;
    replay.seed = scenario.seed;
    replay.frame_count = scenario.frame_count;
    replay.changes = compress_script(scenario.frame_count, scenario.script);
    replay.checkpoints = result.checkpoints;
    replay.final_visible_hash = result.final_visible_hash;

    const std::string path = replay_path(scenario.name);
    const ReplayIoStatus status = save_replay(path, replay);
    if (status != ReplayIoStatus::Ok) {
        std::fprintf(stderr, "error: could not write %s: %s\n", path.c_str(),
                     replay_io_status_name(status));
        return 1;
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && (std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0)) {
        print_usage(argv[0]);
        return 0;
    }

    std::printf("Recording replays into %s\n", DS_REPLAY_DIR);

    int failures = 0;
    int recorded = 0;

    if (argc == 1) {
        for (int32_t i = 0; i < SCENARIO_COUNT; ++i) {
            failures += record_one(SCENARIOS[i]);
            ++recorded;
        }
    } else {
        for (int arg = 1; arg < argc; ++arg) {
            bool found = false;
            for (int32_t i = 0; i < SCENARIO_COUNT; ++i) {
                if (std::strcmp(argv[arg], SCENARIOS[i].name) == 0) {
                    failures += record_one(SCENARIOS[i]);
                    ++recorded;
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::fprintf(stderr, "error: unknown scenario '%s'\n", argv[arg]);
                print_usage(argv[0]);
                return 2;
            }
        }
    }

    std::printf("\n%d recording(s) written, %d failure(s).\n", recorded, failures);
    if (failures == 0) {
        std::printf(
            "\nReview the diff before committing. If a recording CHANGED, the behavior\n"
            "of the game changed -- make sure that was on purpose, and commit these\n"
            "files together with the change that caused them.\n");
    }
    return failures == 0 ? 0 : 1;
}
