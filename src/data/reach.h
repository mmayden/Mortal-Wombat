// How far a move actually reaches, measured rather than read off the data.
//
// This exists because of a specific failure. The jump attack shipped with its
// hitbox at standing-punch height, so it floated above the opponent's head for
// its entire arc and could not touch anyone from any range at any timing. Every
// number in the file was plausible; the geometry was not. Nobody could see it,
// because a hitbox is four integers in a text file and a miss is the absence of
// an event.
//
// So the question "does this move connect, and from where" is answered here by
// simulating the overlap directly, using the same world_box and boxes_overlap
// the simulation uses. Sharing those functions is the point: an analysis that
// reimplemented the geometry could agree with itself and disagree with the
// game, which is exactly how the debug overlay was once blind to the same bug
// the simulation was.
//
// Above the sim boundary. Used by tools/framedata_viewer to draw the answer and
// by tests/unit/test_reach.cpp to assert every move has one.
#pragma once

#include <cstdint>

#include "sim/framedata.h"
#include "sim/sim.h"
#include "sim/state.h"

namespace ds::data {

struct MoveReach {
    // False means the move cannot touch this defender at ANY separation on ANY
    // of its active frames. That is almost always a bug in the geometry rather
    // than a design choice.
    bool connects = false;

    // The separation between the two fighters' ground points, in world units,
    // over which the move connects. Both inclusive; meaningless if !connects.
    int32_t nearest = 0;
    int32_t furthest = 0;

    // The move frame (1-based, as the TOML writes them) that reaches furthest.
    int32_t best_frame = 0;
};

// Separations searched, in world units. 8 is inside any plausible pushbox and
// 400 is most of a screen, so a move that connects nowhere in this range
// connects nowhere that matters.
inline constexpr int32_t REACH_SEARCH_MIN = 8;
inline constexpr int32_t REACH_SEARCH_MAX = 400;

// Measures `move` performed by `attacker` against `defender` standing still.
//
// Both fighters are placed on the ground facing each other, the attacker on the
// left. The defender is a stationary target: this measures geometry, not a
// fight, so nothing moves and no state advances.
inline MoveReach measure_reach(const ds::sim::CharacterData& attacker,
                               const ds::sim::CharacterData& defender, ds::sim::MoveId move,
                               bool defender_crouching = false) {
    using namespace ds::sim;

    const MoveData& data = move_of(attacker, move);
    const Box& defender_local =
        defender_crouching ? defender.crouching_hurtbox : defender.standing_hurtbox;

    Fighter attacking{};
    attacking.facing = Facing::Right;
    attacking.x = Fixed::from_int(0);
    attacking.y = Fixed::from_int(GROUND_Y);

    Fighter defending{};
    defending.facing = Facing::Left;
    defending.y = Fixed::from_int(GROUND_Y);

    MoveReach result;

    for (int32_t gap = REACH_SEARCH_MIN; gap <= REACH_SEARCH_MAX; ++gap) {
        defending.x = Fixed::from_int(gap);
        const Box defender_world = world_box(defending, defender_local);

        for (int32_t i = 0; i < data.hitbox_count; ++i) {
            const HitboxSpan& span = data.hitboxes[i];
            if (box_is_empty(span.box)) {
                continue;
            }
            if (!boxes_overlap(world_box(attacking, span.box), defender_world)) {
                continue;
            }

            if (!result.connects) {
                result.connects = true;
                result.nearest = gap;
            }
            if (gap >= result.furthest) {
                result.furthest = gap;
                // The frame credited is the first one that reaches this far,
                // which is the one a player would actually be trying to hit.
                result.best_frame = span.first_frame;
            }
            break;  // One overlap is enough to call this separation a hit.
        }
    }

    return result;
}

}  // namespace ds::data
