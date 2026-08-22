#include "sim/sim.h"

#include <cstring>

namespace mw::sim {
namespace {

// ROUND_START_FREEZE_FRAMES and ROUND_END_FREEZE_FRAMES live in constants.h,
// in the provisional block — no design document specifies round pacing, and
// keeping every unspecified value in one place is what makes the list of open
// decisions readable. See ADR 0015.

constexpr Fixed STAGE_LEFT = Fixed::from_int(STAGE_LEFT_BOUND);
constexpr Fixed STAGE_RIGHT = Fixed::from_int(STAGE_RIGHT_BOUND);

// Step 2 of ARCHITECTURE.md section 5. Fighters always face each other, and
// facing flips when they cross up (DESIGN.md 4.3).
//
// Ties resolve by player index rather than by position, because at exactly
// equal x there is no positional answer and an arbitrary-but-fixed rule is
// what keeps two machines agreeing.
void resolve_facing(GameState& state) {
    const Fixed x0 = state.fighters[0].x;
    const Fixed x1 = state.fighters[1].x;

    if (x0 == x1) {
        state.fighters[0].facing = Facing::Right;
        state.fighters[1].facing = Facing::Left;
        return;
    }

    const bool p0_is_left = x0 < x1;
    state.fighters[0].facing = p0_is_left ? Facing::Right : Facing::Left;
    state.fighters[1].facing = p0_is_left ? Facing::Left : Facing::Right;
}

// Steps 3 and 4. The v1 state set is closed (DESIGN.md 4.2); this handles the
// ground-movement subset that exists before frame data lands. Jump, attack,
// and stun transitions arrive with the character loader.
void tick_ground_movement(Fighter& fighter, InputFrame input) {
    if (fighter.hitstun_remaining > 0 || fighter.blockstun_remaining > 0) {
        fighter.velocity_x = Fixed();
        return;
    }

    // Block is a button, not hold-back (DESIGN.md 4.1), so blocking and
    // walking backward are never ambiguous and never need disentangling.
    if (input_held(input, Button::Block)) {
        fighter.state = FighterState::Blocking;
        fighter.velocity_x = Fixed();
        return;
    }

    if (input_vertical(input) > 0) {
        fighter.state = FighterState::Crouch;
        fighter.velocity_x = Fixed();
        return;
    }

    // Input is in world space; forward depends on which way the fighter faces.
    const int32_t horizontal = input_horizontal(input);
    const int32_t facing_sign = static_cast<int32_t>(fighter.facing);

    if (horizontal == 0) {
        fighter.state = FighterState::Idle;
        fighter.velocity_x = Fixed();
        return;
    }

    const bool moving_forward = (horizontal == facing_sign);
    fighter.state = moving_forward ? FighterState::WalkForward : FighterState::WalkBackward;

    const Fixed speed = moving_forward ? WALK_FORWARD_SPEED : WALK_BACKWARD_SPEED;
    fighter.velocity_x = speed * horizontal;
}

// Step 7. Clamping every frame is what keeps position bounded, which is in turn
// why fixed.h can treat overflow as a logic error rather than a case to handle.
void clamp_to_stage(Fighter& fighter) {
    fighter.x = fixed_clamp(fighter.x, STAGE_LEFT, STAGE_RIGHT);
}

// Step 8.
void tick_timers(Fighter& fighter) {
    if (fighter.hitstun_remaining > 0) {
        --fighter.hitstun_remaining;
    }
    if (fighter.blockstun_remaining > 0) {
        --fighter.blockstun_remaining;
    }
    if (fighter.state_frames_remaining > 0) {
        --fighter.state_frames_remaining;
    }
}

// Step 9. A round ends on KO or on timer expiry, in which case the fighter
// with more remaining health wins (DESIGN.md 2).
void tick_round_flow(GameState& state) {
    switch (state.round_phase) {
        case RoundPhase::Starting: {
            if (state.phase_frames_remaining > 0) {
                --state.phase_frames_remaining;
            }
            if (state.phase_frames_remaining == 0) {
                state.round_phase = RoundPhase::Fighting;
                state.fighters[0].state = FighterState::Idle;
                state.fighters[1].state = FighterState::Idle;
            }
            break;
        }

        case RoundPhase::Fighting: {
            if (state.round_timer > 0) {
                --state.round_timer;
            }

            const bool ko = state.fighters[0].health <= 0 || state.fighters[1].health <= 0;
            const bool timeout = state.round_timer == 0;
            if (!ko && !timeout) {
                break;
            }

            // On a double KO or an exact health tie at timeout, neither player
            // scores. Awarding both a round would let a match end 2-2.
            int32_t winner = -1;
            if (state.fighters[0].health > state.fighters[1].health) {
                winner = 0;
            } else if (state.fighters[1].health > state.fighters[0].health) {
                winner = 1;
            }

            if (winner >= 0) {
                ++state.rounds_won[winner];
                state.fighters[winner].state = FighterState::Win;
                state.fighters[1 - winner].state = FighterState::Lose;
            }

            const bool match_over = state.rounds_won[0] >= ROUNDS_TO_WIN ||
                                    state.rounds_won[1] >= ROUNDS_TO_WIN;
            state.round_phase = match_over ? RoundPhase::MatchEnded : RoundPhase::Ended;
            state.phase_frames_remaining = ROUND_END_FREEZE_FRAMES;
            break;
        }

        case RoundPhase::Ended: {
            if (state.phase_frames_remaining > 0) {
                --state.phase_frames_remaining;
            }
            if (state.phase_frames_remaining == 0) {
                begin_round(state);
            }
            break;
        }

        case RoundPhase::MatchEnded:
            // Terminal. The match is over and the sim holds this state until
            // the caller starts a new one.
            break;
    }
}

}  // namespace

void reset_fighter(Fighter& fighter, int32_t player_index) {
    // Zeroing first means every field has a defined value even as fields are
    // added later, which the byte-level state hash depends on.
    std::memset(&fighter, 0, sizeof(Fighter));

    fighter.x = Fixed::from_int(player_index == 0 ? ROUND_START_X_P1 : ROUND_START_X_P2);
    fighter.y = Fixed::from_int(GROUND_Y);
    fighter.state = FighterState::RoundStart;
    fighter.facing = player_index == 0 ? Facing::Right : Facing::Left;
    fighter.health = STARTING_HEALTH;
    fighter.move_id = -1;
    fighter.hit_confirm_frame = -1;
}

void begin_round(GameState& state) {
    ++state.round_number;
    state.round_timer = ROUND_TIMER_FRAMES;
    state.round_phase = RoundPhase::Starting;
    state.phase_frames_remaining = ROUND_START_FREEZE_FRAMES;

    reset_fighter(state.fighters[0], 0);
    reset_fighter(state.fighters[1], 1);

    std::memset(state.projectiles, 0, sizeof(state.projectiles));
    for (int32_t i = 0; i < MAX_PROJECTILES; ++i) {
        state.projectiles[i].owner_index = -1;
    }

    // The RNG is deliberately not reseeded here. A match is one continuous
    // stream from its seed; restarting it each round would make round two
    // replay identically to round one.
}

void init_state(GameState& state, uint64_t seed) {
    // Clears padding as well as fields. hash.h walks this struct's bytes, so
    // anything left uninitialized would differ between two machines and read
    // as a desync.
    std::memset(&state, 0, sizeof(GameState));

    rng_seed(state.rng, seed);

    state.frame = 0;
    state.round_number = 0;
    state.rounds_won[0] = 0;
    state.rounds_won[1] = 0;

    begin_round(state);
}

void advance_frame(GameState& state, InputPair current, InputPair previous) {
    // Reserved bits must not reach the sim: a device setting one would change
    // the state hash without changing behavior, which reads as a desync.
    for (int32_t i = 0; i < 2; ++i) {
        current.players[i] = input_sanitized(current.players[i]);
        previous.players[i] = input_sanitized(previous.players[i]);
    }

    // Players have no control during the round-start freeze or after a KO.
    const bool players_active = state.round_phase == RoundPhase::Fighting;

    // Step 2.
    resolve_facing(state);

    // Steps 3 and 4.
    for (int32_t i = 0; i < 2; ++i) {
        Fighter& fighter = state.fighters[i];
        fighter.hit_confirm_frame = -1;

        if (players_active) {
            tick_ground_movement(fighter, current.players[i]);
        } else {
            fighter.velocity_x = Fixed();
        }

        fighter.x += fighter.velocity_x;
        fighter.y += fighter.velocity_y;
    }

    // Steps 5 and 6 — pushbox separation and hit resolution — land with the
    // character loader. They are absent rather than stubbed, so that a reader
    // is not misled into thinking hits already resolve.

    // Step 7.
    for (int32_t i = 0; i < 2; ++i) {
        clamp_to_stage(state.fighters[i]);
    }

    // Step 8.
    for (int32_t i = 0; i < 2; ++i) {
        tick_timers(state.fighters[i]);
    }

    // Step 9.
    tick_round_flow(state);

    // Step 10.
    ++state.frame;
}

}  // namespace mw::sim
