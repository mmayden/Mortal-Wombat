// A jump attack must be able to hit a standing opponent.
//
// Obvious, untested, and false for the entire life of the feature. Two separate
// causes, neither visible from any other test:
//
//   1. resolve_hits keyed on FighterState::Attack, but a jump attack runs while
//      the state is Airborne, so it was never evaluated for collision at all.
//   2. Its hitbox was authored at standing-punch height, so even once evaluated
//      it floated above the opponent's head for the whole arc.
//
// The jump replay recording did not catch it: it asserted the fighter got
// airborne and threw the move, not that the move ever touched anyone. A test
// that watches the wrong noun passes forever.
#include <doctest/doctest.h>

#include "sim/sim.h"

#include "match_data.h"

using namespace ds::sim;

namespace {

constexpr InputFrame NEUTRAL{0u};
constexpr InputPair NO_INPUT{{NEUTRAL, NEUTRAL}};

const MatchData& data() {
    return ds::test::shipped_match_data();
}

// Jumps player one toward player two from `gap` units away, throws the attack
// `press_at` frames after takeoff, and reports the damage dealt.
int32_t jump_in(int32_t gap, int32_t press_at) {
    GameState state;
    init_state(state, 1u);
    while (state.round_phase != RoundPhase::Fighting) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
    }

    const int32_t centre = STAGE_WIDTH / 2;
    state.fighters[0].x = Fixed::from_int(centre - gap / 2);
    state.fighters[1].x = Fixed::from_int(centre + gap / 2);

    InputPair jump = NO_INPUT;
    jump.players[0] = input_with(NEUTRAL, Button::Up);
    advance_frame(state, data(), jump, NO_INPUT);

    for (int32_t frame = 0; frame < 80; ++frame) {
        InputPair input = NO_INPUT;
        if (frame == press_at) {
            input.players[0] = input_with(NEUTRAL, Button::HeavyPunch);
        }
        advance_frame(state, data(), input, NO_INPUT);
    }
    return STARTING_HEALTH - state.fighters[1].health;
}

}  // namespace

TEST_CASE("A jump attack can reach a standing opponent") {
    // Swept rather than spot-checked, because a single range and timing could
    // pass while the move is unusable everywhere else -- which is close to what
    // was actually true.
    int32_t connecting = 0;
    for (int32_t gap = 20; gap <= 140; gap += 10) {
        for (int32_t press_at = 0; press_at <= 30; press_at += 2) {
            if (jump_in(gap, press_at) > 0) {
                ++connecting;
            }
        }
    }

    CAPTURE(connecting);
    CHECK(connecting > 0);

    // Not merely "some combination works". A move that connects in one of two
    // hundred cases is a move nobody can use on purpose.
    CHECK(connecting > 20);
}

TEST_CASE("A jump attack deals the damage DESIGN.md 4.5 specifies") {
    const MoveData& jump_attack = move_of(data().characters[0], MoveId::JumpAttack);
    CHECK(jump_attack.damage == 7);

    bool checked = false;
    for (int32_t gap = 20; gap <= 140 && !checked; gap += 10) {
        for (int32_t press_at = 0; press_at <= 30; press_at += 2) {
            const int32_t dealt = jump_in(gap, press_at);
            if (dealt > 0) {
                CHECK(dealt == jump_attack.damage);
                checked = true;
                break;
            }
        }
    }
    CHECK(checked);
}

TEST_CASE("A jump attack still misses from far enough away") {
    // The counterpart. A hitbox large enough to connect from anywhere would
    // pass the test above while making spacing meaningless.
    CHECK(jump_in(400, 10) == 0);
}
