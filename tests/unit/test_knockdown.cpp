// Knockdown, the rise, and the wakeup choice. ADR 0026.
//
// The research calls this the engine: it is what makes landing one hit worth
// more than the damage it dealt. Two of the sixteen states in DESIGN.md 4.2
// were unreachable until it existed.
//
// The load-bearing case is "a grounded fighter cannot be hit". That single rule
// is what creates okizeme -- an attacker who could simply keep hitting has
// nothing to set up -- and it is the one somebody will eventually be tempted to
// relax for an OTG combo.

#include <doctest/doctest.h>

#include "sim/constants.h"
#include "sim/sim.h"
#include "sim/state.h"

#include "data/framedata_loader.h"
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

void skip_to_fighting(GameState& state) {
    while (state.round_phase != RoundPhase::Fighting) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
    }
}

// Both fighters on the ground, close enough for anything to reach.
GameState fighting_state(int32_t separation = 40) {
    GameState state;
    init_state(state, 1u);
    skip_to_fighting(state);
    state.fighters[0].x = Fixed::from_int(STAGE_WIDTH / 2 - separation / 2);
    state.fighters[1].x = Fixed::from_int(STAGE_WIDTH / 2 + separation / 2);
    return state;
}

}  // namespace

TEST_CASE("The sweep is a hard knockdown and a light punch is not") {
    // The assignment follows the convention nearly every 2D fighter shares.
    // If this changes it is a balance decision, not a bug.
    CHECK(move_of(data().characters[0], MoveId::CrouchHeavyKick).knockdown == KnockdownKind::Hard);
    CHECK(move_of(data().characters[0], MoveId::Special).knockdown == KnockdownKind::Hard);
    CHECK(move_of(data().characters[0], MoveId::StandLightPunch).knockdown == KnockdownKind::None);
}

TEST_CASE("A sweep puts the defender on the floor") {
    GameState state = fighting_state();

    const InputPair sweep = pair_with(input_with(held(Button::HeavyKick), Button::Down), NEUTRAL);
    advance_frame(state, data(), sweep, NO_INPUT);

    const MoveData& md = move_of(data().characters[0], MoveId::CrouchHeavyKick);
    for (int32_t i = 0; i < move_total_frames(md); ++i) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
    }

    CHECK(state.fighters[1].state == FighterState::Knockdown);
    CHECK(state.fighters[1].knockdown_hard == 1);
    // Knockdown replaces hitstun rather than following it -- the floor IS the
    // stun, and serving both would punish one hit twice.
    CHECK(state.fighters[1].hitstun_remaining == 0);
}

// THE LOAD-BEARING RULE. This is what creates okizeme, and it is the one a
// future OTG combo would quietly break.
TEST_CASE("A fighter on the floor cannot be hit") {
    GameState state = fighting_state();

    // Put player two down directly, so the test is about the rule rather than
    // about landing a sweep.
    state.fighters[1].state = FighterState::Knockdown;
    state.fighters[1].state_frames_remaining = KNOCKDOWN_FRAMES;
    const int32_t health = state.fighters[1].health;

    // Player one attacks repeatedly for the whole knockdown.
    for (int32_t i = 0; i < KNOCKDOWN_FRAMES; ++i) {
        const InputPair attack = pair_with(held(Button::LightPunch), NEUTRAL);
        advance_frame(state, data(), attack, NO_INPUT);
    }

    CHECK(state.fighters[1].health == health);
}

TEST_CASE("The rise is invulnerable too") {
    GameState state = fighting_state();
    state.fighters[1].state = FighterState::Wakeup;
    state.fighters[1].state_frames_remaining = WAKEUP_FRAMES;
    const int32_t health = state.fighters[1].health;

    for (int32_t i = 0; i < WAKEUP_FRAMES; ++i) {
        const InputPair attack = pair_with(held(Button::LightPunch), NEUTRAL);
        advance_frame(state, data(), attack, NO_INPUT);
    }

    // Without this a perfectly-timed attack hits a defender with no option at
    // all, which is not a coin flip -- rigged or otherwise.
    CHECK(state.fighters[1].health == health);
}

