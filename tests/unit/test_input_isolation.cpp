// One player's input must move only that player.
//
// Written in response to a report that pressing a direction moved both
// fighters. The sim turned out to be innocent -- but nothing had ever asserted
// it, which is why the question could not be answered without an experiment.
// It is asserted now.
#include <doctest/doctest.h>

#include "sim/sim.h"

#include "match_data.h"

using namespace mw::sim;

namespace {

constexpr InputFrame NEUTRAL{0u};
constexpr InputPair NO_INPUT{{NEUTRAL, NEUTRAL}};

const MatchData& data() {
    return mw::test::shipped_match_data();
}

GameState fighting_state() {
    GameState state;
    init_state(state, 1u);
    while (state.round_phase != RoundPhase::Fighting) {
        advance_frame(state, data(), NO_INPUT, NO_INPUT);
    }
    return state;
}

InputPair only(int32_t player, Button button) {
    InputPair pair = NO_INPUT;
    pair.players[player] = input_with(NEUTRAL, button);
    return pair;
}

}  // namespace

TEST_CASE("One player's directional input moves only that player") {
    for (int32_t actor = 0; actor < 2; ++actor) {
        CAPTURE(actor);
        const int32_t other = 1 - actor;

        GameState state = fighting_state();
        const Fixed actor_before = state.fighters[actor].x;
        const Fixed other_before = state.fighters[other].x;

        // Thirty frames of walking, far short of the ~140 needed to close the
        // round-start gap -- so the two cannot touch and pushboxes cannot be
        // what moves the other fighter.
        for (int32_t i = 0; i < 30; ++i) {
            advance_frame(state, data(), only(actor, Button::Right), NO_INPUT);
        }

        CHECK(state.fighters[actor].x != actor_before);
        CHECK(state.fighters[other].x == other_before);
    }
}

TEST_CASE("One player's attack input moves and acts only for that player") {
    for (int32_t actor = 0; actor < 2; ++actor) {
        CAPTURE(actor);
        const int32_t other = 1 - actor;

        GameState state = fighting_state();
        advance_frame(state, data(), only(actor, Button::HighPunch), NO_INPUT);

        CHECK(state.fighters[actor].state == FighterState::Attack);
        CHECK(state.fighters[other].state != FighterState::Attack);
    }
}

TEST_CASE("Pushboxes DO move both fighters, once they touch") {
    // The legitimate case where one player's input moves the other, and the
    // most likely explanation for a report of "both characters move". Asserted
    // so the distinction between this and an input-routing bug is written down
    // rather than re-derived each time.
    GameState state = fighting_state();

    // Put them touching, then have player one keep walking forward.
    state.fighters[0].x = state.fighters[1].x - Fixed::from_int(20);
    const Fixed other_before = state.fighters[1].x;

    for (int32_t i = 0; i < 30; ++i) {
        advance_frame(state, data(), only(0, Button::Right), NO_INPUT);
    }

    CHECK(state.fighters[1].x > other_before);
}
