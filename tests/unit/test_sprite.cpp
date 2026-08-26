// The six attack buttons must produce six visibly different pictures.
//
// This exists because a human played the build and said, in as many words: "I
// don't know the difference in the inputs or attacks so I don't know what's
// happening. All the buttons do something though."
//
// They were right, and it was not a matter of taste. The limb was a constant
// 26 units for every attack, so a light punch and a heavy kick drew exactly the
// same rectangle while reaching 46 and 77 units respectively. The information
// was all in the frame data and none of it was on screen.
//
// The deeper failure is the one worth guarding: a picture that is ALLOWED to
// disagree with the simulation. That is how the jump attack stayed invisible --
// the renderer and the F1 overlay both showed something plausible that the sim
// did not agree with. So these tests do not check that the limb looks nice.
// They check that it is the hitbox.

#include <string>

#include <doctest/doctest.h>

#include "render/sprite.h"

#include "data/reach.h"
#include "match_data.h"

using namespace ds::sim;
using ds::render::placeholder_fighter_sprites;
using ds::render::SpriteList;

namespace {

// A fighter mid-attack on a chosen frame of a chosen move.
Fighter attacking_on(MoveId move, int32_t move_frame) {
    Fighter fighter{};
    reset_fighter(fighter, 0);
    fighter.state = FighterState::Attack;
    fighter.move_id = static_cast<int32_t>(move);
    fighter.move_frame = move_frame;
    return fighter;
}

// The limb is the widest quad that is not the body. The body is drawn first and
// is always BODY_WIDTH across, so anything wider than it is the reach.
struct Limb {
    bool found = false;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

Limb limb_of(const CharacterData& character, const Fighter& fighter) {
    // The free function, not the manifest: this library is built without RTTI
    // and the test binary with it, so constructing the polymorphic type here
    // leaves its typeinfo undefined at link time on GCC and Clang.
    SpriteList sprites{};
    placeholder_fighter_sprites(fighter, character, 0, sprites);

    // The body is quad 0. Any later quad narrower than the body is the guard
    // plate, so the limb is identified by being the one that extends forward
    // past the body's edge.
    Limb best;
    for (int32_t i = 1; i < sprites.count; ++i) {
        const auto& quad = sprites.quads[i];
        if (quad.width <= 0.0f) {
            continue;
        }
        if (!best.found || quad.width > best.w) {
            best = Limb{true, quad.offset_x, quad.offset_y, quad.width, quad.height};
        }
    }
    return best;
}

// The frame a move's hitbox is first live, which is when the limb is drawn at
// full extension.
int32_t first_active(const MoveData& move) {
    return move.startup + 1;
}

}  // namespace

TEST_CASE("Every attack draws a limb") {
    const MatchData& data = ds::test::shipped_match_data();
    const CharacterData& character = data.characters[0];

    for (int32_t m = 0; m < MOVE_COUNT; ++m) {
        const MoveId move = static_cast<MoveId>(m);
        const MoveData& md = move_of(character, move);
        INFO("move index " << m);

        const Limb limb = limb_of(character, attacking_on(move, first_active(md)));
        REQUIRE(limb.found);
        CHECK(limb.w > 0.0f);
        CHECK(limb.h > 0.0f);
    }
}

// THE REPORTED PROBLEM, as an assertion.
//
// Six buttons that draw the same rectangle are six buttons a player cannot
// tell apart, however different their frame data is.
TEST_CASE("Light, medium and heavy draw visibly different limbs") {
    const MatchData& data = ds::test::shipped_match_data();
    const CharacterData& character = data.characters[0];

    struct Family {
        const char* name;
        MoveId light;
        MoveId medium;
        MoveId heavy;
    };

    const Family families[] = {
        {"punch", MoveId::StandLightPunch, MoveId::StandMediumPunch, MoveId::StandHeavyPunch},
        {"kick", MoveId::StandLightKick, MoveId::StandMediumKick, MoveId::StandHeavyKick},
    };

    for (const Family& family : families) {
        INFO("family: " << std::string(family.name));

        const Limb light = limb_of(
            character, attacking_on(family.light, first_active(move_of(character, family.light))));
        const Limb medium =
            limb_of(character,
                    attacking_on(family.medium, first_active(move_of(character, family.medium))));
        const Limb heavy = limb_of(
            character, attacking_on(family.heavy, first_active(move_of(character, family.heavy))));

        REQUIRE(light.found);
        REQUIRE(medium.found);
        REQUIRE(heavy.found);

        // Strictly increasing, not merely different: a player has to be able to
        // read WHICH is heavier, not only that two things are not the same.
        CHECK(medium.w > light.w);
        CHECK(heavy.w > medium.w);
    }
}

TEST_CASE("A punch and a kick are drawn at different heights") {
    const MatchData& data = ds::test::shipped_match_data();
    const CharacterData& character = data.characters[0];

    const Limb punch =
        limb_of(character, attacking_on(MoveId::StandHeavyPunch,
                                        first_active(move_of(character, MoveId::StandHeavyPunch))));
    const Limb kick =
        limb_of(character, attacking_on(MoveId::StandHeavyKick,
                                        first_active(move_of(character, MoveId::StandHeavyKick))));

    REQUIRE(punch.found);
    REQUIRE(kick.found);
    CHECK(punch.y != kick.y);
}

// The guard against the jump-attack class of bug: the drawing is not allowed to
// be its own independent set of numbers.
TEST_CASE("The limb matches the move's real hitbox") {
    const MatchData& data = ds::test::shipped_match_data();
    const CharacterData& character = data.characters[0];

    for (int32_t m = 0; m < MOVE_COUNT; ++m) {
        const MoveId move = static_cast<MoveId>(m);
        const MoveData& md = move_of(character, move);
        INFO("move index " << m);

        const Limb limb = limb_of(character, attacking_on(move, first_active(md)));
        REQUIRE(limb.found);

        // Furthest-reaching hitbox of the move, which is what the limb draws.
        const Box* reach = nullptr;
        for (int32_t i = 0; i < md.hitbox_count; ++i) {
            const Box& box = md.hitboxes[i].box;
            if (box_is_empty(box)) {
                continue;
            }
            if (reach == nullptr || box.x + box.w > reach->x + reach->w) {
                reach = &box;
            }
        }
        REQUIRE(reach != nullptr);

        // Facing right, the limb runs from the body edge to the hitbox's far
        // edge, and stands at the hitbox's height and thickness.
        CHECK(limb.x + limb.w == doctest::Approx(static_cast<float>(reach->x + reach->w)));
        CHECK(limb.y == doctest::Approx(static_cast<float>(reach->y)));
        CHECK(limb.h == doctest::Approx(static_cast<float>(reach->h)));
    }
}

TEST_CASE("The limb extends through startup and retracts through recovery") {
    const MatchData& data = ds::test::shipped_match_data();
    const CharacterData& character = data.characters[0];
    const MoveData& md = move_of(character, MoveId::StandHeavyKick);

    const Limb early = limb_of(character, attacking_on(MoveId::StandHeavyKick, 1));
    const Limb live = limb_of(character, attacking_on(MoveId::StandHeavyKick, first_active(md)));
    const Limb late = limb_of(
        character, attacking_on(MoveId::StandHeavyKick, md.startup + md.active + md.recovery));

    REQUIRE(early.found);
    REQUIRE(live.found);

    // Full extension happens on active frames and nowhere else, so the picture
    // teaches the timing rather than only reporting that an attack happened.
    CHECK(early.w < live.w);
    if (late.found) {
        CHECK(late.w < live.w);
    }
}

// Reported from a playtest as "it doesn't seem like knockdowns are happening".
// They were happening perfectly. body_height_for knew only about crouching, so
// a fighter on the floor was drawn at full standing height with no tint --
// identical to one standing still.
//
// Third time the simulation has done something the picture did not show, after
// the jump attack and the six identical attack limbs.
TEST_CASE("A knocked-down fighter is drawn lying down") {
    const MatchData& data = ds::test::shipped_match_data();
    const CharacterData& character = data.characters[0];

    Fighter standing{};
    reset_fighter(standing, 0);
    standing.state = FighterState::Idle;

    Fighter downed = standing;
    downed.state = FighterState::Knockdown;

    Fighter rising = standing;
    rising.state = FighterState::Wakeup;

    auto body_of = [&](const Fighter& fighter) {
        SpriteList sprites{};
        placeholder_fighter_sprites(fighter, character, 0, sprites);
        REQUIRE(sprites.count > 0);
        return sprites.quads[0];  // The body is drawn first.
    };

    const auto up = body_of(standing);
    const auto down = body_of(downed);
    const auto mid = body_of(rising);

    // Lying down is a SHAPE before it is anything else: short and wide. Height
    // alone would read as a very small fighter rather than a horizontal one.
    CHECK(down.height < up.height * 0.5f);
    CHECK(down.width > up.width);

    // Getting up is visibly between the two, which is the cue an attacker times
    // against -- their window is closing, not already gone.
    CHECK(mid.height > down.height);
    CHECK(mid.height < up.height);
}