TEST_CASE("A soft knockdown lets the defender delay the rise") {
    GameState quick = fighting_state();
    GameState slow = fighting_state();

    for (GameState* state : {&quick, &slow}) {
        state->fighters[1].state = FighterState::Knockdown;
        state->fighters[1].state_frames_remaining = KNOCKDOWN_FRAMES;
        state->fighters[1].knockdown_hard = 0;
        state->fighters[1].wakeup_delayed = 0;
    }

    const InputPair rise_now = pair_with(NEUTRAL, NEUTRAL);
    const InputPair rise_late = pair_with(NEUTRAL, held(Button::Down));

    int32_t quick_frames = 0;
    while (quick.fighters[1].state != FighterState::Idle && quick_frames < 300) {
        advance_frame(quick, data(), rise_now, rise_now);
        ++quick_frames;
    }

    int32_t slow_frames = 0;
    while (slow.fighters[1].state != FighterState::Idle && slow_frames < 300) {
        advance_frame(slow, data(), rise_late, rise_late);
        ++slow_frames;
    }

    // The attacker must cover two timings. If these were equal there would be
    // no choice, and the knockdown would be a script rather than a guess.
    CHECK(slow_frames > quick_frames);
    CHECK(slow_frames - quick_frames == WAKEUP_DELAY_FRAMES);
}

TEST_CASE("A hard knockdown ignores the delay input") {
    GameState state = fighting_state();
    state.fighters[1].state = FighterState::Knockdown;
    state.fighters[1].state_frames_remaining = KNOCKDOWN_FRAMES;
    state.fighters[1].knockdown_hard = 1;

    const InputPair try_delay = pair_with(NEUTRAL, held(Button::Down));

    int32_t frames = 0;
    while (state.fighters[1].state != FighterState::Idle && frames < 300) {
        advance_frame(state, data(), try_delay, try_delay);
        ++frames;
    }

    // Fixed timing is the whole reward for landing the harder move.
    CHECK(frames == KNOCKDOWN_FRAMES + WAKEUP_FRAMES + 1);
}

TEST_CASE("The delay can only be taken once") {
    GameState state = fighting_state();
    state.fighters[1].state = FighterState::Knockdown;
    state.fighters[1].state_frames_remaining = KNOCKDOWN_FRAMES;
    state.fighters[1].knockdown_hard = 0;

    const InputPair hold_down = pair_with(NEUTRAL, held(Button::Down));

    int32_t frames = 0;
    while (state.fighters[1].state != FighterState::Idle && frames < 600) {
        advance_frame(state, data(), hold_down, hold_down);
        ++frames;
    }

    // Holding down forever must not mean never being vulnerable. That is not a
    // choice, it is a hiding place.
    CHECK(frames < 600);
    CHECK(frames == KNOCKDOWN_FRAMES + WAKEUP_DELAY_FRAMES + WAKEUP_FRAMES + 1);
}

TEST_CASE("Crouch-blocking a sweep does not knock you down") {
    GameState state = fighting_state();

    // Down-back: the sweep is a LOW and a standing block does not cover it
    // (ADR 0028). Back for player two is Right.
    //
    // This case used to hold Right alone and passed, because height did not
    // exist. It failing was the feature arriving.
    const InputFrame guard = input_with(held(Button::Right), Button::Down);
    const InputPair sweep = pair_with(input_with(held(Button::HeavyKick), Button::Down), guard);
    advance_frame(state, data(), sweep, NO_INPUT);

    const MoveData& md = move_of(data().characters[0], MoveId::CrouchHeavyKick);
    for (int32_t i = 0; i < move_total_frames(md); ++i) {
        advance_frame(state, data(), pair_with(NEUTRAL, guard), pair_with(NEUTRAL, guard));
    }

    // Blocking is supposed to be the thing that stops this happening to you.
    CHECK(state.fighters[1].state != FighterState::Knockdown);
    CHECK(state.fighters[1].health == STARTING_HEALTH);
}
