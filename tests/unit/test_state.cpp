// GameState and the sim's ground-movement subset. ADR 0005.
//
// The layout assertions here duplicate static_asserts in state.h on purpose:
// the static_assert fails the build, which is the right behavior, but a
// developer staring at a wall of template errors benefits from a named test
// that says what the contract is and why.
#include <cstring>
#include <type_traits>

#include <doctest/doctest.h>

#include "sim/sim.h"
#include "sim/state.h"

#include "match_data.h"

using namespace mw::sim;

namespace {

constexpr InputFrame NEUTRAL{0u};
constexpr InputPair NO_INPUT{{NEUTRAL, NEUTRAL}};

InputPair pair_with(InputFrame p1, InputFrame p2) {
    return InputPair{{p1, p2}};
}

InputFrame held(Button button) {
    return input_with(NEUTRAL, button);
}

// Runs the state past the round-start freeze so that player input is live.
void skip_to_fighting(GameState& state) {
    while (state.round_phase != RoundPhase::Fighting) {
        advance_frame(state, mw::test::shipped_match_data(), NO_INPUT, NO_INPUT);
    }
}

}  // namespace

TEST_CASE("GameState satisfies the rollback memcpy contract") {
    // Rollback saves and restores this struct up to 8x per frame. Each of
    // these is what makes that a memcpy rather than a serialization pass.
    CHECK(std::is_trivially_copyable_v<GameState>);
    CHECK(std::is_standard_layout_v<GameState>);
    CHECK(sizeof(GameState) < 4096);
}

TEST_CASE("GameState has no implicit padding") {
    // The desync test hashes this struct byte by byte across three platforms.
    // Padding bytes are uninitialized, so any implicit padding would differ
    // between machines and report a desync where behavior actually matched.
    CHECK(sizeof(Fighter) == 14 * sizeof(int32_t));
    CHECK(sizeof(Projectile) == 8 * sizeof(int32_t));
    CHECK(sizeof(GameState) == 2 * sizeof(Fighter) + MAX_PROJECTILES * sizeof(Projectile) +
                                   8 * sizeof(int32_t) + sizeof(RngState));
}

TEST_CASE("init_state produces a fully defined struct") {
    // Zeroing the whole struct, padding included, is what makes the byte-level
    // hash meaningful. This compares two independently initialized states
    // rather than inspecting fields, because it is the *bytes* that matter.
    GameState a;
    GameState b;
    std::memset(&a, 0xAA, sizeof(GameState));
    std::memset(&b, 0x55, sizeof(GameState));

    init_state(a, 12345u);
    init_state(b, 12345u);

    CHECK(std::memcmp(&a, &b, sizeof(GameState)) == 0);
}

TEST_CASE("init_state sets the opening match values") {
    GameState state;
    init_state(state, 1u);

    CHECK(state.frame == 0);
    CHECK(state.round_number == 1);
    CHECK(state.rounds_won[0] == 0);
    CHECK(state.rounds_won[1] == 0);
    CHECK(state.round_timer == ROUND_TIMER_FRAMES);
    CHECK(state.round_phase == RoundPhase::Starting);

    SUBCASE("both fighters start healthy and facing each other") {
        CHECK(state.fighters[0].health == STARTING_HEALTH);
        CHECK(state.fighters[1].health == STARTING_HEALTH);
        CHECK(state.fighters[0].facing == Facing::Right);
        CHECK(state.fighters[1].facing == Facing::Left);
        CHECK(state.fighters[0].x.to_int() == ROUND_START_X_P1);
        CHECK(state.fighters[1].x.to_int() == ROUND_START_X_P2);
    }

    SUBCASE("no projectile is live and none claims an owner") {
        for (const Projectile& projectile : state.projectiles) {
            CHECK(projectile.active == 0);
            CHECK(projectile.owner_index == -1);
        }
    }
}

TEST_CASE("A different seed changes only the RNG, not the opening position") {
    GameState a;
    GameState b;
    init_state(a, 1u);
    init_state(b, 2u);

    CHECK(a.fighters[0].x == b.fighters[0].x);
    CHECK(a.fighters[1].x == b.fighters[1].x);
    CHECK(std::memcmp(&a.rng, &b.rng, sizeof(RngState)) != 0);
}

TEST_CASE("Players have no control during the round-start freeze") {
    GameState state;
    init_state(state, 1u);
    const Fixed start_x = state.fighters[0].x;

    advance_frame(state, mw::test::shipped_match_data(), pair_with(held(Button::Right), NEUTRAL), NO_INPUT);

    CHECK(state.round_phase == RoundPhase::Starting);
    CHECK(state.fighters[0].x == start_x);
}

