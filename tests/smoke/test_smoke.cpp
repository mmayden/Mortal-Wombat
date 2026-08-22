// Headless smoke test. BLUEPRINT.md 1.3.
//
// Boot the sim, run 600 ticks (10 seconds at 60Hz), assert nothing exploded
// and no state ran away. This catches the entire class of "it compiles but it
// falls over on launch", which is the most common way agent-written game code
// fails.
//
// It runs headless because the sim has no platform dependency at all — that is
// the payoff of the one-way rule in ARCHITECTURE.md section 1.
#include <chrono>
#include <cstdint>
#include <cstdio>

#include <doctest/doctest.h>

#include "sim/hash.h"
#include "sim/sim.h"

using namespace mw::sim;

namespace {

constexpr int32_t SMOKE_TICKS = 600;

constexpr InputFrame NEUTRAL{0u};
constexpr InputPair NO_INPUT{{NEUTRAL, NEUTRAL}};

// A deterministic stand-in for a player mashing buttons. Uses the frame index
// rather than an RNG so that a smoke failure is reproducible from the frame
// number alone.
InputPair scripted_input(int32_t frame) {
    InputFrame p1 = NEUTRAL;
    InputFrame p2 = NEUTRAL;

    if ((frame / 20) % 2 == 0) {
        p1 = input_with(p1, Button::Right);
    } else {
        p1 = input_with(p1, Button::Left);
    }
    if ((frame / 13) % 3 == 0) {
        p1 = input_with(p1, Button::HighPunch);
    }
    if ((frame / 17) % 2 == 0) {
        p2 = input_with(p2, Button::Block);
    } else {
        p2 = input_with(p2, Button::Left);
    }
    if ((frame / 31) % 4 == 0) {
        p2 = input_with(p2, Button::Down);
    }

    return InputPair{{p1, p2}};
}

// Every invariant that must hold on every frame of every match. Checked each
// tick rather than at the end, so a failure names the frame it happened on.
void check_invariants(const GameState& state, int32_t tick) {
    INFO("tick ", tick, " frame ", state.frame);

    REQUIRE(state.frame == tick + 1);
    REQUIRE(state.round_timer >= 0);
    REQUIRE(state.round_timer <= ROUND_TIMER_FRAMES);
    REQUIRE(state.reserved == 0);

    for (int32_t i = 0; i < 2; ++i) {
        const Fighter& fighter = state.fighters[i];
        REQUIRE(fighter.health >= 0);
        REQUIRE(fighter.health <= STARTING_HEALTH);
        REQUIRE(fighter.x >= Fixed::from_int(STAGE_LEFT_BOUND));
        REQUIRE(fighter.x <= Fixed::from_int(STAGE_RIGHT_BOUND));
        REQUIRE(fighter.hitstun_remaining >= 0);
        REQUIRE(fighter.blockstun_remaining >= 0);
        REQUIRE(state.rounds_won[i] <= ROUNDS_TO_WIN);
    }

    for (const Projectile& projectile : state.projectiles) {
        REQUIRE((projectile.active == 0 || projectile.active == 1));
        REQUIRE(projectile.owner_index >= -1);
        REQUIRE(projectile.owner_index < 2);
    }
}

}  // namespace

TEST_CASE("The sim survives 600 ticks of idle input") {
    GameState state;
    init_state(state, 20260822u);

    for (int32_t tick = 0; tick < SMOKE_TICKS; ++tick) {
        advance_frame(state, NO_INPUT, NO_INPUT);
        check_invariants(state, tick);
    }

    CHECK(state.frame == SMOKE_TICKS);
}

TEST_CASE("The sim survives 600 ticks of scripted input") {
    GameState state;
    init_state(state, 20260822u);

    InputPair previous = NO_INPUT;
    for (int32_t tick = 0; tick < SMOKE_TICKS; ++tick) {
        const InputPair current = scripted_input(tick);
        advance_frame(state, current, previous);
        check_invariants(state, tick);
        previous = current;
    }

    CHECK(state.frame == SMOKE_TICKS);
}

TEST_CASE("The sim survives a full match played to its conclusion") {
    // Exercises the round transitions, which idle ticks never reach: a 90
    // second round timer is 5400 frames, so a smoke run alone never sees one.
    GameState state;
    init_state(state, 99u);

    int32_t tick = 0;
    const int32_t limit = 60 * 60 * 10;  // ten minutes of frames

    while (state.round_phase != RoundPhase::MatchEnded && tick < limit) {
        // Force a KO periodically rather than waiting out three 90-second
        // rounds, which would put this test far outside its time budget.
        if (state.round_phase == RoundPhase::Fighting && state.frame % 200 == 0) {
            state.fighters[1].health = 0;
        }
        advance_frame(state, scripted_input(tick), scripted_input(tick - 1));
        ++tick;
    }

    CHECK(state.round_phase == RoundPhase::MatchEnded);
    CHECK(state.rounds_won[0] == ROUNDS_TO_WIN);
    CHECK(tick < limit);
}

TEST_CASE("600 ticks stay well inside the frame budget") {
    // BLUEPRINT.md 1.3, step 5. The sim gets 16.67ms per frame minus whatever
    // rendering needs, and rollback may re-simulate up to 8 frames within one
    // of those. The assertion is deliberately loose — it is a guard against a
    // catastrophic regression, not a benchmark. Profiling belongs in Tracy
    // under the profile preset (ADR 0012).
    GameState state;
    init_state(state, 1u);

    // Timing a wall clock here is fine: this is a test, which lives above the
    // sim boundary. Nothing inside src/sim/ may do this (ADR 0002).
    const auto start = std::chrono::steady_clock::now();
    for (int32_t tick = 0; tick < SMOKE_TICKS; ++tick) {
        advance_frame(state, scripted_input(tick), scripted_input(tick - 1));
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;

    const int64_t micros =
        std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    const double per_frame_micros = static_cast<double>(micros) / SMOKE_TICKS;

    std::printf("smoke: %d ticks in %lld us (%.2f us/frame)\n", SMOKE_TICKS,
                static_cast<long long>(micros), per_frame_micros);

    // 1ms per frame is roughly 16x the sim's share of a 60Hz budget. Crossing
    // it means something is very wrong, not that a machine was busy.
    CHECK(per_frame_micros < 1000.0);
}
