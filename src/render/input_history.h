// A scrolling list of recent inputs, the way a training mode shows them.
//
// Requested after a playtest: the readout showed only the current frame, so an
// input vanished the instant it changed and there was no way to see what you
// had just done. Every fighting game solves this the same way -- a vertical
// list off to the side, newest at the bottom, each entry carrying how long that
// input was held.
//
// THIS IS NOT ADR 0021's INPUT HISTORY. That one lives in GameState because
// motion inputs and auto-combos decide what move comes out, and anything that
// changes the outcome of a match has to be rolled back with everything else.
// This is a picture of the past for a human to read. It affects nothing, so by
// ARCHITECTURE.md 3 it belongs here and must never move down.
//
// Header-only and SDL-free so the behaviour can be tested. The drawing needs a
// renderer; the coalescing is the part with logic in it.
#pragma once

#include <cstdint>
#include <string>

#include "sim/input.h"

namespace ds::render {

// About fifteen seconds of distinct inputs at a realistic rate, and more than
// fits on screen either way. Old entries fall off the top.
inline constexpr int32_t INPUT_HISTORY_CAPACITY = 32;

struct InputHistoryEntry {
    uint16_t buttons = 0;
    int32_t frames_held = 0;
    int32_t started_on = 0;
};

// A ring buffer, oldest first when read through entry_at().
struct InputHistory {
    InputHistoryEntry entries[INPUT_HISTORY_CAPACITY];
    int32_t count = 0;  // How many entries are live, capped at capacity.
    int32_t head = 0;   // Index of the oldest entry.
};

// Call once per SIMULATION frame, never per render frame -- a frame count that
// counts render frames would read differently on different machines and would
// be wrong by a factor of ten on a fast one.
//
// An unchanged input extends the current entry rather than adding one. That is
// what makes holding a direction show as "R  40" instead of forty identical
// lines, and it is why an input stays on screen until the next one arrives.
inline void input_history_push(InputHistory& history, ds::sim::InputFrame input,
                               int32_t sim_frame) {
    const uint16_t buttons = ds::sim::input_sanitized(input).buttons;

    if (history.count > 0) {
        const int32_t newest = (history.head + history.count - 1) % INPUT_HISTORY_CAPACITY;
        if (history.entries[newest].buttons == buttons) {
            ++history.entries[newest].frames_held;
            return;
        }
    }

    const int32_t slot = (history.head + history.count) % INPUT_HISTORY_CAPACITY;
    history.entries[slot] = InputHistoryEntry{buttons, 1, sim_frame};

    if (history.count < INPUT_HISTORY_CAPACITY) {
        ++history.count;
    } else {
        // Full: the new entry overwrote the oldest, so the window slides.
        history.head = (history.head + 1) % INPUT_HISTORY_CAPACITY;
    }
}

// `index` 0 is the oldest entry held, `count - 1` the newest.
inline const InputHistoryEntry& input_history_at(const InputHistory& history, int32_t index) {
    return history.entries[(history.head + index) % INPUT_HISTORY_CAPACITY];
}

// One line for the list: directions then buttons, in the order the controls
// table in AGENTS.md prints them.
//
// Letters rather than the numpad notation the genre normally uses. A player who
// already knows that 236P is a quarter-circle does not need this display; the
// person who does need it has never seen a numpad chart.
inline std::string input_history_label(uint16_t buttons) {
    using ds::sim::Button;
    struct Label {
        Button button;
        const char* name;
    };
    constexpr Label LABELS[] = {
        {Button::Up, "U"},          {Button::Down, "D"},        {Button::Left, "L"},
        {Button::Right, "R"},       {Button::LightPunch, "LP"}, {Button::MediumPunch, "MP"},
        {Button::HeavyPunch, "HP"}, {Button::LightKick, "LK"},  {Button::MediumKick, "MK"},
        {Button::HeavyKick, "HK"},
    };

    std::string out;
    for (const Label& label : LABELS) {
        if ((buttons & static_cast<uint16_t>(label.button)) != 0u) {
            if (!out.empty()) {
                out += " ";
            }
            out += label.name;
        }
    }
    return out.empty() ? "-" : out;
}

}  // namespace ds::render