TEST_CASE("The round-start freeze ends and hands over control") {
    GameState state;
    init_state(state, 1u);
    skip_to_fighting(state);

    CHECK(state.round_phase == RoundPhase::Fighting);
    CHECK(state.fighters[0].state == FighterState::Idle);
    CHECK(state.fighters[1].state == FighterState::Idle);
}

TEST_CASE("Walking forward and backward use different speeds") {
    // DESIGN.md 4.4: 1.2 units/frame forward, 1.0 back. The asymmetry is a
    // feel decision, so it gets a test rather than living only in a constant.
    GameState state;
    init_state(state, 1u);
    skip_to_fighting(state);

    SUBCASE("player 1 faces right, so Right is forward") {
        const Fixed before = state.fighters[0].x;
        advance_frame(state, mw::test::shipped_match_data(), pair_with(held(Button::Right), NEUTRAL), NO_INPUT);

        CHECK(state.fighters[0].state == FighterState::WalkForward);
        CHECK(state.fighters[0].x - before == WALK_FORWARD_SPEED);
    }

    SUBCASE("and Left is backward, which is slower") {
        const Fixed before = state.fighters[0].x;
        advance_frame(state, mw::test::shipped_match_data(), pair_with(held(Button::Left), NEUTRAL), NO_INPUT);

        CHECK(state.fighters[0].state == FighterState::WalkBackward);
        CHECK(before - state.fighters[0].x == WALK_BACKWARD_SPEED);
        CHECK(WALK_BACKWARD_SPEED < WALK_FORWARD_SPEED);
    }
}

TEST_CASE("Block is a button, not hold-back") {
    // DESIGN.md 4.1. This is the whole reason walking backward and blocking
    // are never ambiguous, which removes a class of edge cases from the state
    // machine rather than handling them.
    GameState state;
    init_state(state, 1u);
    skip_to_fighting(state);

    const Fixed before = state.fighters[0].x;
    const InputFrame back_and_block = input_with(held(Button::Left), Button::Block);
    advance_frame(state, mw::test::shipped_match_data(), pair_with(back_and_block, NEUTRAL), NO_INPUT);

    CHECK(state.fighters[0].state == FighterState::Blocking);
    CHECK(state.fighters[0].x == before);
}

TEST_CASE("Fighters always face each other") {
    // DESIGN.md 4.3. Facing is derived from position every frame rather than
    // stored and flipped, so a cross-up cannot leave it stale.
    GameState state;
    init_state(state, 1u);
    skip_to_fighting(state);

    CHECK(state.fighters[0].facing == Facing::Right);

    // Teleport player 1 past player 2. Reaching across like this is a test
    // affordance; the sim itself only ever moves fighters by velocity.
    state.fighters[0].x = state.fighters[1].x + Fixed::from_int(50);
    advance_frame(state, mw::test::shipped_match_data(), NO_INPUT, NO_INPUT);

    CHECK(state.fighters[0].facing == Facing::Left);
    CHECK(state.fighters[1].facing == Facing::Right);

    SUBCASE("exactly equal positions resolve without oscillating") {
        state.fighters[0].x = state.fighters[1].x;
        advance_frame(state, mw::test::shipped_match_data(), NO_INPUT, NO_INPUT);
        const Facing first = state.fighters[0].facing;

        advance_frame(state, mw::test::shipped_match_data(), NO_INPUT, NO_INPUT);
        CHECK(state.fighters[0].facing == first);
    }
}

TEST_CASE("Fighters cannot walk out of the stage") {
    GameState state;
    init_state(state, 1u);
    skip_to_fighting(state);

    // Long enough to cross the whole stage several times over.
    for (int32_t i = 0; i < 2000; ++i) {
        advance_frame(state, mw::test::shipped_match_data(), pair_with(held(Button::Left), held(Button::Right)), NO_INPUT);
    }

    CHECK(state.fighters[0].x >= Fixed::from_int(STAGE_LEFT_BOUND));
    CHECK(state.fighters[0].x <= Fixed::from_int(STAGE_RIGHT_BOUND));
    CHECK(state.fighters[1].x >= Fixed::from_int(STAGE_LEFT_BOUND));
    CHECK(state.fighters[1].x <= Fixed::from_int(STAGE_RIGHT_BOUND));
}

TEST_CASE("The round timer counts down only while fighting") {
    GameState state;
    init_state(state, 1u);

    CHECK(state.round_timer == ROUND_TIMER_FRAMES);
    advance_frame(state, mw::test::shipped_match_data(), NO_INPUT, NO_INPUT);
    CHECK(state.round_timer == ROUND_TIMER_FRAMES);

    skip_to_fighting(state);
    const int32_t at_start = state.round_timer;
    advance_frame(state, mw::test::shipped_match_data(), NO_INPUT, NO_INPUT);
    CHECK(state.round_timer == at_start - 1);
}

