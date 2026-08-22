#include "render/sprite.h"

namespace mw::render {
namespace {

// DESIGN.md 5.1: fighter body is a solid rectangle in a per-player color, with
// a white notch on the leading edge showing facing.
//
// Colour keys off player_index, never off facing: facing flips whenever the
// fighters cross up, so keying on it would swap the two players' colours
// mid-match.
constexpr Color P1_BODY{0x4A, 0x90, 0xD9, 0xFF};  // blue
constexpr Color P2_BODY{0xD9, 0x5F, 0x4A, 0xFF};  // rust
constexpr Color FACING_NOTCH{0xFF, 0xFF, 0xFF, 0xFF};

// DESIGN.md 4.4 gives fighter standing height as ~140 units.
//
// PROVISIONAL, and known to be wrong: width is specified nowhere. The 32 here
// matches the standing hurtbox sketched in docs/framedata_schema.md, but that
// sketch was itself a guess, so this is a guess agreeing with a guess.
//
// It shows. 32x140 renders as a narrow pillar, and DESIGN.md 5.3 asks for
// wombats that are "round, heavy, and short-limbed" with a silhouette that
// reads at small sizes -- close to the opposite proportion. Fixing it properly
// needs DESIGN.md 5.4, which has not named the cast and says not to invent it.
//
// Left visibly wrong on purpose rather than quietly tuned to something
// plausible: a placeholder that looks unfinished prompts the decision, and one
// that looks fine gets shipped by default.
constexpr float BODY_WIDTH = 32.0f;
constexpr float BODY_HEIGHT = 140.0f;
constexpr float CROUCH_HEIGHT = 80.0f;

constexpr float NOTCH_WIDTH = 6.0f;
constexpr float NOTCH_HEIGHT = 10.0f;

// A crouching fighter is shorter. Everything else about the placeholder is
// state-independent until frame data exists to describe it — deliberately, so
// that nothing here has to be unlearned when real boxes arrive.
float body_height_for(const mw::sim::Fighter& fighter) {
    return fighter.state == mw::sim::FighterState::Crouch ? CROUCH_HEIGHT : BODY_HEIGHT;
}

SpriteQuad untextured(float x, float y, float w, float h, Color tint) {
    SpriteQuad quad{};
    quad.offset_x = x;
    quad.offset_y = y;
    quad.width = w;
    quad.height = h;
    quad.tint = tint;
    quad.texture = nullptr;  // the whole placeholder path, in one field
    return quad;
}

}  // namespace

void PlaceholderManifest::fighter_sprites(const mw::sim::Fighter& fighter, int32_t player_index,
                                          SpriteList& out) const {
    out.count = 0;

    const float height = body_height_for(fighter);
    const bool player_one = player_index == 0;

    // Offsets are relative to the ground point, between the feet, with -y up
    // (ARCHITECTURE.md 3). The renderer converts to screen space; the manifest
    // never sees a pixel.
    sprite_list_push(out, untextured(-BODY_WIDTH * 0.5f, -height, BODY_WIDTH, height,
                                     player_one ? P1_BODY : P2_BODY));

    // The notch sits on the leading edge, so it reads which way the fighter is
    // pointing at a glance. Facing is +1 right, -1 left.
    const float facing = static_cast<float>(static_cast<int32_t>(fighter.facing));
    const float notch_x = facing > 0.0f ? (BODY_WIDTH * 0.5f - NOTCH_WIDTH) : -BODY_WIDTH * 0.5f;

    sprite_list_push(out,
                     untextured(notch_x, -height * 0.75f, NOTCH_WIDTH, NOTCH_HEIGHT, FACING_NOTCH));
}

}  // namespace mw::render
