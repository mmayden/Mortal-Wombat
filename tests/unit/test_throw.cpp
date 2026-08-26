// Throws, and the triangle they close. ADR 0029.
//
// Attack heights gave offence an axis but left one hole open: a defender who
// crouch-blocks and simply waits. Crouching covers lows and mids, and the only
// overhead is a jump attack that can be seen coming from across the screen.
// Waiting was close to free.
//
// A throw is the answer, and the reason it works is that it cannot be blocked
// at all. That makes the rock-paper-scissors the genre runs on:
//
//     attack beats throw  (throws are short and slow to recover)
//     throw  beats block  (unblockable)
//     block  beats attack (that is what blocking is)
//
// The last case in this file asserts that triangle directly. If any edge of it
// ever breaks, one option becomes strictly correct and the guessing stops.

#include <utility>

#include <doctest/doctest.h>

#include "sim/constants.h"
#include "sim/sim.h"
#include "sim/state.h"

#include "match_data.h"

using namespace ds::sim;

namespace {

constexpr InputFrame NEUTRAL{0u};
constexpr InputPair NO_INPUT{{NEUTRAL, NEUTRAL}};

InputFrame held(Button button) {
    return input_with(NEUTRAL, button);
}

const MatchData& data() {
    return ds::test::shipped_match_data();
}

// Light punch and light kick together -- the genre's convention, and it costs
// no button.
const InputFrame THROW_INPUT = input_with(held(Button::LightPunch), Button::LightKick);

void skip_to_fighting(GameState& state) {
    while (state.round_phase != RoundPhase::Fighting) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
    }
}

// Close enough for a throw, which is deliberately short-ranged.
GameState fighting_state(int32_t separation = 30) {
    GameState state;
    init_state(state, 1u);
    skip_to_fighting(state);
    state.fighters[0].x = Fixed::from_int(STAGE_WIDTH / 2 - separation / 2);
    state.fighters[1].x = Fixed::from_int(STAGE_WIDTH / 2 + separation / 2);
    return state;
}

// Player one throws while player two holds `guard`. Returns player two's health
// loss over the move.
int32_t throw_damage(InputFrame guard, int32_t separation = 30) {
    GameState state = fighting_state(separation);
    const int32_t before = state.fighters[1].health;

    advance_frame(state, data(), InputPair{{THROW_INPUT, guard}}, NO_INPUT);

    const MoveData& md = move_of(data().characters[0], MoveId::Throw);
    const InputPair hold{{NEUTRAL, guard}};
    for (int32_t i = 0; i < move_total_frames(md) + 2; ++i) {
        advance_frame(state, data(), hold, hold);
    }
    return before - state.fighters[1].health;
}

// Back for player two is Right: they start on the right, facing left.
const InputFrame STAND_BLOCK = held(Button::Right);
const InputFrame CROUCH_BLOCK = input_with(held(Button::Right), Button::Down);

}  // namespace

TEST_CASE("Light punch and light kick together produce a throw, not a punch") {
    GameState state = fighting_state();
    advance_frame(state, data(), InputPair{{THROW_INPUT, NEUTRAL}}, NO_INPUT);

    // The binding table returns on its first match and light punch is first, so
    // this only works because the throw is checked ahead of it.
    CHECK(state.fighters[0].state == FighterState::Attack);
    CHECK(state.fighters[0].move_id == static_cast<int32_t>(MoveId::Throw));
}

TEST_CASE("Either light button alone is still a normal") {
    for (const auto& pair : {std::pair{Button::LightPunch, MoveId::StandLightPunch},
                             std::pair{Button::LightKick, MoveId::StandLightKick}}) {
        GameState state = fighting_state();
        advance_frame(state, data(), InputPair{{held(pair.first), NEUTRAL}}, NO_INPUT);
        CHECK(state.fighters[0].move_id == static_cast<int32_t>(pair.second));
    }
}

