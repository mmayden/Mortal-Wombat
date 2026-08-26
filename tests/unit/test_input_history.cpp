// The training-mode input list.
//
// Requested after a playtest: the readout showed only the current frame, so an
// input vanished the moment it changed and there was no way to see what you had
// just done.
//
// The logic worth testing is the coalescing. A held direction must be ONE row
// with a frame count, not forty identical rows -- that is what makes the list
// readable, and what makes an input stay on screen until the next one arrives.
//
// This history is render-side and affects nothing (ARCHITECTURE.md 3). It is
// not ADR 0021's input history, which lives in GameState because motion inputs
// decide which move comes out.

#include <string>

#include <doctest/doctest.h>

#include "render/input_history.h"

using ds::render::input_history_at;
using ds::render::INPUT_HISTORY_CAPACITY;
using ds::render::input_history_label;
using ds::render::input_history_push;
using ds::render::InputHistory;
using ds::sim::Button;
using ds::sim::InputFrame;

namespace {

InputFrame held(std::initializer_list<Button> buttons) {
    InputFrame frame{0u};
    for (Button button : buttons) {
        frame = ds::sim::input_with(frame, button);
    }
    return frame;
}

}  // namespace

TEST_CASE("A held input is one row that grows, not one row per frame") {
    InputHistory history;
    for (int32_t frame = 0; frame < 40; ++frame) {
        input_history_push(history, held({Button::Right}), frame);
    }

    REQUIRE(history.count == 1);
    CHECK(input_history_at(history, 0).frames_held == 40);
    CHECK(input_history_at(history, 0).started_on == 0);
}

TEST_CASE("A changed input starts a new row") {
    InputHistory history;
    input_history_push(history, held({Button::Right}), 0);
    input_history_push(history, held({Button::Right}), 1);
    input_history_push(history, held({Button::Right, Button::HeavyKick}), 2);
    input_history_push(history, held({Button::Right}), 3);

    REQUIRE(history.count == 3);
    CHECK(input_history_at(history, 0).frames_held == 2);
    CHECK(input_history_at(history, 1).frames_held == 1);
    CHECK(input_history_at(history, 1).started_on == 2);
    CHECK(input_history_at(history, 2).frames_held == 1);
}

TEST_CASE("Releasing everything is itself an entry") {
    // Letting go is an input. A list that hid it would show a punch apparently
    // held forever, which is exactly the confusion this display exists to fix.
    InputHistory history;
    input_history_push(history, held({Button::LightPunch}), 0);
    input_history_push(history, InputFrame{0u}, 1);

    REQUIRE(history.count == 2);
    CHECK(input_history_at(history, 1).buttons == 0u);
}

TEST_CASE("The list slides once it is full, keeping the newest") {
    InputHistory history;

    // Alternate two inputs so every push is a distinct entry.
    const int32_t pushes = INPUT_HISTORY_CAPACITY + 10;
    for (int32_t i = 0; i < pushes; ++i) {
        input_history_push(history, (i % 2 == 0) ? held({Button::Left}) : held({Button::Right}), i);
    }

    REQUIRE(history.count == INPUT_HISTORY_CAPACITY);

    // The newest entry is the last one pushed, and the oldest survivor is the
    // right distance behind it. A ring buffer that loses the NEWEST would be
    // useless and would still look plausible from a count.
    const auto& newest = input_history_at(history, history.count - 1);
    CHECK(newest.started_on == pushes - 1);

    const auto& oldest = input_history_at(history, 0);
    CHECK(oldest.started_on == pushes - INPUT_HISTORY_CAPACITY);
}

TEST_CASE("Entries stay in order after the buffer wraps") {
    InputHistory history;
    const int32_t pushes = INPUT_HISTORY_CAPACITY * 2 + 5;
    for (int32_t i = 0; i < pushes; ++i) {
        input_history_push(history, (i % 2 == 0) ? held({Button::Up}) : held({Button::Down}), i);
    }

    for (int32_t i = 1; i < history.count; ++i) {
        INFO("index " << i);
        CHECK(input_history_at(history, i).started_on >
              input_history_at(history, i - 1).started_on);
    }
}

TEST_CASE("Every button is nameable in the list") {
    // A button missing from the label table is a control the display silently
    // cannot see -- the same hole --input-test exists to rule out.
    constexpr Button ALL[] = {Button::Up,         Button::Down,       Button::Left,
                              Button::Right,      Button::LightPunch, Button::MediumPunch,
                              Button::HeavyPunch, Button::LightKick,  Button::MediumKick,
                              Button::HeavyKick};

    for (Button button : ALL) {
        CHECK(input_history_label(static_cast<uint16_t>(button)) != "-");
    }
    CHECK(input_history_label(0u) == "-");
}

TEST_CASE("A direction and a button share one row") {
    const std::string label = input_history_label(
        static_cast<uint16_t>(held({Button::Down, Button::HeavyPunch}).buttons));
    CHECK(label.find("D") != std::string::npos);
    CHECK(label.find("HP") != std::string::npos);
}
