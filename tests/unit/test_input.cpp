// Input as data. ADR 0002.
//
// Every ambiguity resolved in input.h is one the state machine never has to
// handle, and one that cannot resolve differently on two machines. These tests
// pin those resolutions down.
#include <initializer_list>
#include <type_traits>

#include <doctest/doctest.h>

#include "sim/input.h"

using namespace mw::sim;

namespace {
constexpr InputFrame NEUTRAL{0u};

InputFrame make(std::initializer_list<Button> buttons) {
    InputFrame frame = NEUTRAL;
    for (Button button : buttons) {
        frame = input_with(frame, button);
    }
    return frame;
}
}  // namespace

TEST_CASE("Buttons set and clear independently") {
    const InputFrame frame = make({Button::HighPunch, Button::Block});

    CHECK(input_held(frame, Button::HighPunch));
    CHECK(input_held(frame, Button::Block));
    CHECK_FALSE(input_held(frame, Button::LowPunch));
    CHECK_FALSE(input_held(frame, Button::Up));

    const InputFrame cleared = input_without(frame, Button::Block);
    CHECK(input_held(cleared, Button::HighPunch));
    CHECK_FALSE(input_held(cleared, Button::Block));
}

TEST_CASE("All five buttons occupy distinct bits") {
    // DESIGN.md 4.1. A collision here would make two buttons the same button,
    // which is the sort of thing that is obvious in a test and invisible in
    // a bitfield.
    const Button all[] = {Button::LowPunch, Button::HighPunch, Button::LowKick, Button::HighKick,
                          Button::Block};

    for (Button a : all) {
        for (Button b : all) {
            const bool same = static_cast<uint16_t>(a) == static_cast<uint16_t>(b);
            const bool overlap = (static_cast<uint16_t>(a) & static_cast<uint16_t>(b)) != 0u;
            CHECK(overlap == same);
        }
    }
}

TEST_CASE("Edge detection compares against the previous frame") {
    const InputFrame pressed = make({Button::HighPunch});

    SUBCASE("a button newly held reads as pressed") {
        CHECK(input_pressed(pressed, NEUTRAL, Button::HighPunch));
        CHECK_FALSE(input_released(pressed, NEUTRAL, Button::HighPunch));
    }

    SUBCASE("a button held for a second frame is no longer pressed") {
        CHECK_FALSE(input_pressed(pressed, pressed, Button::HighPunch));
        CHECK(input_held(pressed, Button::HighPunch));
    }

    SUBCASE("a button let go reads as released") {
        CHECK(input_released(NEUTRAL, pressed, Button::HighPunch));
        CHECK_FALSE(input_pressed(NEUTRAL, pressed, Button::HighPunch));
    }

    SUBCASE("a button untouched is neither") {
        CHECK_FALSE(input_pressed(NEUTRAL, NEUTRAL, Button::HighPunch));
        CHECK_FALSE(input_released(NEUTRAL, NEUTRAL, Button::HighPunch));
    }
}

TEST_CASE("Opposing directions resolve to neutral") {
    // Physically reachable on a keyboard and on a worn d-pad. Resolving it in
    // one place means the state machine never sees the contradiction.
    CHECK(input_horizontal(make({Button::Left, Button::Right})) == 0);
    CHECK(input_vertical(make({Button::Up, Button::Down})) == 0);
}

TEST_CASE("Single directions resolve to a sign") {
    CHECK(input_horizontal(make({Button::Right})) == 1);
    CHECK(input_horizontal(make({Button::Left})) == -1);
    CHECK(input_horizontal(NEUTRAL) == 0);

    // +1 is Down and -1 is Up, matching sim screen space where -y is up.
    CHECK(input_vertical(make({Button::Down})) == 1);
    CHECK(input_vertical(make({Button::Up})) == -1);
    CHECK(input_vertical(NEUTRAL) == 0);
}

TEST_CASE("Sanitizing strips reserved bits") {
    // A device driver or a malformed network packet setting an unused bit would
    // otherwise change the state hash without changing behavior, and read as a
    // desync on a frame where nothing actually diverged.
    const InputFrame dirty{0xFFFFu};
    const InputFrame clean = input_sanitized(dirty);

    CHECK(clean.buttons == INPUT_BUTTON_MASK);
    CHECK((clean.buttons & static_cast<uint16_t>(~INPUT_BUTTON_MASK)) == 0u);

    SUBCASE("meaningful bits survive") {
        const InputFrame real = make({Button::HighKick, Button::Down});
        CHECK(input_sanitized(real).buttons == real.buttons);
    }

    SUBCASE("sanitizing is idempotent") {
        CHECK(input_sanitized(clean).buttons == clean.buttons);
    }
}

TEST_CASE("InputFrame stays small enough to send every frame") {
    // Rollback transmits this per player per frame. Growth here is bandwidth
    // and latency, both of which a fighting game is judged on.
    CHECK(sizeof(InputFrame) == sizeof(uint16_t));
    CHECK(std::is_trivially_copyable_v<InputFrame>);
    CHECK(std::is_trivially_copyable_v<InputPair>);
}
