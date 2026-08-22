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

namespace mw::sim {

// DESIGN.md 4.1: five buttons and four directions. Block is a button, not
// hold-back, which removes the walk-backward/block ambiguity entirely.
enum class Button : uint16_t {
    Up = 1u << 0,
    Down = 1u << 1,
    Left = 1u << 2,
    Right = 1u << 3,
    LowPunch = 1u << 4,
    HighPunch = 1u << 5,
    LowKick = 1u << 6,
    HighKick = 1u << 7,
    Block = 1u << 8,
};

// Sixteen bits is what rollback sends over the wire per player per frame, so
// this is deliberately small. Nine bits are used; the rest are reserved and
// must stay zero, since a nonzero reserved bit would change the state hash
// without changing behavior.
struct InputFrame {
    uint16_t buttons;
};

inline constexpr uint16_t INPUT_BUTTON_MASK = 0x01FFu;

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

constexpr int32_t input_vertical(InputFrame input) {
    const bool up = input_held(input, Button::Up);
    const bool down = input_held(input, Button::Down);
    if (up == down) {
        return 0;
    }
    // Returns +1 for Down and -1 for Up, matching sim screen space where -y is
    // up (ARCHITECTURE.md 3). Both-held resolves to neutral, same as the
    // horizontal rule.
    return down ? 1 : -1;
}

// Both players' input for one frame — the complete input to advance_frame.
struct InputPair {
    InputFrame players[2];
};

}  // namespace mw::sim
