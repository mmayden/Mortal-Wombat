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

// Which move a button press produces, given whether the fighter is crouching.
//
// Returns MoveId::Count when no attack button was newly pressed. Edges rather
// than held state: holding punch must produce one punch, not one per frame.
MoveId requested_move(InputFrame current, InputFrame previous, bool crouching) {
    struct Binding {
        Button button;
        MoveId standing;
        MoveId crouching;
    };

    // Order is the priority order when two buttons are pressed on the same
    // frame. Fixed rather than "whichever the loop happens to find first", so
    // that two machines resolve a simultaneous press identically. Punches
    // before kicks and light before heavy, because the lighter option is the
    // one a player mashing both is more likely to be able to react out of.
    constexpr Binding BINDINGS[] = {
        {Button::LowPunch, MoveId::StandLowPunch, MoveId::CrouchLowPunch},
        {Button::HighPunch, MoveId::StandHighPunch, MoveId::CrouchHighPunch},
        {Button::LowKick, MoveId::StandLowKick, MoveId::CrouchLowKick},
        {Button::HighKick, MoveId::StandHighKick, MoveId::CrouchHighKick},
    };

    for (const Binding& binding : BINDINGS) {
        if (input_pressed(current, previous, binding.button)) {
            return crouching ? binding.crouching : binding.standing;
        }
    }
    return MoveId::Count;
}

// True when any attack button was newly pressed this frame.
bool attack_pressed(InputFrame current, InputFrame previous) {
    return input_pressed(current, previous, Button::LowPunch) ||
           input_pressed(current, previous, Button::HighPunch) ||
           input_pressed(current, previous, Button::LowKick) ||
           input_pressed(current, previous, Button::HighKick);
}

// Gravity, derived from the character's jump_duration and jump_apex rather
// than written down as its own number.
//
// The arc is integrated discretely -- each frame the fighter moves by its
// current velocity, then gravity is added -- so the rise over `half` frames is
//
//     rise = v0 + sum(v0 - k*g) for k in [1, half)  =  g * half * (half + 1) / 2
//
// with v0 = half * g. Setting that equal to jump_apex gives the gravity below.
// Deriving it means changing jump_apex in a character's TOML actually changes
// how high they jump, instead of silently disagreeing with a separate constant.
//
// The exact denominator depends on the integration order and was got wrong
// twice before being derived by simulating the loop rather than reasoning about
// it. advance_frame applies gravity BEFORE moving, except on the frame the
// fighter leaves the ground, where the launch velocity is applied with no
// gravity yet -- that lone frame is the whole difference between half*(half+1)
// and half*(half-1), and between a 90-unit jump and a 99-unit one.
//
// The continuous formula (2*apex/half^2) is wrong here for the same reason:
// discrete integration is not the parabola it approximates.
Fixed jump_gravity(const CharacterData& character) {
    const int32_t half = character.jump_duration / 2;
    if (half <= 0) {
        return Fixed();
    }
    return Fixed::from_ratio(2 * character.jump_apex, half * (half + 1));
}

Fixed jump_rise_velocity(const CharacterData& character) {
    return jump_gravity(character) * (character.jump_duration / 2);
}

// Begins a jump. DESIGN.md 4.3: three types, chosen at takeoff, and the
// trajectory is committed there -- no air control, no double jump, no jump
// cancel. That commitment is the whole reason this is one function that runs
// once rather than a per-frame velocity update.
void begin_jump(Fighter& fighter, const CharacterData& character, int32_t horizontal) {
    fighter.state = FighterState::JumpStartup;
    fighter.state_frames_remaining = JUMP_STARTUP_FRAMES;
    fighter.move_id = -1;
    fighter.move_frame = 0;
    fighter.hit_already_landed = 0;

    // Horizontal velocity is set now and never touched again until landing.
    // `horizontal` is in world space; forward depends on facing.
    if (horizontal == 0) {
        fighter.velocity_x = Fixed();
    } else {
        const bool forward = (horizontal == static_cast<int32_t>(fighter.facing));
        const Fixed speed = forward ? character.walk_forward_speed : character.walk_backward_speed;
        fighter.velocity_x = speed * JUMP_HORIZONTAL_SCALE * horizontal;
    }

    // Vertical velocity is deliberately NOT set here. Movement is applied every
    // frame, so setting it now would lift the fighter off the ground during the
    // startup frames -- which is the opposite of a commitment window, and is
    // what the "stays grounded through startup" test caught. It is set at the
    // moment the fighter actually leaves the ground, below.
    fighter.velocity_y = Fixed();
}

