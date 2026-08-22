// Hit detection, blocking, and stun. ARCHITECTURE.md section 5, steps 5 and 6.
//
// This is the tier that says whether the game is a fighting game. Everything
// before it — fixed point, RNG, input, the loop — was scaffolding for the
// moment one fighter can hit another.
//
// Tests run against the SHIPPED frame data rather than invented numbers, so a
// change to data/characters/*.toml that breaks combat fails here.
#include <cstring>
#include <type_traits>

#include <doctest/doctest.h>

#include "sim/sim.h"

#include "match_data.h"

using namespace mw::sim;

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
    return mw::test::shipped_match_data();
}

void skip_to_fighting(GameState& state) {
    while (state.round_phase != RoundPhase::Fighting) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
    }
}

// Puts the fighters close enough that a normal reaches. Reaching in to set
// position is a test affordance; the sim only ever moves fighters by velocity.
void place_at_range(GameState& state, int32_t separation) {
    const int32_t centre = STAGE_WIDTH / 2;
    state.fighters[0].x = Fixed::from_int(centre - separation / 2);
    state.fighters[1].x = Fixed::from_int(centre + separation / 2);
}

GameState fighting_state(int32_t separation = 40) {
    GameState state;
    init_state(state, 1u);
    skip_to_fighting(state);
    place_at_range(state, separation);
    return state;
}

// Presses a button for one frame, then holds neutral. Returns the frame index
// within the move, so a caller can drive a move to a specific frame.
void press(GameState& state, Button button, int32_t player) {
    InputPair current = NO_INPUT;
    current.players[player] = held(button);
    advance_frame(state, data(), current, NO_INPUT);
}

}  // namespace

TEST_CASE("boxes_overlap is exclusive at the edges") {
    // A hitbox whose right edge exactly meets a hurtbox's left edge has not
    // reached it. Counting that as contact would silently make every move one
    // unit longer than its frame data says.
    const Box a{0, 0, 10, 10};

    CHECK(boxes_overlap(a, Box{5, 5, 10, 10}));
    CHECK(boxes_overlap(a, Box{9, 9, 10, 10}));
    CHECK_FALSE(boxes_overlap(a, Box{10, 0, 10, 10}));  // touching on x
    CHECK_FALSE(boxes_overlap(a, Box{0, 10, 10, 10}));  // touching on y
    CHECK_FALSE(boxes_overlap(a, Box{20, 20, 10, 10}));

    SUBCASE("overlap is symmetric") {
        const Box b{5, 5, 10, 10};
        CHECK(boxes_overlap(a, b) == boxes_overlap(b, a));
    }
}

TEST_CASE("world_box mirrors for a left-facing fighter") {
    // Authored data is always written facing right, so this is the only place
    // facing is applied to geometry.
    Fighter fighter{};
    fighter.x = Fixed::from_int(100);
    fighter.y = Fixed::from_int(GROUND_Y);

    const Box local{20, -40, 30, 10};

    fighter.facing = Facing::Right;
    const Box right = world_box(fighter, local);
    CHECK(right.x == 120);
    CHECK(right.y == GROUND_Y - 40);

    fighter.facing = Facing::Left;
    const Box left = world_box(fighter, local);
    CHECK(left.x == 100 - (20 + 30));

    SUBCASE("mirroring preserves width, so reach is the same either way") {
        CHECK(left.w == right.w);
        CHECK(left.h == right.h);
        CHECK(right.x - 100 == 100 - (left.x + left.w));
    }
}

TEST_CASE("A punch connects and deals its documented damage") {
    // The first hit that connects — the last step of the bootstrap order in the
    // stack decision.
    GameState state = fighting_state();
    const int32_t before = state.fighters[1].health;

    press(state, Button::HighPunch, 0);
    REQUIRE(state.fighters[0].state == FighterState::Attack);

    // High punch has 7 frames of startup, so it cannot have hit yet.
    CHECK(state.fighters[1].health == before);

    for (int32_t i = 0; i < 12; ++i) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
    }

    const MoveData& hp = move_of(data().characters[0], MoveId::StandHighPunch);
    CHECK(state.fighters[1].health == before - hp.damage);
    CHECK(hp.damage == 8);  // DESIGN.md 4.5
}

TEST_CASE("A hit lands exactly once, not once per active frame") {
    // High punch is active for 3 frames. Without hit_already_landed it would
    // deal 8 damage three times, and every move in the game would hit for
    // triple its documented value.
    GameState state = fighting_state();
    const int32_t before = state.fighters[1].health;

    press(state, Button::HighPunch, 0);
    for (int32_t i = 0; i < 30; ++i) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
    }

    const MoveData& hp = move_of(data().characters[0], MoveId::StandHighPunch);
    CHECK(state.fighters[1].health == before - hp.damage);
}

