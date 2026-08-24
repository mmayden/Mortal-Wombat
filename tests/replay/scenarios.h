// Replay scenarios — the input scripts the recordings are generated from.
//
// Shared by the replay test and by mw_replay_record so that re-recording
// cannot drift from what is tested. A scenario is deliberately code rather
// than data: the recorded FILE is the artifact under test, and the script is
// how that artifact gets regenerated when a change legitimately invalidates it.
//
// ADR 0011 targets 50+ recordings covering combos, edge cases, round
// transitions, and every past bug. These four exist to prove the mechanism;
// the library grows as behavior does. Every fixed bug should leave one behind.
#pragma once

#include <cstdint>

#include "sim/input.h"

namespace mw::test {

struct Scenario {
    const char* name;
    const char* description;
    uint64_t seed;
    int32_t frame_count;
    mw::sim::InputPair (*script)(int32_t frame);
};

namespace scripts {

using mw::sim::Button;
using mw::sim::InputFrame;
using mw::sim::InputPair;

constexpr InputFrame NEUTRAL{0u};

inline InputPair idle(int32_t) {
    return InputPair{{NEUTRAL, NEUTRAL}};
}

// Both fighters walk toward each other, then away. Pins down walk speeds, the
// forward/backward asymmetry, and pushback-free approach.
inline InputPair approach_and_retreat(int32_t frame) {
    const bool approaching = (frame / 60) % 2 == 0;
    const InputFrame p1 = input_with(NEUTRAL, approaching ? Button::Right : Button::Left);
    const InputFrame p2 = input_with(NEUTRAL, approaching ? Button::Left : Button::Right);
    return InputPair{{p1, p2}};
}

// Player 1 walks into the left wall and stays there. Pins down stage clamping,
// which is what keeps positions bounded and lets fixed.h treat overflow as a
// logic error rather than a case to handle.
inline InputPair walk_into_wall(int32_t) {
    return InputPair{{input_with(NEUTRAL, Button::Left), NEUTRAL}};
}

// Player 2 blocks while player 1 advances and attacks. Pins down that block is
// a button rather than hold-back (DESIGN.md 4.1) and that blocking holds
// position.
inline InputPair advance_versus_block(int32_t frame) {
    InputFrame p1 = input_with(NEUTRAL, Button::Right);
    if ((frame / 24) % 2 == 0) {
        p1 = input_with(p1, Button::HighPunch);
    }

    // Player 2 blocks, and also holds back, to prove the two do not interact.
    InputFrame p2 = input_with(NEUTRAL, Button::Block);
    if ((frame / 40) % 3 == 0) {
        p2 = input_with(p2, Button::Right);
    }

    return InputPair{{p1, p2}};
}

// Crouching and standing on a cycle, both players out of phase. Pins down the
// crouch transition and that opposing directions resolve to neutral.
inline InputPair crouch_cycle(int32_t frame) {
    InputFrame p1 = NEUTRAL;
    InputFrame p2 = NEUTRAL;

    if ((frame / 15) % 2 == 0) {
        p1 = input_with(p1, Button::Down);
    }
    if ((frame / 15) % 3 == 0) {
        // Both directions at once, which a keyboard can produce and which the
        // input layer resolves to neutral rather than leaving to the state
        // machine.
        p2 = input_with(input_with(p2, Button::Left), Button::Right);
    } else {
        p2 = input_with(p2, Button::Down);
    }

    return InputPair{{p1, p2}};
}

// Input is IGNORED during the round-start freeze: advance_frame only acts on
// player input while round_phase is Fighting, and ROUND_START_FREEZE_FRAMES is
// 90. A scenario that starts walking on frame 0 has not moved by frame 90.
//
// Every script below therefore waits it out. Getting this wrong is silent: the
// fighters simply never close the distance and every attack whiffs, while the
// replay still reproduces perfectly.
inline constexpr int32_t FREEZE_FRAMES = 90;

// Long enough to close from the 200-unit round-start separation to inside the
// reach of a normal, at 1.2 units per frame.
inline constexpr int32_t APPROACH_UNTIL = FREEZE_FRAMES + 130;

// Player one walks in and throws high punches; player two stands still. Pins
// down the whole hit chain: startup, active frames, box overlap, damage, and
// hitstun.
//
// This is the scenario that would catch an agent refactoring movement or box
// geometry into something that compiles, passes every unit test, and no longer
// connects.
inline InputPair walk_in_and_punch(int32_t frame) {
    InputFrame p1 = NEUTRAL;
    if (frame < APPROACH_UNTIL) {
        p1 = input_with(p1, Button::Right);
    } else if (frame % 40 == 0) {
        p1 = input_with(p1, Button::HighPunch);
    }
    return InputPair{{p1, NEUTRAL}};
}

// The same approach, but player two holds block throughout. Pins down that a
// blocked hit deals no damage -- the current behaviour, with chip damage
// undecided -- while still applying blockstun.
inline InputPair attack_into_block(int32_t frame) {
    InputFrame p1 = NEUTRAL;
    if (frame < APPROACH_UNTIL) {
        p1 = input_with(p1, Button::Right);
    } else if (frame % 30 == 0) {
        p1 = input_with(p1, Button::LowKick);
    }
    return InputPair{{p1, input_with(NEUTRAL, Button::Block)}};
}

// Both fighters close and mash high punch. Pins down trades -- that a
// simultaneous hit lands for both, rather than the lower-indexed player
// silently winning every exchange.
inline InputPair mutual_pressure(int32_t frame) {
    InputFrame p1 = NEUTRAL;
    InputFrame p2 = NEUTRAL;
    if (frame < FREEZE_FRAMES + 70) {
        p1 = input_with(p1, Button::Right);
        p2 = input_with(p2, Button::Left);
    } else if (frame % 26 == 0) {
        p1 = input_with(p1, Button::HighPunch);
        p2 = input_with(p2, Button::HighPunch);
    }
    return InputPair{{p1, p2}};
}

// Player one jumps in and attacks from the air; player two stands still. Pins
// down the fixed arc, the jump attack, and landing recovery together.
//
// A jump is the one action whose whole value is that it cannot be changed once
// started (DESIGN.md 4.3), so this recording is what would catch air control
// creeping in -- the fighter would land somewhere else and the hash would move.
inline InputPair jump_in_and_attack(int32_t frame) {
    InputFrame p1 = NEUTRAL;
    if (frame < FREEZE_FRAMES + 60) {
        p1 = input_with(p1, Button::Right);
    } else if (frame % 70 == 0) {
        p1 = input_with(input_with(p1, Button::Up), Button::Right);
    } else if (frame % 70 == 20) {
        p1 = input_with(p1, Button::HighPunch);
    }
    return InputPair{{p1, NEUTRAL}};
}

}  // namespace scripts

// The frame count on each scenario clears the 90-frame round-start freeze, so
// every recording actually exercises live gameplay rather than the intro.
inline constexpr Scenario SCENARIOS[] = {
    {"idle", "Both fighters neutral for five seconds. The baseline.", 20260822u, 300,
     &scripts::idle},
    {"approach_retreat", "Both fighters walk in and out. Covers walk speeds.", 1234567u, 420,
     &scripts::approach_and_retreat},
    {"wall_clamp", "Player 1 walks into the left wall and holds.", 98765u, 400,
     &scripts::walk_into_wall},
    {"advance_vs_block", "Player 1 advances and attacks into player 2's block.", 555u, 480,
     &scripts::advance_versus_block},
    {"crouch_cycle", "Both fighters crouch and stand out of phase.", 31337u, 360,
     &scripts::crouch_cycle},
    {"punch_connects", "Player 1 walks in and lands high punches. The hit chain.", 2468u, 420,
     &scripts::walk_in_and_punch},
    {"attack_into_block", "Player 1 attacks into a held block. No damage, blockstun.", 1357u, 420,
     &scripts::attack_into_block},
    {"mutual_pressure", "Both fighters punch from close range. Trades.", 8642u, 420,
     &scripts::mutual_pressure},
    {"jump_attack", "Player 1 jumps in and attacks from the air. Fixed arcs.", 13579u, 480,
     &scripts::jump_in_and_attack},
};

inline constexpr int32_t SCENARIO_COUNT =
    static_cast<int32_t>(sizeof(SCENARIOS) / sizeof(SCENARIOS[0]));

// Full-state hashes are recorded every this many frames, so that a divergence
// reports the interval it began in rather than only that the end differed.
inline constexpr int32_t CHECKPOINT_INTERVAL = 30;

}  // namespace mw::test
