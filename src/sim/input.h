// Input as data. ADR 0002.
//
// The simulation never polls a keyboard, a gamepad, or SDL. It receives one
// InputFrame per player per frame and nothing else. That is what makes a match
// reducible to a seed plus a list of these — which is what makes replays,
// rollback, and the desync test possible.
//
// The platform layer translates devices into this; the sim only sees the
// bitfield.
#pragma once

#include <cstdint>

namespace ds::sim {

// Six attack buttons and four directions -- DESIGN.md 4.1, ADR 0021.
//
// LIGHT / MEDIUM / HEAVY, not low/mid/high. The names matter: attack HEIGHT is
// a separate axis entirely and is still undecided (drawing-board/RULESET.md
// decision 7). These buttons were called LowPunch and HighPunch until the sixth
// button landed, which would have read as "a punch that hits low" the moment
// heights arrived.
//
// There is no Block button. Blocking is holding BACK (DESIGN.md 4.1), which is
// what makes a crossup an axis of offence: because "back" is relative to the
// opponent, an attack that changes sides mid-animation forces the defender to
// reverse their input. A block button gives every attack the same answer and
// deletes that axis entirely.
//
// It costs something real, which is the point. Blocking and retreating are now
// the same input, so a defender cannot do both -- and that is what puts throws
// back under consideration (drawing-board/RULESET.md decision 13).
enum class Button : uint16_t {
    Up = 1u << 0,
    Down = 1u << 1,
    Left = 1u << 2,
    Right = 1u << 3,
    LightPunch = 1u << 4,
    MediumPunch = 1u << 5,
    HeavyPunch = 1u << 6,
    LightKick = 1u << 7,
    MediumKick = 1u << 8,
    HeavyKick = 1u << 9,
};

// Sixteen bits is what rollback sends over the wire per player per frame, so
// this is deliberately small. Ten bits are used; the rest are reserved and
// must stay zero, since a nonzero reserved bit would change the state hash
// without changing behavior.
struct InputFrame {
    uint16_t buttons;
};

inline constexpr uint16_t INPUT_BUTTON_MASK = 0x03FFu;

constexpr bool input_held(InputFrame input, Button button) {
    return (input.buttons & static_cast<uint16_t>(button)) != 0u;
}

// "Pressed" means held this frame and not the previous one. The sim computes
// this from two InputFrames rather than storing edge state, so that a rollback
// to an earlier frame recomputes it correctly instead of restoring a stale
// edge flag.
constexpr bool input_pressed(InputFrame current, InputFrame previous, Button button) {
    return input_held(current, button) && !input_held(previous, button);
}

constexpr bool input_released(InputFrame current, InputFrame previous, Button button) {
    return !input_held(current, button) && input_held(previous, button);
}

constexpr InputFrame input_with(InputFrame input, Button button) {
    return InputFrame{static_cast<uint16_t>(input.buttons | static_cast<uint16_t>(button))};
}

constexpr InputFrame input_without(InputFrame input, Button button) {
    return InputFrame{static_cast<uint16_t>(input.buttons &
                                            static_cast<uint16_t>(~static_cast<uint16_t>(button)))};
}

// Strips reserved bits. The platform layer calls this before handing input to
// the sim, so that a device driver setting a bit we do not use cannot alter a
// state hash and read as a desync.
constexpr InputFrame input_sanitized(InputFrame input) {
    return InputFrame{static_cast<uint16_t>(input.buttons & INPUT_BUTTON_MASK)};
}

// Left and Right pressed together is physically possible on a keyboard and on
// a broken d-pad. Resolving it here, once, keeps the ambiguity out of the state
// machine. Neither wins: the fighter stands still.
constexpr int32_t input_horizontal(InputFrame input) {
    const bool left = input_held(input, Button::Left);
    const bool right = input_held(input, Button::Right);
    if (left == right) {
        return 0;
    }
    return right ? 1 : -1;
}

// Up wins when both are held. This is SOCD resolution -- Simultaneous Opposing
// Cardinal Directions -- and it is not symmetric with the horizontal rule.
//
// A d-pad pivots and an analog stick has one position, so neither can report Up
// and Down at once. A keyboard can, because the keys are independent, and so can
// a leverless controller, where every direction is its own button.
//
// Resolving to neutral made a real input do nothing. A player crouch-blocking
// holds Down; to jump they press Up without releasing Down. Under a neutral rule
// that is neither a crouch nor a jump, and the fighter just stands there. Up
// priority is what essentially every fighting game does, for exactly this case.
//
// It could not be found with a gamepad, because a gamepad cannot produce the
// input -- but it was reachable from the keyboard the game already ships with.
constexpr int32_t input_vertical(InputFrame input) {
    // Returns +1 for Down and -1 for Up, matching sim screen space where -y is
    // up (ARCHITECTURE.md 3).
    if (input_held(input, Button::Up)) {
        return -1;
    }
    return input_held(input, Button::Down) ? 1 : 0;
}

// Both players' input for one frame — the complete input to advance_frame.
struct InputPair {
    InputFrame players[2];
};

}  // namespace ds::sim
