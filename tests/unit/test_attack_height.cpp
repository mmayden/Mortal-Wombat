// Attack heights, and the guessing they create. ADR 0028.
//
// Before this, holding back answered every attack in the game. Blocking was a
// single decision made once, and offence had no way through a defender who
// simply held a direction -- which meant the knockdown loop, the thing the
// research calls the engine, opened into a room with nothing in it.
//
// The load-bearing property is the LAST case in this file: there must exist a
// pair of attacks that cannot both be blocked by the same stance. That is what
// a mixup IS, stated as an assertion, and it is the reason the other cases
// matter rather than being arbitrary rules about kicks.

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

void skip_to_fighting(GameState& state) {
    while (state.round_phase != RoundPhase::Fighting) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
    }
}

GameState fighting_state(int32_t separation = 40) {
    GameState state;
    init_state(state, 1u);
    skip_to_fighting(state);
    state.fighters[0].x = Fixed::from_int(STAGE_WIDTH / 2 - separation / 2);
    state.fighters[1].x = Fixed::from_int(STAGE_WIDTH / 2 + separation / 2);
    return state;
}

// Player one throws `attack` while player two holds `guard` throughout.
// Returns true if player two blocked it -- took no damage while being hit at.
bool blocks(InputFrame attack, InputFrame guard, MoveId move) {
    GameState state = fighting_state();
    const int32_t health = state.fighters[1].health;

    advance_frame(state, data(), InputPair{{attack, guard}}, NO_INPUT);

    const MoveData& md = move_of(data().characters[0], move);
    const InputPair hold{{NEUTRAL, guard}};
    for (int32_t i = 0; i < move_total_frames(md) + 2; ++i) {
        advance_frame(state, data(), hold, hold);
    }

    return state.fighters[1].health == health;
}

// Back for player two is Right: they start on the right, facing left.
const InputFrame STAND_BLOCK = held(Button::Right);
const InputFrame CROUCH_BLOCK = input_with(held(Button::Right), Button::Down);

}  // namespace

TEST_CASE("The shipped heights follow the genre convention") {
    const CharacterData& c = data().characters[0];

    // Crouching kicks are lows; jump attacks are overheads. Universal enough
    // that a player arriving from any other 2D fighter already knows it.
    CHECK(move_of(c, MoveId::CrouchLightKick).height == AttackHeight::Low);
    CHECK(move_of(c, MoveId::CrouchMediumKick).height == AttackHeight::Low);
    CHECK(move_of(c, MoveId::CrouchHeavyKick).height == AttackHeight::Low);
    CHECK(move_of(c, MoveId::JumpAttack).height == AttackHeight::Overhead);

    // Everything else is mid, including crouching PUNCHES -- the distinction
    // is the limb, not the stance, and getting that backwards is the most
    // common way to misread this table.
    CHECK(move_of(c, MoveId::StandLightPunch).height == AttackHeight::Mid);
    CHECK(move_of(c, MoveId::StandHeavyKick).height == AttackHeight::Mid);
    CHECK(move_of(c, MoveId::CrouchHeavyPunch).height == AttackHeight::Mid);
}

TEST_CASE("A mid is blocked by either stance") {
    CHECK(blocks(held(Button::LightPunch), STAND_BLOCK, MoveId::StandLightPunch));
    CHECK(blocks(held(Button::LightPunch), CROUCH_BLOCK, MoveId::StandLightPunch));
}

TEST_CASE("A low must be blocked crouching") {
    const InputFrame sweep = input_with(held(Button::HeavyKick), Button::Down);

    CHECK(blocks(sweep, CROUCH_BLOCK, MoveId::CrouchHeavyKick));
    // Standing through a sweep is the whole point of a low existing.
    CHECK_FALSE(blocks(sweep, STAND_BLOCK, MoveId::CrouchHeavyKick));
}

TEST_CASE("Guarding the wrong stance is not guarding at all") {
    // A defender who holds back but crouches into an overhead is HIT, not
    // chipped or partially protected. Half-blocking is a mechanic this game
    // does not have and should not acquire by accident.
    GameState state = fighting_state();
    const int32_t health = state.fighters[1].health;

    const InputFrame sweep = input_with(held(Button::HeavyKick), Button::Down);
    advance_frame(state, data(), InputPair{{sweep, STAND_BLOCK}}, NO_INPUT);

    const MoveData& md = move_of(data().characters[0], MoveId::CrouchHeavyKick);
    const InputPair hold{{NEUTRAL, STAND_BLOCK}};
    for (int32_t i = 0; i < move_total_frames(md); ++i) {
        advance_frame(state, data(), hold, hold);
    }

    CHECK(state.fighters[1].health < health);
    CHECK(state.fighters[1].health == health - md.damage);
}

TEST_CASE("Not guarding at all is still not guarding, whatever the stance") {
    // Crouching is not blocking. Only holding BACK is (ADR 0021), and it would
    // be easy to write the stance check in a way that let a plain crouch stop
    // a low.
    const InputFrame sweep = input_with(held(Button::HeavyKick), Button::Down);
    CHECK_FALSE(blocks(sweep, held(Button::Down), MoveId::CrouchHeavyKick));
    CHECK_FALSE(blocks(held(Button::LightPunch), NEUTRAL, MoveId::StandLightPunch));
}

// THE POINT OF ALL OF IT.
//
// A mixup is two attacks that one stance cannot both answer. If no such pair
// exists, blocking is a decision made once and offence has no way in -- and
// the knockdown loop leads somewhere empty.
TEST_CASE("There exists a genuine mixup: no single stance answers everything") {
    const CharacterData& c = data().characters[0];

    bool has_low = false;
    bool has_overhead = false;
    for (int32_t m = 0; m < MOVE_COUNT; ++m) {
        const AttackHeight height = move_of(c, static_cast<MoveId>(m)).height;
        has_low = has_low || height == AttackHeight::Low;
        has_overhead = has_overhead || height == AttackHeight::Overhead;
    }

    REQUIRE(has_low);
    REQUIRE(has_overhead);

    // Stated as behaviour rather than as data: a standing block stops the
    // overhead and not the low, and crouching does the reverse. Neither stance
    // is safe, which is what forces the guess.
    const InputFrame sweep = input_with(held(Button::HeavyKick), Button::Down);
    CHECK(blocks(sweep, CROUCH_BLOCK, MoveId::CrouchHeavyKick));
    CHECK_FALSE(blocks(sweep, STAND_BLOCK, MoveId::CrouchHeavyKick));
}