// Advances a fighter that is in the air, or about to be.
//
// Returns true if it handled the fighter this frame, so the caller can skip the
// grounded state machine entirely -- being airborne is not a state a ground
// input can interrupt.
bool tick_airborne(Fighter& fighter, const CharacterData& character, InputFrame current,
                   InputFrame previous) {
    // Neither branch below decrements state_frames_remaining. Step 8 does that
    // for every fighter, once. Decrementing here as well halved every duration
    // -- landing recovery ran for 2 frames instead of 4 -- which is the kind of
    // error that reads as a feel problem rather than a bug.
    if (fighter.state == FighterState::JumpStartup) {
        if (fighter.state_frames_remaining == 0) {
            fighter.state = FighterState::Airborne;
            fighter.velocity_y = -jump_rise_velocity(character);
        }
        return true;
    }

    if (fighter.state == FighterState::Landing) {
        fighter.velocity_x = Fixed();
        if (fighter.state_frames_remaining == 0) {
            fighter.state = FighterState::Idle;
            fighter.move_id = -1;
            fighter.move_frame = 0;
        }
        return true;
    }

    if (fighter.state != FighterState::Airborne) {
        return false;
    }

    // A jump attack, if one has not already been thrown. DESIGN.md 4.5 gives
    // one jump attack rather than one per button, so any attack button
    // produces it.
    if (fighter.move_id < 0 && attack_pressed(current, previous)) {
        fighter.move_id = static_cast<int32_t>(MoveId::JumpAttack);
        fighter.move_frame = 1;
        fighter.hit_already_landed = 0;
    } else if (fighter.move_id >= 0) {
        ++fighter.move_frame;
    }

    // Gravity. Horizontal velocity is deliberately untouched: the arc was
    // committed at takeoff.
    fighter.velocity_y += jump_gravity(character);
    return true;
}

// Called after movement has been applied. Ends the jump on ground contact.
//
// Landing is detected by position rather than by counting frames, so the fixed
// arc and the ground plane cannot disagree -- a fighter cannot end a jump in
// the air or sink through the floor because a duration was tuned.
void resolve_landing(Fighter& fighter) {
    if (fighter.state != FighterState::Airborne) {
        return;
    }
    if (fighter.y < Fixed::from_int(GROUND_Y)) {
        return;
    }

    fighter.y = Fixed::from_int(GROUND_Y);
    fighter.velocity_y = Fixed();
    fighter.velocity_x = Fixed();
    fighter.state = FighterState::Landing;
    fighter.state_frames_remaining = LANDING_FRAMES;

    // The jump attack ends on touchdown regardless of its remaining frames.
    // DESIGN.md 4.5 gives its active window as "until landing", which the
    // schema cannot express as an integer -- so the rule lives here, in the
    // sim, rather than as a number in the data.
    fighter.move_id = -1;
    fighter.move_frame = 0;
}

void begin_move(Fighter& fighter, MoveId move) {
    fighter.state = FighterState::Attack;
    fighter.move_id = static_cast<int32_t>(move);

    // The frame the move starts IS frame 1 of the move, matching how
    // docs/framedata_schema.md asks hitbox frame ranges to be written. Starting
    // at 0 and incrementing next frame would give every move one extra frame of
    // startup that appears nowhere in its data.
    fighter.move_frame = 1;

    fighter.velocity_x = Fixed();
    fighter.hit_already_landed = 0;
}