TEST_CASE("A hit puts the defender in hitstun for the documented duration") {
    GameState state = fighting_state();

    press(state, Button::HighPunch, 0);
    while (state.fighters[1].hitstun_remaining == 0) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
        REQUIRE(state.frame < 200);  // guard against never connecting
    }

    const MoveData& hp = move_of(data().characters[0], MoveId::StandHighPunch);
    CHECK(state.fighters[1].state == FighterState::Hitstun);

    // One less than the move's hitstun, because the frame a hit lands is the
    // FIRST frame of stun rather than a free frame before it starts. Step 8
    // decrements it in the same frame step 6 applied it.
    CHECK(state.fighters[1].hitstun_remaining == hp.hitstun - 1);

    SUBCASE("and the defender cannot act for exactly the documented duration") {
        int32_t stunned_frames = 1;  // the frame the hit landed
        while (state.fighters[1].hitstun_remaining > 0) {
            // Player two mashes punch throughout; none of it should come out.
            advance_frame(state, data(), pair_with(NEUTRAL, held(Button::HighPunch)), NO_INPUT);
            CHECK(state.fighters[1].state != FighterState::Attack);
            ++stunned_frames;
            REQUIRE(stunned_frames < 100);
        }
        CHECK(stunned_frames == hp.hitstun);
        CHECK(hp.hitstun == 18);  // DESIGN.md 4.5
    }
}

TEST_CASE("Blocking prevents damage but still costs the defender their turn") {
    // DESIGN.md 4.6 cuts chip damage, so a blocked hit deals none. Blockstun is
    // what keeps attacking into a block a real decision rather than a free one.
    GameState state = fighting_state();
    const int32_t before = state.fighters[1].health;

    // Player two holds block from the outset; player one attacks into it.
    InputPair attack_into_block = pair_with(held(Button::HighPunch), held(Button::Block));
    advance_frame(state, data(), attack_into_block, NO_INPUT);

    const InputPair keep_blocking = pair_with(NEUTRAL, held(Button::Block));
    for (int32_t i = 0; i < 12; ++i) {
        advance_frame(state, data(), keep_blocking, keep_blocking);
    }

    const MoveData& hp = move_of(data().characters[0], MoveId::StandHighPunch);
    CHECK(state.fighters[1].health == before);
    CHECK(state.fighters[1].blockstun_remaining > 0);
    CHECK(state.fighters[1].blockstun_remaining <= hp.blockstun);
}

TEST_CASE("Blockstun is shorter than hitstun, so blocking is the better outcome") {
    // DESIGN.md 4.5 gives every move less blockstun than hitstun. If that ever
    // inverted, blocking would be worse than being hit and the defensive game
    // would collapse.
    for (int32_t i = 0; i < MOVE_COUNT; ++i) {
        const MoveData& move = data().characters[0].moves[i];
        CAPTURE(i);
        CHECK(move.blockstun < move.hitstun);
    }
}

TEST_CASE("Simultaneous hits both land") {
    // A trade. If hits were resolved one fighter at a time, player one's hit
    // would put player two into hitstun before player two's hit was tested,
    // and player one would win every trade forever.
    GameState state = fighting_state();
    const int32_t p1_before = state.fighters[0].health;
    const int32_t p2_before = state.fighters[1].health;

    const InputPair both_punch = pair_with(held(Button::HighPunch), held(Button::HighPunch));
    advance_frame(state, data(), both_punch, NO_INPUT);

    for (int32_t i = 0; i < 12; ++i) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
    }

    CHECK(state.fighters[0].health < p1_before);
    CHECK(state.fighters[1].health < p2_before);
}

TEST_CASE("A move whiffs when the opponent is out of range") {
    GameState state = fighting_state(400);
    const int32_t before = state.fighters[1].health;

    press(state, Button::HighPunch, 0);
    for (int32_t i = 0; i < 30; ++i) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
    }

    CHECK(state.fighters[1].health == before);
}

TEST_CASE("An attack runs to completion before another can start") {
    // Committing to an action means committing (DESIGN.md 3). Attacks cannot be
    // cancelled, which DESIGN.md 4.6 also cuts explicitly.
    GameState state = fighting_state(400);

    press(state, Button::HighKick, 0);
    REQUIRE(state.fighters[0].move_id == static_cast<int32_t>(MoveId::StandHighKick));

    const MoveData& hk = move_of(data().characters[0], MoveId::StandHighKick);
    const int32_t total = move_total_frames(hk);

    for (int32_t i = 1; i < total; ++i) {
        // Mashing low punch mid-kick must not interrupt it.
        advance_frame(state, data(), pair_with(held(Button::LowPunch), NEUTRAL), NO_INPUT);
        CHECK(state.fighters[0].move_id == static_cast<int32_t>(MoveId::StandHighKick));
    }

    advance_frame(state, data(), NO_INPUT, NO_INPUT);
    CHECK(state.fighters[0].state == FighterState::Idle);
}