TEST_CASE("A KO ends the round and awards it to the survivor") {
    GameState state;
    init_state(state, 1u);
    skip_to_fighting(state);

    state.fighters[1].health = 0;
    advance_frame(state, mw::test::shipped_match_data(), NO_INPUT, NO_INPUT);

    CHECK(state.rounds_won[0] == 1);
    CHECK(state.rounds_won[1] == 0);
    CHECK(state.round_phase == RoundPhase::Ended);
    CHECK(state.fighters[0].state == FighterState::Win);
    CHECK(state.fighters[1].state == FighterState::Lose);
}

TEST_CASE("A double KO awards the round to neither player") {
    // Otherwise both players score and a best-of-three can end 2-2.
    GameState state;
    init_state(state, 1u);
    skip_to_fighting(state);

    state.fighters[0].health = 0;
    state.fighters[1].health = 0;
    advance_frame(state, mw::test::shipped_match_data(), NO_INPUT, NO_INPUT);

    CHECK(state.rounds_won[0] == 0);
    CHECK(state.rounds_won[1] == 0);
    CHECK(state.round_phase == RoundPhase::Ended);
}

TEST_CASE("Timeout awards the round to whoever has more health") {
    // DESIGN.md 2.
    GameState state;
    init_state(state, 1u);
    skip_to_fighting(state);

    state.round_timer = 1;
    state.fighters[0].health = 40;
    state.fighters[1].health = 60;
    advance_frame(state, mw::test::shipped_match_data(), NO_INPUT, NO_INPUT);

    CHECK(state.round_timer == 0);
    CHECK(state.rounds_won[1] == 1);
    CHECK(state.rounds_won[0] == 0);
}

TEST_CASE("A new round resets health but preserves the score") {
    GameState state;
    init_state(state, 1u);
    skip_to_fighting(state);

    state.fighters[1].health = 0;
    advance_frame(state, mw::test::shipped_match_data(), NO_INPUT, NO_INPUT);
    REQUIRE(state.round_phase == RoundPhase::Ended);

    while (state.round_phase == RoundPhase::Ended) {
        advance_frame(state, mw::test::shipped_match_data(), NO_INPUT, NO_INPUT);
    }

    CHECK(state.round_number == 2);
    CHECK(state.rounds_won[0] == 1);
    CHECK(state.fighters[0].health == STARTING_HEALTH);
    CHECK(state.fighters[1].health == STARTING_HEALTH);
    CHECK(state.round_timer == ROUND_TIMER_FRAMES);
    CHECK(state.fighters[0].x.to_int() == ROUND_START_X_P1);
}

TEST_CASE("Winning two rounds ends the match") {
    // DESIGN.md 4.4: rounds to win is 2.
    GameState state;
    init_state(state, 1u);

    for (int32_t round = 0; round < ROUNDS_TO_WIN; ++round) {
        skip_to_fighting(state);
        state.fighters[1].health = 0;
        advance_frame(state, mw::test::shipped_match_data(), NO_INPUT, NO_INPUT);

        if (round + 1 < ROUNDS_TO_WIN) {
            while (state.round_phase == RoundPhase::Ended) {
                advance_frame(state, mw::test::shipped_match_data(), NO_INPUT, NO_INPUT);
            }
        }
    }

    CHECK(state.rounds_won[0] == ROUNDS_TO_WIN);
    CHECK(state.round_phase == RoundPhase::MatchEnded);

    SUBCASE("and the match state is terminal") {
        for (int32_t i = 0; i < 600; ++i) {
            advance_frame(state, mw::test::shipped_match_data(), NO_INPUT, NO_INPUT);
        }
        CHECK(state.round_phase == RoundPhase::MatchEnded);
        CHECK(state.round_number == ROUNDS_TO_WIN);
    }
}

TEST_CASE("The frame counter advances exactly once per call") {
    GameState state;
    init_state(state, 1u);

    for (int32_t i = 0; i < 100; ++i) {
        CHECK(state.frame == i);
        advance_frame(state, mw::test::shipped_match_data(), NO_INPUT, NO_INPUT);
    }
}

TEST_CASE("Reserved padding stays zero") {
    // If this ever fails, someone used the padding field as storage without
    // reading why it exists, and the byte-level hash is no longer trustworthy.
    GameState state;
    init_state(state, 1u);

    for (int32_t i = 0; i < 200; ++i) {
        advance_frame(state, mw::test::shipped_match_data(), pair_with(held(Button::Right), held(Button::Left)), NO_INPUT);
    }
    CHECK(state.reserved == 0);
}
