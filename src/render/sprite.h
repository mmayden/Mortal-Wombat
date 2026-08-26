// The swappable seam between the renderer and whatever supplies its pixels.
//
// ADR 0013 is explicit: "the renderer must treat sprites as swappable from day
// one: it draws from a sprite manifest, and the placeholder path is one
// manifest implementation. No rendering code assumes rectangles."
//
// So this file exists at bootstrap, before there is any art, specifically so
// that the renderer never learns what a placeholder box is. It asks the
// manifest what to draw for a fighter and draws that. Today every answer is an
// untextured quad. When the Blender pipeline lands (ADR 0013 v2), a manifest
// that returns atlas rects replaces this one and renderer.cpp does not change.
//
// This is above the sim boundary, so floats are fine here.
#pragma once

#include <cstdint>

#include "sim/framedata.h"
#include "sim/state.h"

struct SDL_Texture;

namespace ds::render {

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

// One drawable. Position is in world units, relative to the fighter's ground
// point, and already mirrored for facing by the manifest.
//
// `texture` null means "no art for this yet" — the renderer fills the quad with
// `tint` instead of sampling. That is the entire placeholder path, and it is
// the only branch in the drawing code that knows about it.
struct SpriteQuad {
    float offset_x;
    float offset_y;
    float width;
    float height;
    Color tint;

    SDL_Texture* texture;
    // Source rect in the atlas, in pixels. Ignored when texture is null.
    float source_x;
    float source_y;
    float source_w;
    float source_h;
};

// How many quads one fighter can contribute in a frame. Body, plus the facing
// notch, plus room for the debug boxes once frame data exists.
inline constexpr int32_t MAX_QUADS_PER_FIGHTER = 8;

struct SpriteList {
    SpriteQuad quads[MAX_QUADS_PER_FIGHTER];
    int32_t count;
};

inline void sprite_list_push(SpriteList& list, const SpriteQuad& quad) {
    if (list.count < MAX_QUADS_PER_FIGHTER) {
        list.quads[list.count] = quad;
        ++list.count;
    }
}

// The manifest interface. One level of inheritance, which is what ADR 0001
// permits — the rule forbids going deeper, not using a seam.
class SpriteManifest {
public:
    virtual ~SpriteManifest() = default;

    SpriteManifest(const SpriteManifest&) = delete;
    SpriteManifest& operator=(const SpriteManifest&) = delete;

    // Fills `out` with everything to draw for this fighter this frame.
    //
    // `player_index` is passed separately because it is the fighter's stable
    // identity. Anything player-specific -- colour now, character art later --
    // must key off this and never off `facing`, which flips on every cross-up.
    //
    // `character` is here so the drawing can be DERIVED from frame data rather
    // than guessed alongside it. Without it the limb was a fixed 26 units for
    // every attack, so a light punch and a heavy kick looked identical while
    // reaching 46 and 77 -- a picture that disagreed with the simulation, which
    // is the same failure that hid the jump attack bug for weeks.
    virtual void fighter_sprites(const ds::sim::Fighter& fighter,
                                 const ds::sim::CharacterData& character, int32_t player_index,
                                 SpriteList& out) const = 0;

protected:
    SpriteManifest() = default;
};

// v1's manifest: solid boxes. DESIGN.md 5.1 calls this "a decision, not a
// placeholder apology" — feel is entirely frame data, and colored rectangles
// feel identical to finished sprites while costing nothing to iterate on.
class PlaceholderManifest final : public SpriteManifest {
public:
    void fighter_sprites(const ds::sim::Fighter& fighter, const ds::sim::CharacterData& character,
                         int32_t player_index, SpriteList& out) const override;
};

// The placeholder manifest's whole body, as a free function.
//
// PlaceholderManifest delegates to this and adds nothing. The split exists so
// tests can call it without touching the polymorphic type: this library is
// built -fno-rtti (ADR 0001) so it emits no typeinfo, while the test binary is
// built with RTTI for doctest, and constructing the class across that boundary
// leaves an undefined reference to its typeinfo at link time. Green on MSVC,
// red on GCC and Clang, like the four before it.
//
// Testing the function rather than the interface is the better shape anyway --
// building a sprite list is a pure transformation, and the virtual exists only
// so real art can replace it later.
void placeholder_fighter_sprites(const ds::sim::Fighter& fighter,
                                 const ds::sim::CharacterData& character, int32_t player_index,
                                 SpriteList& out);

}  // namespace ds::render