TEST_CASE("Holding a button produces one attack, not one per frame") {
    // Edge detection, from two consecutive InputFrames. The sim derives edges
    // rather than storing them so a rollback recomputes them correctly.
    GameState state = fighting_state(400);

    const InputPair holding = pair_with(held(Button::LowPunch), NEUTRAL);
    advance_frame(state, data(), holding, NO_INPUT);
    REQUIRE(state.fighters[0].state == FighterState::Attack);

    const MoveData& lp = move_of(data().characters[0], MoveId::StandLowPunch);
    const int32_t total = move_total_frames(lp);

    for (int32_t i = 0; i < total; ++i) {
        advance_frame(state, data(), holding, holding);
    }

    // The button never went up, so no second punch may start.
    CHECK(state.fighters[0].state == FighterState::Idle);
}

TEST_CASE("Crouching produces the crouching variant") {
    GameState state = fighting_state(400);

    advance_frame(state, data(),
                  pair_with(input_with(held(Button::Down), Button::LowKick), NEUTRAL), NO_INPUT);

    CHECK(state.fighters[0].move_id == static_cast<int32_t>(MoveId::CrouchLowKick));
}

TEST_CASE("Being hit interrupts the defender's move") {
    // Without this, a fighter struck during startup would resume their attack
    // the instant hitstun ended, from the frame they left off.
    GameState state = fighting_state();

    // Player two starts a slow high kick; player one interrupts with a fast
    // low punch.
    advance_frame(state, data(), pair_with(NEUTRAL, held(Button::HighKick)), NO_INPUT);
    REQUIRE(state.fighters[1].state == FighterState::Attack);

    advance_frame(state, data(), pair_with(held(Button::LowPunch), NEUTRAL), NO_INPUT);
    for (int32_t i = 0; i < 8; ++i) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
    }

    REQUIRE(state.fighters[1].hitstun_remaining > 0);
    CHECK(state.fighters[1].move_id == -1);
    CHECK(state.fighters[1].move_frame == 0);
}

TEST_CASE("Pushboxes keep fighters from occupying the same space") {
    GameState state = fighting_state(4);

    for (int32_t i = 0; i < 60; ++i) {
        // Both walk into each other for a full second.
        advance_frame(state, data(), pair_with(held(Button::Right), held(Button::Left)), NO_INPUT);
    }

    const Box a = world_box(state.fighters[0], data().characters[0].pushbox);
    const Box b = world_box(state.fighters[1], data().characters[1].pushbox);
    CHECK_FALSE(boxes_overlap(a, b));
}

TEST_CASE("Pushbox separation is symmetric") {
    // Both fighters are pushed the same distance, so the resolution does not
    // depend on which is processed first -- which is what makes it identical on
    // two machines.
    GameState state = fighting_state(0);
    const Fixed centre = (state.fighters[0].x + state.fighters[1].x) / 2;

    advance_frame(state, data(), NO_INPUT, NO_INPUT);

    const Fixed left_gap = centre - state.fighters[0].x;
    const Fixed right_gap = state.fighters[1].x - centre;
    CHECK(left_gap == right_gap);
}

TEST_CASE("Damage cannot drive health below zero") {
    GameState state = fighting_state();
    state.fighters[1].health = 1;

    press(state, Button::HighPunch, 0);
    for (int32_t i = 0; i < 12; ++i) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
    }

    CHECK(state.fighters[1].health == 0);
}

TEST_CASE("A KO from a real hit ends the round") {
    // The full chain, end to end: input, startup, active frames, overlap,
    // damage, KO, round flow.
    GameState state = fighting_state();
    state.fighters[1].health = 1;

    press(state, Button::HighPunch, 0);
    for (int32_t i = 0; i < 20; ++i) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
    }

    CHECK(state.fighters[1].health == 0);
    CHECK(state.rounds_won[0] == 1);
    CHECK(state.round_phase == RoundPhase::Ended);
}

TEST_CASE("hit_confirm_frame is set on the hit frame and cleared after") {
    // The render and audio layers observe this rather than being called back
    // into, because a side effect inside the sim fires again on every rollback
    // re-simulation (ARCHITECTURE.md 1).
    GameState state = fighting_state();

    press(state, Button::HighPunch, 0);
    while (state.fighters[0].hit_confirm_frame < 0) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
        REQUIRE(state.frame < 200);
    }

    CHECK(state.fighters[0].hit_confirm_frame == state.frame - 1);

    advance_frame(state, data(), NO_INPUT, NO_INPUT);
    CHECK(state.fighters[0].hit_confirm_frame == -1);
}

TEST_CASE("Combat is deterministic from the same inputs") {
    // The property everything else rests on. Two runs of the same script must
    // produce identical state, or rollback and replay are both meaningless.
    auto run = [] {
        GameState state = fighting_state();
        for (int32_t i = 0; i < 120; ++i) {
            const bool punch = (i % 17) == 0;
            const bool block = (i % 23) == 0;
            const InputPair input = pair_with(punch ? held(Button::HighPunch) : NEUTRAL,
                                              block ? held(Button::Block) : held(Button::LowKick));
            advance_frame(state, data(), input, input);
        }
        return state;
    };

    const GameState a = run();
    const GameState b = run();
    CHECK(std::memcmp(&a, &b, sizeof(GameState)) == 0);
}
