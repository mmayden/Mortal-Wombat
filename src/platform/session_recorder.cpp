#include "platform/session_recorder.h"

#include <cstdarg>
#include <cstdio>

#include <SDL3/SDL.h>

#include "sim/hash.h"
#include "sim/state.h"

#include "mw_log.h"

namespace mw::platform {
namespace {

// The replay format version this writes. Must track REPLAY_FORMAT_VERSION in
// tests/replay/replay_file.cpp -- if that bumps and this does not, the reader
// rejects recordings made here, which is the correct failure but only if
// somebody notices this comment.
constexpr int32_t RECORDER_FORMAT_VERSION = 1;

// Formats one line and writes it.
//
// Everything goes through SDL_IOStream because that is how this project already
// writes files (SDL_SaveBMP for screenshots), and because MSVC treats a plain
// fopen as a deprecation error under /WX.
bool write_line(SDL_IOStream* stream, const char* format, ...) {
    char buffer[256];

    std::va_list args;
    va_start(args, format);
    const int length = std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (length < 0 || static_cast<size_t>(length) >= sizeof(buffer)) {
        return false;
    }
    return SDL_WriteIO(stream, buffer, static_cast<size_t>(length)) == static_cast<size_t>(length);
}

}  // namespace

void recorder_begin(SessionRecorder& recorder, uint64_t seed) {
    recorder.active = true;
    recorder.truncated = false;
    recorder.seed = seed;
    recorder.frame_count = 0;
    recorder.change_count = 0;
    recorder.checkpoint_count = 0;
}

void recorder_frame(SessionRecorder& recorder, mw::sim::InputPair input,
                    const mw::sim::GameState& state) {
    if (!recorder.active) {
        return;
    }

    // The frame this input applied to. advance_frame has already incremented,
    // so the frame just simulated is one behind the current value.
    const int32_t frame = state.frame - 1;

    // Store a change only when the input actually changed, which is what keeps
    // a ten-second recording a few dozen readable lines instead of six hundred.
    const bool first = recorder.change_count == 0;
    const RecordedChange* last = first ? nullptr : &recorder.changes[recorder.change_count - 1];
    const bool changed = first || last->buttons[0] != input.players[0].buttons ||
                         last->buttons[1] != input.players[1].buttons;

    if (changed) {
        if (recorder.change_count < MAX_RECORDED_CHANGES) {
            RecordedChange& entry = recorder.changes[recorder.change_count++];
            entry.frame = frame;
            entry.buttons[0] = input.players[0].buttons;
            entry.buttons[1] = input.players[1].buttons;
        } else {
            recorder.truncated = true;
        }
    }

    // Hashed after advancing, matching run_replay in tests/replay/replay_runner.h.
    // Taking it before would produce a file that never reproduces.
    if (state.frame % RECORDER_CHECKPOINT_INTERVAL == 0) {
        if (recorder.checkpoint_count < MAX_RECORDED_CHECKPOINTS) {
            RecordedCheckpoint& entry = recorder.checkpoints[recorder.checkpoint_count++];
            entry.frame = state.frame;
            entry.hash = mw::sim::hash_state(state);
        } else {
            recorder.truncated = true;
        }
    }

    recorder.frame_count = state.frame;
}

bool recorder_write(const SessionRecorder& recorder, const char* path,
                    const mw::sim::GameState& final_state) {
    if (!recorder.active) {
        return false;
    }

    // "wb" and an explicit newline escape in every line below, so the file is
    // byte-identical on Windows and Linux. A recording differing only by line
    // ending reads as a diff every time it is regenerated on the other
    // platform -- and a CRLF difference in exactly this kind of output once
    // produced a false desync across all 1971 lines of a state trace.
    SDL_IOStream* stream = SDL_IOFromFile(path, "wb");
    if (stream == nullptr) {
        MW_LOG_ERROR("could not open '%s' for writing: %s", path, SDL_GetError());
        return false;
    }

    bool ok = true;
    ok = ok && write_line(stream, "# Replay recording -- captured from a played session.\n");
    ok = ok && write_line(stream, "#\n");
    ok = ok &&
         write_line(stream, "# Written by mortal_wombat --record. Do not hand-edit the hashes.\n");
    ok = ok &&
         write_line(stream, "# Drop this into tests/replays/ and add its name to the scenario\n");
    ok = ok && write_line(stream, "# list to turn a bug someone found by playing into a test.\n");
    if (recorder.truncated) {
        ok = ok && write_line(stream, "#\n");
        ok = ok &&
             write_line(stream, "# WARNING: this session exceeded the recorder's limits and was\n");
        ok = ok && write_line(stream, "# TRUNCATED. It will not replay the whole session.\n");
    }
    ok = ok && write_line(stream, "\n");
    ok = ok && write_line(stream, "version %d\n", RECORDER_FORMAT_VERSION);
    ok = ok && write_line(stream, "name recorded_session\n");
    ok = ok && write_line(stream, "description Captured from live play.\n");
    ok = ok && write_line(stream, "seed %llu\n", static_cast<unsigned long long>(recorder.seed));
    ok = ok && write_line(stream, "frames %d\n", recorder.frame_count);

    ok = ok && write_line(stream, "\n");
    ok = ok &&
         write_line(stream,
                    "# input <frame> <p1_buttons> <p2_buttons> -- holds until the next line\n");
    for (int32_t i = 0; i < recorder.change_count && ok; ++i) {
        const RecordedChange& change = recorder.changes[i];
        ok = write_line(stream, "input %d 0x%04X 0x%04X\n", change.frame, change.buttons[0],
                        change.buttons[1]);
    }

    ok = ok && write_line(stream, "\n");
    ok = ok &&
         write_line(stream,
                    "# Full state hash at intervals, so a divergence reports its first frame.\n");
    for (int32_t i = 0; i < recorder.checkpoint_count && ok; ++i) {
        const RecordedCheckpoint& checkpoint = recorder.checkpoints[i];
        ok = write_line(stream, "checkpoint %d 0x%016llX\n", checkpoint.frame,
                        static_cast<unsigned long long>(checkpoint.hash));
    }

    ok = ok && write_line(stream, "\n");
    ok = ok &&
         write_line(stream, "# Visible-state hash after the final frame. Excludes RNG position\n");
    ok = ok &&
         write_line(stream, "# so that a render-side effect drawing a number cannot fail this.\n");
    ok =
        ok && write_line(stream, "final 0x%016llX\n",
                         static_cast<unsigned long long>(mw::sim::hash_state_visible(final_state)));

    const bool closed = SDL_CloseIO(stream);
    if (!closed || !ok) {
        MW_LOG_ERROR("failed to finish writing '%s': %s", path, SDL_GetError());
        return false;
    }

    MW_LOG_INFO("recorded %d frames (%d input changes) to %s", recorder.frame_count,
                recorder.change_count, path);
    if (recorder.truncated) {
        MW_LOG_WARN("the session was truncated -- it exceeded the recorder's limits");
    }
    return true;
}

}  // namespace mw::platform
