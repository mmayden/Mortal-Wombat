// Jumping. DESIGN.md 4.3 and 4.4.
//
// The defining property is commitment: the trajectory is fixed at takeoff and
// nothing the player does afterwards changes it. DESIGN.md 3 makes that the
// feel target for the whole game -- "committing to an action means committing"
// -- so most of this file is about what CANNOT happen mid-air.
#include <cstring>

#include <doctest/doctest.h>

#include "sim/sim.h"

#include "match_data.h"

using namespace ds::sim;

namespace {

constexpr InputFrame NEUTRAL{0u};
constexpr InputPair NO_INPUT{{NEUTRAL, NEUTRAL}};

InputFrame held(Button button) {
    return input_with(NEUTRAL, button);
}
InputPair pair_with(InputFrame p1, InputFrame p2) {
    return InputPair{{p1, p2}};
}
const MatchData& data() {
    return ds::test::shipped_match_data();
}

GameState fighting_state() {
    GameState state;
    init_state(state, 1u);
    while (state.round_phase != RoundPhase::Fighting) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
    }
    return state;
}

// Jumps player one and runs until they are airborne.
void takeoff(GameState& state, InputFrame jump_input) {
    advance_frame(state, data(), pair_with(jump_input, NEUTRAL), NO_INPUT);
    REQUIRE(state.fighters[0].state == FighterState::JumpStartup);

    while (state.fighters[0].state == FighterState::JumpStartup) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
    }
    REQUIRE(state.fighters[0].state == FighterState::Airborne);
}

}  // namespace

TEST_CASE("Up starts a jump") {
    GameState state = fighting_state();
    advance_frame(state, data(), pair_with(held(Button::Up), NEUTRAL), NO_INPUT);

    CHECK(state.fighters[0].state == FighterState::JumpStartup);
    CHECK(state.fighters[0].y == Fixed::from_int(GROUND_Y));
}

TEST_CASE("A crouching fighter can jump without releasing down") {
    // The SOCD case. A keyboard or leverless player holds Down to crouch and
    // presses Up to jump without letting go, which a gamepad cannot produce.
    // Before Up priority this resolved to neutral and the jump never came out.
    GameState state = fighting_state();

    advance_frame(state, data(), pair_with(held(Button::Down), NEUTRAL), NO_INPUT);
    REQUIRE(state.fighters[0].state == FighterState::Crouch);

    const InputFrame down_and_up = input_with(held(Button::Down), Button::Up);
    advance_frame(state, data(), pair_with(down_and_up, NEUTRAL),
                  pair_with(held(Button::Down), NEUTRAL));

    CHECK(state.fighters[0].state == FighterState::JumpStartup);
}

TEST_CASE("The fighter stays grounded through jump startup") {
    // The commitment window. A fighter in startup is still a grounded target,
    // which is what makes a jump something an opponent can react to.
    GameState state = fighting_state();
    advance_frame(state, data(), pair_with(held(Button::Up), NEUTRAL), NO_INPUT);

    for (int32_t i = 0; i < JUMP_STARTUP_FRAMES - 1; ++i) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
        CHECK(state.fighters[0].y == Fixed::from_int(GROUND_Y));
    }
}

TEST_CASE("A jump reaches roughly the apex DESIGN.md 4.4 specifies") {
    // Gravity is derived from jump_duration and jump_apex rather than being its
    // own constant, so this checks the derivation, not a magic number. The
    // tolerance covers fixed-point rounding over 22 frames of accumulation.
    GameState state = fighting_state();
    takeoff(state, held(Button::Up));

    Fixed highest = state.fighters[0].y;
    while (state.fighters[0].state == FighterState::Airborne) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
        highest = fixed_min(highest, state.fighters[0].y);
        REQUIRE(state.frame < 400);
    }

    const int32_t apex_units = Fixed::from_int(GROUND_Y).to_int() - highest.to_int();
    const int32_t expected = data().characters[0].jump_apex;

    CAPTURE(apex_units);
    CAPTURE(expected);
    CHECK(apex_units >= expected - 3);
    CHECK(apex_units <= expected + 3);
}

TEST_CASE("A jump lasts roughly the duration DESIGN.md 4.4 specifies") {
    GameState state = fighting_state();
    takeoff(state, held(Button::Up));

    int32_t airborne_frames = 0;
    while (state.fighters[0].state == FighterState::Airborne) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
        ++airborne_frames;
        REQUIRE(airborne_frames < 200);
    }

    const int32_t expected = data().characters[0].jump_duration;
    CAPTURE(airborne_frames);
    CAPTURE(expected);
    CHECK(airborne_frames >= expected - 3);
    CHECK(airborne_frames <= expected + 3);
}

TEST_CASE("A fighter always returns to exactly the ground plane") {
    // Landing is detected by position, not by counting frames, so the arc and
    // the floor cannot disagree. Snapping to GROUND_Y also means a jump leaves
    // no drift: a hundred jumps end at the same height as the first.
    GameState state = fighting_state();

    for (int32_t jump = 0; jump < 3; ++jump) {
        takeoff(state, held(Button::Up));
        while (state.fighters[0].state != FighterState::Idle) {
            advance_frame(state, data(), NO_INPUT, NO_INPUT);
            REQUIRE(state.frame < 1000);
        }
        CHECK(state.fighters[0].y == Fixed::from_int(GROUND_Y));
    }
}