// Steps 3 and 4. The v1 state set is closed (DESIGN.md 4.2); this handles the
// ground subset. Jumping arrives with the airborne states.
void tick_fighter_state(Fighter& fighter, const CharacterData& character, InputFrame current,
                        InputFrame previous) {
    // An attack in progress owns the fighter until it finishes.
    if (fighter.state == FighterState::Attack) {
        ++fighter.move_frame;
        fighter.velocity_x = Fixed();

        const MoveData& move = move_of(character, static_cast<MoveId>(fighter.move_id));
        if (fighter.move_frame > move_total_frames(move)) {
            fighter.state = FighterState::Idle;
            fighter.move_id = -1;
            fighter.move_frame = 0;
        }
        return;
    }

    // Airborne states own the fighter completely -- a ground input cannot
    // interrupt a jump, which is what "committed at takeoff" means.
    if (tick_airborne(fighter, character, current, previous)) {
        return;
    }

    // Losing your turn is the entire point of these two states, and it is what
    // makes landing a hit worth anything.
    if (fighter.hitstun_remaining > 0) {
        fighter.state = FighterState::Hitstun;
        fighter.velocity_x = Fixed();
        return;
    }
    if (fighter.blockstun_remaining > 0) {
        fighter.state = FighterState::Blockstun;
        fighter.velocity_x = Fixed();
        return;
    }

    const bool crouching = input_vertical(current) > 0;

    // Up starts a jump. Checked before attacks so that up-plus-button is a
    // jump attack rather than a grounded normal -- there is no separate jump
    // button, and DESIGN.md 4.1 has only four directions.
    if (input_vertical(current) < 0) {
        begin_jump(fighter, character, input_horizontal(current));
        return;
    }

    const MoveId move = requested_move(current, previous, crouching);
    if (move != MoveId::Count) {
        begin_move(fighter, move);
        return;
    }

    // Block is a button, not hold-back (DESIGN.md 4.1), so blocking and walking
    // backward are never ambiguous and never need disentangling.
    if (input_held(current, Button::Block)) {
        fighter.state = FighterState::Blocking;
        fighter.velocity_x = Fixed();
        return;
    }

    if (crouching) {
        fighter.state = FighterState::Crouch;
        fighter.velocity_x = Fixed();
        return;
    }

    // Input is in world space; forward depends on which way the fighter faces.
    const int32_t horizontal = input_horizontal(current);
    const int32_t facing_sign = static_cast<int32_t>(fighter.facing);

    if (horizontal == 0) {
        fighter.state = FighterState::Idle;
        fighter.velocity_x = Fixed();
        return;
    }

    const bool moving_forward = (horizontal == facing_sign);
    fighter.state = moving_forward ? FighterState::WalkForward : FighterState::WalkBackward;

    const Fixed speed =
        moving_forward ? character.walk_forward_speed : character.walk_backward_speed;
    fighter.velocity_x = speed * horizontal;
}

// The hurtbox a fighter presents right now.
Box active_hurtbox(const Fighter& fighter, const CharacterData& character) {
    if (fighter.state == FighterState::Attack) {
        const MoveData& move = move_of(character, static_cast<MoveId>(fighter.move_id));
        if (!box_is_empty(move.hurtbox_override)) {
            return move.hurtbox_override;
        }
    }
    if (fighter.state == FighterState::Crouch) {
        return character.crouching_hurtbox;
    }
    return character.standing_hurtbox;
}

