// Every move must be able to hit somebody.
//
// This is the regression net for a bug that shipped and survived review: the
// jump attack's hitbox sat at standing-punch height, so it floated above the
// opponent's head for the whole arc and could not connect from any range at any
// timing. It was invisible three ways over -- the numbers looked plausible in
// the TOML, the renderer hid the limb because it keyed on the wrong state, and
// the F1 debug overlay was blind for the same reason the simulation was.
//
// A miss is the absence of an event, so nothing failed. Nothing ever fails when
// a hitbox reaches nowhere; the move simply never works, quietly, forever.
//
// These assertions are cheap and structural. They do not check that a move is
// balanced or that its range is good -- both are design questions that need a
// human. They check that the geometry is not nonsense.

#include <string>

#include <doctest/doctest.h>

#include "data/reach.h"
#include "match_data.h"

using namespace ds::sim;
using ds::data::measure_reach;
using ds::data::MoveReach;

namespace {

// Returns std::string, not const char*. A const char* silently converts to bool
// on the way into doctest's message stream, so the name of the failing move
// printed as "1" -- which is worse than printing nothing, because it looks like
// data. Caught only because a deliberately broken hitbox was used to prove
// these assertions could fail at all.
std::string move_name(MoveId move) {
    switch (move) {
        case MoveId::StandLowPunch:
            return "stand low punch";
        case MoveId::StandHighPunch:
            return "stand high punch";
        case MoveId::StandLowKick:
            return "stand low kick";
        case MoveId::StandHighKick:
            return "stand high kick";
        case MoveId::CrouchLowPunch:
            return "crouch low punch";
        case MoveId::CrouchHighPunch:
            return "crouch high punch";
        case MoveId::CrouchLowKick:
            return "crouch low kick";
        case MoveId::CrouchHighKick:
            return "crouch high kick";
        case MoveId::JumpAttack:
            return "jump attack";
        case MoveId::Special:
            return "special";
        default:
            return "unknown";
    }
}

}  // namespace

TEST_CASE("Every move can connect with a standing opponent") {
    const MatchData& data = ds::test::shipped_match_data();

    for (int32_t c = 0; c < 2; ++c) {
        for (int32_t m = 0; m < MOVE_COUNT; ++m) {
            const MoveId move = static_cast<MoveId>(m);

            // The jump attack is the exception, and deliberately so: it is
            // performed from the air, and this harness places both fighters on
            // the ground. Testing it here would assert something false. Its own
            // reach is covered by tests/unit/test_jump_attack_reach.cpp, which
            // runs a real jump.
            if (move == MoveId::JumpAttack) {
                continue;
            }

            INFO("character " << c << ", move: " << move_name(move));

            const MoveReach reach = measure_reach(data.characters[c], data.characters[1 - c], move);

            // A move that reaches nowhere is the jump-attack bug again. It is
            // not a balance problem -- it is a move that does not exist.
            REQUIRE(reach.connects);
            CHECK(reach.furthest > reach.nearest);
        }
    }
}

TEST_CASE("A crouching opponent can still be hit by something") {
    const MatchData& data = ds::test::shipped_match_data();

    // Not every move should hit a croucher -- that is the whole point of an
    // attack height, and decision 7 in drawing-board/RULESET.md has not settled
    // heights yet. What must be true is that SOME move can, or crouching is a
    // free defence against everything and the game has no offence at all.
    for (int32_t c = 0; c < 2; ++c) {
        INFO("character " << c);
        bool anything_hits = false;

        for (int32_t m = 0; m < MOVE_COUNT; ++m) {
            const MoveId move = static_cast<MoveId>(m);
            if (move == MoveId::JumpAttack) {
                continue;
            }
            if (measure_reach(data.characters[c], data.characters[1 - c], move, true).connects) {
                anything_hits = true;
                break;
            }
        }

        CHECK(anything_hits);
    }
}

TEST_CASE("Heavier attacks reach at least as far as lighter ones") {
    const MatchData& data = ds::test::shipped_match_data();

    // Not a law of the genre, but it is what the current data intends, and the
    // pairing is the kind of thing a hand edit silently inverts. If a deliberate
    // design change makes a light poke the longest range tool, delete this case
    // and say so -- do not quietly widen the tolerance.
    for (int32_t c = 0; c < 2; ++c) {
        INFO("character " << c);
        const CharacterData& attacker = data.characters[c];
        const CharacterData& defender = data.characters[1 - c];

        const int32_t light_punch =
            measure_reach(attacker, defender, MoveId::StandLowPunch).furthest;
        const int32_t heavy_punch =
            measure_reach(attacker, defender, MoveId::StandHighPunch).furthest;
        const int32_t light_kick = measure_reach(attacker, defender, MoveId::StandLowKick).furthest;
        const int32_t heavy_kick =
            measure_reach(attacker, defender, MoveId::StandHighKick).furthest;

        CHECK(heavy_punch >= light_punch);
        CHECK(heavy_kick >= light_kick);
    }
}