TEST_CASE("There is no air control") {
    // DESIGN.md 4.3, and the single most important property here. Two jumps
    // from the same position with wildly different mid-air input must land in
    // exactly the same place.
    auto jump_with_mid_air_input = [](Button mid_air) {
        GameState state = fighting_state();
        takeoff(state, held(Button::Up));

        while (state.fighters[0].state == FighterState::Airborne) {
            advance_frame(state, data(), pair_with(held(mid_air), NEUTRAL), NO_INPUT);
        }
        return state.fighters[0].x;
    };

    const Fixed pushing_left = jump_with_mid_air_input(Button::Left);
    const Fixed pushing_right = jump_with_mid_air_input(Button::Right);

    CHECK(pushing_left == pushing_right);
}

TEST_CASE("Jump direction is committed at takeoff") {
    GameState state = fighting_state();
    const Fixed start = state.fighters[0].x;

    // Player one faces right, so Right is a forward jump.
    takeoff(state, input_with(held(Button::Up), Button::Right));
    while (state.fighters[0].state != FighterState::Idle) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
        REQUIRE(state.frame < 400);
    }

    CHECK(state.fighters[0].x > start);

    SUBCASE("and a forward jump travels further than walking the same frames") {
        // Otherwise a forward jump is strictly worse than walking: same
        // distance, no ability to block, and a landing recovery.
        const Fixed travelled = state.fighters[0].x - start;
        const Fixed walked =
            data().characters[0].walk_forward_speed * data().characters[0].jump_duration;
        CHECK(travelled > walked);
    }
}

TEST_CASE("A neutral jump does not drift") {
    GameState state = fighting_state();
    const Fixed start = state.fighters[0].x;

    takeoff(state, held(Button::Up));
    while (state.fighters[0].state != FighterState::Idle) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
        REQUIRE(state.frame < 400);
    }

    CHECK(state.fighters[0].x == start);
}

TEST_CASE("A jump cannot be cancelled or repeated in the air") {
    // No double jump, no jump cancel (DESIGN.md 4.3).
    GameState state = fighting_state();
    takeoff(state, held(Button::Up));

    int32_t airborne = 0;
    while (state.fighters[0].state == FighterState::Airborne) {
        // Mash up throughout.
        advance_frame(state, data(), pair_with(held(Button::Up), NEUTRAL), NO_INPUT);
        CHECK(state.fighters[0].state != FighterState::JumpStartup);
        ++airborne;
        REQUIRE(airborne < 200);
    }
}

TEST_CASE("An attack in the air produces the jump attack") {
    GameState state = fighting_state();
    takeoff(state, held(Button::Up));

    advance_frame(state, data(), pair_with(held(Button::HeavyPunch), NEUTRAL), NO_INPUT);
    CHECK(state.fighters[0].move_id == static_cast<int32_t>(MoveId::JumpAttack));

    SUBCASE("and only one, however much the player mashes") {
        const int32_t first_frame = state.fighters[0].move_frame;
        advance_frame(state, data(), pair_with(held(Button::LightKick), NEUTRAL), NO_INPUT);
        CHECK(state.fighters[0].move_id == static_cast<int32_t>(MoveId::JumpAttack));
        CHECK(state.fighters[0].move_frame == first_frame + 1);
    }
}

TEST_CASE("The jump attack ends on landing regardless of its frame count") {
    // DESIGN.md 4.5 gives its active window as "until landing", which the
    // schema cannot express as an integer -- so the rule lives in the sim.
    GameState state = fighting_state();
    takeoff(state, held(Button::Up));
    advance_frame(state, data(), pair_with(held(Button::HeavyPunch), NEUTRAL), NO_INPUT);
    REQUIRE(state.fighters[0].move_id == static_cast<int32_t>(MoveId::JumpAttack));

    while (state.fighters[0].state == FighterState::Airborne) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
        REQUIRE(state.frame < 400);
    }

    CHECK(state.fighters[0].state == FighterState::Landing);
    CHECK(state.fighters[0].move_id == -1);
}

TEST_CASE("Landing recovery blocks action for its documented duration") {
    GameState state = fighting_state();
    takeoff(state, held(Button::Up));

    while (state.fighters[0].state == FighterState::Airborne) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
        REQUIRE(state.frame < 400);
    }
    REQUIRE(state.fighters[0].state == FighterState::Landing);

    int32_t recovering = 0;
    while (state.fighters[0].state == FighterState::Landing) {
        // Mashing punch during recovery must produce nothing.
        advance_frame(state, data(), pair_with(held(Button::HeavyPunch), NEUTRAL), NO_INPUT);
        CHECK(state.fighters[0].state != FighterState::Attack);
        ++recovering;
        REQUIRE(recovering < 60);
    }

    CHECK(recovering == LANDING_FRAMES);
}

TEST_CASE("Jumping is deterministic") {
    auto run = [] {
        GameState state = fighting_state();
        for (int32_t i = 0; i < 200; ++i) {
            const bool jump = (i % 60) == 0;
            const bool attack = (i % 60) == 20;
            const InputFrame p1 = jump     ? input_with(held(Button::Up), Button::Right)
                                  : attack ? held(Button::HeavyPunch)
                                           : NEUTRAL;
            advance_frame(state, data(), pair_with(p1, NEUTRAL), NO_INPUT);
        }
        return state;
    };

    const GameState a = run();
    const GameState b = run();
    CHECK(std::memcmp(&a, &b, sizeof(GameState)) == 0);
}