// THE POINT. A throw that could be blocked would answer nothing.
TEST_CASE("A throw cannot be blocked, in either stance") {
    const MoveData& md = move_of(data().characters[0], MoveId::Throw);

    CHECK(throw_damage(STAND_BLOCK) == md.damage);
    CHECK(throw_damage(CROUCH_BLOCK) == md.damage);
    CHECK(throw_damage(NEUTRAL) == md.damage);
}

TEST_CASE("A throw reaches barely, so it only works up close") {
    const MoveData& md = move_of(data().characters[0], MoveId::Throw);

    CHECK(throw_damage(NEUTRAL, 30) == md.damage);
    // At normal poking range it catches nothing. Short reach is the price of
    // being unblockable.
    CHECK(throw_damage(NEUTRAL, 120) == 0);
}

TEST_CASE("A throw knocks down softly, so it does not loop into itself") {
    GameState state = fighting_state();
    advance_frame(state, data(), InputPair{{THROW_INPUT, NEUTRAL}}, NO_INPUT);

    const MoveData& md = move_of(data().characters[0], MoveId::Throw);
    for (int32_t i = 0; i < move_total_frames(md); ++i) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
    }

    REQUIRE(state.fighters[1].state == FighterState::Knockdown);
    // SOFT, deliberately. Convention says a throw is a hard knockdown, but
    // convention also lets a defender tech the throw and this game does not
    // (ADR 0029) -- so the reward is the weaker knockdown, and the defender
    // keeps the timing choice on the way up.
    CHECK(state.fighters[1].knockdown_hard == 0);
}

TEST_CASE("Jumping beats a throw") {
    GameState state = fighting_state();

    // Player two leaves the ground; player one throws at where they were.
    advance_frame(state, data(), InputPair{{NEUTRAL, held(Button::Up)}}, NO_INPUT);
    for (int32_t i = 0; i < 6; ++i) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
    }
    REQUIRE(state.fighters[1].state == FighterState::Airborne);

    const int32_t before = state.fighters[1].health;
    advance_frame(state, data(), InputPair{{THROW_INPUT, NEUTRAL}}, NO_INPUT);
    const MoveData& md = move_of(data().characters[0], MoveId::Throw);
    for (int32_t i = 0; i < move_total_frames(md); ++i) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
    }

    // The third corner of the triangle. Without it, a throw would answer
    // everything and there would be no reason ever to press anything else.
    CHECK(state.fighters[1].health == before);
}

TEST_CASE("A whiffed throw is punishable, which is what makes it a commitment") {
    const MoveData& thrown = move_of(data().characters[0], MoveId::Throw);
    const MoveData& light = move_of(data().characters[0], MoveId::StandLightPunch);

    // Recovery long enough that being wrong costs a turn. If a throw whiffed
    // safely it would be free, and free is the one thing DESIGN 3 does not
    // allow an option to be.
    CHECK(thrown.recovery > light.recovery + light.startup);
}

// THE TRIANGLE, stated as behaviour.
TEST_CASE("No single defensive choice answers both an attack and a throw") {
    const MoveData& sweep = move_of(data().characters[0], MoveId::CrouchHeavyKick);

    // Crouch-blocking stops the sweep...
    GameState blocked = fighting_state();
    const int32_t blocked_before = blocked.fighters[1].health;
    const InputFrame sweep_input = input_with(held(Button::HeavyKick), Button::Down);
    advance_frame(blocked, data(), InputPair{{sweep_input, CROUCH_BLOCK}}, NO_INPUT);
    for (int32_t i = 0; i < move_total_frames(sweep); ++i) {
        advance_frame(blocked, data(), InputPair{{NEUTRAL, CROUCH_BLOCK}},
                      InputPair{{NEUTRAL, CROUCH_BLOCK}});
    }
    CHECK(blocked.fighters[1].health == blocked_before);

    // ...and does nothing at all about a throw.
    CHECK(throw_damage(CROUCH_BLOCK) > 0);
}