// Step 5. Fighters have bodies and cannot occupy the same space.
//
// Both are pushed half the overlap, which keeps the resolution symmetric --
// order-independent, and therefore identical on two machines. At a stage wall
// the clamp in step 7 can reintroduce a small overlap; that is accepted for v1
// rather than solved with an iterative solver, which would be the beginning of
// the physics engine ADR 0006 declines to have.
void resolve_pushboxes(GameState& state, const MatchData& data) {
    const Box a = world_box(state.fighters[0], data.characters[0].pushbox);
    const Box b = world_box(state.fighters[1], data.characters[1].pushbox);

    if (!boxes_overlap(a, b)) {
        return;
    }

    const int32_t left_index = (state.fighters[0].x <= state.fighters[1].x) ? 0 : 1;
    const int32_t right_index = 1 - left_index;

    const Box& left_box = (left_index == 0) ? a : b;
    const Box& right_box = (left_index == 0) ? b : a;

    const int32_t overlap = (left_box.x + left_box.w) - right_box.x;
    if (overlap <= 0) {
        return;
    }

    const Fixed push = Fixed::from_int(overlap) / 2;
    state.fighters[left_index].x -= push;
    state.fighters[right_index].x += push;
}

// Step 6. The step where a fighting game becomes a fighting game.
//
// Both attackers are evaluated against the state as it stands BEFORE any hit is
// applied, and the results are applied afterwards. Resolving one fighter fully
// and then the other would let player one hit put player two into hitstun
// before player two simultaneous hit was tested, silently making player one win
// every trade, on every machine, forever.
void resolve_hits(GameState& state, const MatchData& data) {
    struct Outcome {
        bool connected;
        bool blocked;
        int32_t damage;
        int32_t stun;
    };
    Outcome outcomes[2] = {};

    for (int32_t attacker = 0; attacker < 2; ++attacker) {
        const int32_t defender = 1 - attacker;
        const Fighter& attacking = state.fighters[attacker];
        const Fighter& defending = state.fighters[defender];

        // Keyed on move_id, NOT on the state.
        //
        // A jump attack runs while the state is Airborne -- the jump owns the
        // state and the move rides along. Testing for FighterState::Attack
        // meant the jump attack was never evaluated for collision at all: a
        // sweep of 208 combinations of range and timing produced zero hits.
        //
        // This is the third place the same assumption caused a bug. The
        // renderer hid the jump attack's limb, the debug overlay hid its
        // hitbox, and here it could not connect. "Attacking" is a move being
        // active, not a state the fighter is in.
        if (attacking.move_id < 0 || attacking.hit_already_landed != 0) {
            continue;
        }

        const MoveData& move =
            move_of(data.characters[attacker], static_cast<MoveId>(attacking.move_id));
        if (!move_is_active_on(move, attacking.move_frame)) {
            continue;
        }

        const Box hurtbox =
            world_box(defending, active_hurtbox(defending, data.characters[defender]));

        for (int32_t i = 0; i < move.hitbox_count; ++i) {
            const HitboxSpan& span = move.hitboxes[i];
            if (attacking.move_frame < span.first_frame || attacking.move_frame > span.last_frame) {
                continue;
            }
            if (!boxes_overlap(world_box(attacking, span.box), hurtbox)) {
                continue;
            }

            // DESIGN.md 4.6 cuts chip damage, so a blocked hit deals none. It
            // still costs the defender their turn, which is what keeps
            // attacking into a block a real decision rather than a free one.
            const bool blocked = defending.state == FighterState::Blocking;
            outcomes[attacker] = Outcome{true, blocked, blocked ? 0 : move.damage,
                                         blocked ? move.blockstun : move.hitstun};
            break;
        }
    }

    for (int32_t attacker = 0; attacker < 2; ++attacker) {
        if (!outcomes[attacker].connected) {
            continue;
        }
        const int32_t defender = 1 - attacker;

        state.fighters[attacker].hit_already_landed = 1;
        state.fighters[attacker].hit_confirm_frame = state.frame;

        Fighter& hurt = state.fighters[defender];
        if (outcomes[attacker].blocked) {
            hurt.blockstun_remaining = outcomes[attacker].stun;
            hurt.state = FighterState::Blockstun;
        } else {
            hurt.health -= outcomes[attacker].damage;
            if (hurt.health < 0) {
                hurt.health = 0;
            }
            hurt.hitstun_remaining = outcomes[attacker].stun;
            hurt.state = FighterState::Hitstun;

            // Being hit interrupts whatever the defender was doing. Without
            // this, a fighter struck during startup would resume the attack the
            // moment hitstun ended, from the frame they left off.
            hurt.move_id = -1;
            hurt.move_frame = 0;
        }
        hurt.velocity_x = Fixed();
    }
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

            const bool match_over =
                state.rounds_won[0] >= ROUNDS_TO_WIN || state.rounds_won[1] >= ROUNDS_TO_WIN;
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

bool boxes_overlap(const Box& a, const Box& b) {
    // Strict inequalities: touching edges do not overlap. A hitbox whose right
    // edge exactly meets a hurtbox left edge has not reached it, and counting
    // that as contact would make every move one unit longer than its data says.
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

Box world_box(const Fighter& fighter, const Box& local) {
    // Authored facing right (docs/framedata_schema.md), so a left-facing
    // fighter mirrors x about the origin. Mirroring the far edge rather than
    // the near one is what keeps the box the same width either way.
    const int32_t origin_x = fighter.x.to_int();
    const int32_t origin_y = fighter.y.to_int();
    const int32_t x =
        (fighter.facing == Facing::Right) ? origin_x + local.x : origin_x - (local.x + local.w);

    return Box{x, origin_y + local.y, local.w, local.h};
}

void advance_frame(GameState& state, const MatchData& data, InputPair current, InputPair previous) {
    // Reserved bits must not reach the sim: a device setting one would change
    // the state hash without changing behavior, which reads as a desync.
    for (int32_t i = 0; i < 2; ++i) {
        current.players[i] = input_sanitized(current.players[i]);
        previous.players[i] = input_sanitized(previous.players[i]);
    }

    // Players have no control during the round-start freeze or after a KO.
    const bool players_active = state.round_phase == RoundPhase::Fighting;

    // Step 1 is input decoding, which happens inside step 3 where the decision
    // it feeds is made.

    // Step 2.
    resolve_facing(state);

    // Steps 3 and 4.
    for (int32_t i = 0; i < 2; ++i) {
        Fighter& fighter = state.fighters[i];
        fighter.hit_confirm_frame = -1;

        if (players_active) {
            tick_fighter_state(fighter, data.characters[i], current.players[i],
                               previous.players[i]);
        } else {
            fighter.velocity_x = Fixed();
        }

        fighter.x += fighter.velocity_x;
        fighter.y += fighter.velocity_y;
    }

    // Ground contact, checked after movement so the fighter is tested at the
    // position it actually reached this frame.
    for (int32_t i = 0; i < 2; ++i) {
        resolve_landing(state.fighters[i]);
    }

    // Step 5.
    resolve_pushboxes(state, data);

    // Step 6. Hits resolve after movement, so a hitbox is tested at the
    // position it actually occupies this frame -- the ordering that makes frame
    // data mean what the frame-data table says it means.
    if (players_active) {
        resolve_hits(state, data);
    }

    // Step 7.
    for (int32_t i = 0; i < 2; ++i) {
        clamp_to_stage(state.fighters[i]);
    }

    // Step 8. Runs after hit resolution, so stun applied this frame is
    // decremented once here: the frame a hit lands is the FIRST frame of the
    // defender's stun, not a free frame before it starts. A move with 18
    // hitstun therefore holds the defender for exactly 18 frames counting the
    // one they were hit on, which is what the DESIGN.md 4.5 number means.
    for (int32_t i = 0; i < 2; ++i) {
        tick_timers(state.fighters[i]);
    }

    // Step 9.
    tick_round_flow(state);

    // Step 10.
    ++state.frame;
}

}  // namespace mw::sim
