// Input as data. ADR 0002.
//
// Every ambiguity resolved in input.h is one the state machine never has to
// handle, and one that cannot resolve differently on two machines. These tests
// pin those resolutions down.
#include <initializer_list>
#include <type_traits>

#include <doctest/doctest.h>

#include "sim/input.h"

using namespace ds::sim;

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
    const InputFrame frame = make({Button::HeavyPunch, Button::MediumKick});

    CHECK(input_held(frame, Button::HeavyPunch));
    CHECK(input_held(frame, Button::MediumKick));
    CHECK_FALSE(input_held(frame, Button::LightPunch));
    CHECK_FALSE(input_held(frame, Button::Up));

    const InputFrame cleared = input_without(frame, Button::MediumKick);
    CHECK(input_held(cleared, Button::HeavyPunch));
    CHECK_FALSE(input_held(cleared, Button::MediumKick));
}

TEST_CASE("All ten buttons occupy distinct bits") {
    // DESIGN.md 4.1. A collision here would make two buttons the same button,
    // which is the sort of thing that is obvious in a test and invisible in
    // a bitfield. Directions are included because they share the field: a
    // sixth attack button colliding with Up would read as a jump.
    const Button all[] = {Button::Up,         Button::Down,       Button::Left,
                          Button::Right,      Button::LightPunch, Button::MediumPunch,
                          Button::HeavyPunch, Button::LightKick,  Button::MediumKick,
                          Button::HeavyKick};

    for (Button a : all) {
        for (Button b : all) {
            const bool same = static_cast<uint16_t>(a) == static_cast<uint16_t>(b);
            const bool overlap = (static_cast<uint16_t>(a) & static_cast<uint16_t>(b)) != 0u;
            CHECK(overlap == same);
        }
    }
}

TEST_CASE("Edge detection compares against the previous frame") {
    const InputFrame pressed = make({Button::HeavyPunch});

    SUBCASE("a button newly held reads as pressed") {
        CHECK(input_pressed(pressed, NEUTRAL, Button::HeavyPunch));
        CHECK_FALSE(input_released(pressed, NEUTRAL, Button::HeavyPunch));
    }

    SUBCASE("a button held for a second frame is no longer pressed") {
        CHECK_FALSE(input_pressed(pressed, pressed, Button::HeavyPunch));
        CHECK(input_held(pressed, Button::HeavyPunch));
    }

    SUBCASE("a button let go reads as released") {
        CHECK(input_released(NEUTRAL, pressed, Button::HeavyPunch));
        CHECK_FALSE(input_pressed(NEUTRAL, pressed, Button::HeavyPunch));
    }

    SUBCASE("a button untouched is neither") {
        CHECK_FALSE(input_pressed(NEUTRAL, NEUTRAL, Button::HeavyPunch));
        CHECK_FALSE(input_released(NEUTRAL, NEUTRAL, Button::HeavyPunch));
    }
}

TEST_CASE("Opposing horizontal directions resolve to neutral") {
    // Physically reachable on a keyboard and on a leverless controller, where
    // each direction is its own button. Resolving it in one place means the
    // state machine never sees the contradiction.
    //
    // Neutral is one of the accepted SOCD schemes for the horizontal axis; the
    // alternative used by some games is last-input-wins, which needs input
    // history the sim deliberately does not keep.
    CHECK(input_horizontal(make({Button::Left, Button::Right})) == 0);
}

TEST_CASE("Up beats Down when both are held") {
    // NOT symmetric with the horizontal rule, and deliberately so.
    //
    // Resolving this pair to neutral made a real input do nothing: a player
    // crouch-blocking holds Down, and to jump they press Up without releasing
    // it. Neutral is neither a crouch nor a jump, so the fighter just stood
    // there. Up priority is what essentially every fighting game does.
    //
    // Unreachable with a gamepad -- a d-pad pivots and a stick has one position
    // -- but reachable from the keyboard the game already ships with.
    CHECK(input_vertical(make({Button::Up, Button::Down})) == -1);
    CHECK(input_vertical(make({Button::Up})) == -1);
    CHECK(input_vertical(make({Button::Down})) == 1);
    CHECK(input_vertical(NEUTRAL) == 0);
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
        const InputFrame real = make({Button::HeavyKick, Button::Down});
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
