#include "render/sprite.h"

#include "sim/framedata.h"

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

// Feedback colours. Chosen to stay readable against both player colours rather
// than to look good -- these are diagnostic, and DESIGN.md 5.1 keeps v1 on
// placeholder boxes deliberately.
constexpr Color GUARD{0xC8, 0xD8, 0xF0, 0xFF};      // pale blue-white
constexpr Color HIT_FLASH{0xFF, 0xF2, 0xC0, 0xFF};  // hot cream
constexpr Color DOWNED{0x38, 0x34, 0x40, 0xFF};     // washed out

constexpr float LIMB_LENGTH = 26.0f;
constexpr float LIMB_THICKNESS = 12.0f;
constexpr float GUARD_THICKNESS = 5.0f;

uint8_t clamp_channel(int32_t value) {
    if (value < 0) {
        return 0u;
    }
    if (value > 255) {
        return 255u;
    }
    return static_cast<uint8_t>(value);
}

Color brighten(Color color, int32_t amount) {
    return Color{clamp_channel(color.r + amount), clamp_channel(color.g + amount),
                 clamp_channel(color.b + amount), color.a};
}

Color blend(Color from, Color to, float t) {
    const auto mix = [t](uint8_t a, uint8_t b) {
        return clamp_channel(static_cast<int32_t>(
            static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t));
    };
    return Color{mix(from.r, to.r), mix(from.g, to.g), mix(from.b, to.b), from.a};
}

// Kicks come out low, punches high. Only used to place the placeholder limb.
bool is_kick(int32_t move_id) {
    switch (static_cast<mw::sim::MoveId>(move_id)) {
        case mw::sim::MoveId::StandLowKick:
        case mw::sim::MoveId::StandHighKick:
        case mw::sim::MoveId::CrouchLowKick:
        case mw::sim::MoveId::CrouchHighKick:
            return true;
        default:
            return false;
    }
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
    using mw::sim::FighterState;

    out.count = 0;

    const float height = body_height_for(fighter);
    const bool player_one = player_index == 0;
    Color body = player_one ? P1_BODY : P2_BODY;

    // State shown through colour and an extended limb.
    //
    // DESIGN.md 5.1 asks for a text label above the fighter in debug builds.
    // There is no font path yet -- ADR 0012 puts Dear ImGui in the debug UI and
    // shipped text is not built -- so state is shown by tint instead. This is a
    // substitute for that line, not a replacement for it.
    //
    // It is not cosmetic. Until this existed, Blocking, Attack, Hitstun and
    // Blockstun all rendered identically to Idle: pressing block or punch
    // produced no visible change whatsoever, so the game read as ignoring the
    // input. Feel cannot be judged through a display that shows nothing
    // happening, and DESIGN.md 3 makes feel the anti-drift anchor.
    // An attacking fighter is not always in FighterState::Attack: a jump attack
    // happens while the state is Airborne, because the jump owns the state and
    // the move rides along. Keying purely off the state left jump attacks with
    // no visual indication at all -- the same invisibility that made blocking
    // look broken.
    const bool attacking = fighter.state == FighterState::Attack ||
                           (fighter.state == FighterState::Airborne && fighter.move_id >= 0);

    if (attacking) {
        body = brighten(body, 40);
    }

    switch (fighter.state) {
        case FighterState::Attack:
            break;
        case FighterState::Blocking:
            body = blend(body, GUARD, 0.45f);
            break;
        case FighterState::Hitstun:
            body = blend(body, HIT_FLASH, 0.65f);
            break;
        case FighterState::Blockstun:
            body = blend(body, GUARD, 0.25f);
            break;
        case FighterState::Win:
            body = brighten(body, 60);
            break;
        case FighterState::Lose:
            body = blend(body, DOWNED, 0.5f);
            break;
        default:
            break;
    }

    // Offsets are relative to the ground point, between the feet, with -y up
    // (ARCHITECTURE.md 3). The renderer converts to screen space; the manifest
    // never sees a pixel.
    sprite_list_push(out, untextured(-BODY_WIDTH * 0.5f, -height, BODY_WIDTH, height, body));

    // A limb, so an attack is visible as motion rather than only as a colour
    // shift. Length is fixed and does not consult frame data: this is a
    // placeholder telling you an attack is happening, and the F1 overlay is
    // where the real hitbox lives. Anything more here would be inventing
    // animation the design has not specified.
    if (attacking) {
        const float facing = static_cast<float>(static_cast<int32_t>(fighter.facing));
        const float limb_y = is_kick(fighter.move_id) ? -height * 0.35f : -height * 0.72f;
        const float limb_x = facing > 0.0f ? BODY_WIDTH * 0.5f : -BODY_WIDTH * 0.5f - LIMB_LENGTH;

        sprite_list_push(
            out, untextured(limb_x, limb_y, LIMB_LENGTH, LIMB_THICKNESS, brighten(body, 30)));
    }

    // A guard plate on the leading edge while blocking, so the defensive stance
    // reads at a glance and not only by tint.
    if (fighter.state == FighterState::Blocking || fighter.state == FighterState::Blockstun) {
        const float facing = static_cast<float>(static_cast<int32_t>(fighter.facing));
        const float plate_x =
            facing > 0.0f ? BODY_WIDTH * 0.5f - GUARD_THICKNESS : -BODY_WIDTH * 0.5f;

        sprite_list_push(
            out, untextured(plate_x, -height * 0.85f, GUARD_THICKNESS, height * 0.6f, GUARD));
    }

    // The notch sits on the leading edge, so it reads which way the fighter is
    // pointing at a glance. Facing is +1 right, -1 left.
    const float facing = static_cast<float>(static_cast<int32_t>(fighter.facing));
    const float notch_x = facing > 0.0f ? (BODY_WIDTH * 0.5f - NOTCH_WIDTH) : -BODY_WIDTH * 0.5f;

    sprite_list_push(out,
                     untextured(notch_x, -height * 0.75f, NOTCH_WIDTH, NOTCH_HEIGHT, FACING_NOTCH));
}

}  // namespace mw::render
