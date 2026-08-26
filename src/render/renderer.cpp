#include "render/renderer.h"

#include <SDL3/SDL.h>

#include "render/camera.h"
#include "render/readout.h"
#include "sim/constants.h"
#include "sim/sim.h"

namespace ds::render {
namespace {

using ds::sim::Fighter;
using ds::sim::FighterState;
using ds::sim::GameState;

// DESIGN.md 5.3: period-appropriate UI — chunky bars, heavy shapes.
constexpr Color BACKDROP{0x1A, 0x18, 0x22, 0xFF};
constexpr Color GROUND{0x33, 0x2C, 0x3A, 0xFF};
constexpr Color STAGE_EDGE{0x55, 0x48, 0x60, 0xFF};

constexpr Color HEALTH_FILL{0xE8, 0xC0, 0x4A, 0xFF};
constexpr Color HEALTH_EMPTY{0x4A, 0x1E, 0x1E, 0xFF};
constexpr Color HEALTH_FRAME{0xEC, 0xE8, 0xE0, 0xFF};
constexpr Color ROUND_PIP{0xE8, 0xC0, 0x4A, 0xFF};
constexpr Color TIMER_BAR{0xEC, 0xE8, 0xE0, 0xFF};
constexpr Color DEBUG_ORIGIN{0x50, 0xE0, 0x70, 0xFF};
constexpr Color DEBUG_HURTBOX{0x4A, 0x9C, 0xFF, 0xFF};      // blue
constexpr Color DEBUG_HITBOX{0xFF, 0x3A, 0x3A, 0xFF};       // red
constexpr Color DEBUG_HITBOX_IDLE{0x8A, 0x2A, 0x2A, 0xFF};  // red, dimmed
constexpr Color DEBUG_PUSHBOX{0xE8, 0xD0, 0x4A, 0xFF};      // yellow

// DESIGN.md 5.5 has not settled the stage yet and says not to invent one, so
// this is a flat backdrop and a ground line — the minimum that makes position
// readable — rather than a guess at what the stage looks like.
constexpr float HUD_MARGIN = 8.0f;
constexpr float HEALTH_BAR_WIDTH = 200.0f;
constexpr float HEALTH_BAR_HEIGHT = 14.0f;
constexpr float ROUND_PIP_SIZE = 7.0f;

void set_color(SDL_Renderer* renderer, Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void fill(SDL_Renderer* renderer, float x, float y, float w, float h, Color color) {
    set_color(renderer, color);
    const SDL_FRect rect{x, y, w, h};
    SDL_RenderFillRect(renderer, &rect);
}

void outline(SDL_Renderer* renderer, float x, float y, float w, float h, Color color) {
    set_color(renderer, color);
    const SDL_FRect rect{x, y, w, h};
    SDL_RenderRect(renderer, &rect);
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float to_float(ds::sim::Fixed value) {
    // The one-way crossing. Below this line everything is exact integer
    // arithmetic; above it, pixels. Conversion happens here and never in the
    // other direction (ARCHITECTURE.md 1).
    return static_cast<float>(value.raw) / static_cast<float>(ds::sim::FIXED_ONE);
}

void draw_stage(SDL_Renderer* renderer, float camera) {
    const float screen_w = static_cast<float>(ds::sim::SCREEN_WIDTH);
    const float screen_h = static_cast<float>(ds::sim::SCREEN_HEIGHT);
    const float ground_y = static_cast<float>(ds::sim::GROUND_Y);

    fill(renderer, 0.0f, 0.0f, screen_w, screen_h, BACKDROP);
    fill(renderer, 0.0f, ground_y, screen_w, screen_h - ground_y, GROUND);

    // The stage walls, so that walking into one is visibly a wall rather than
    // the fighter mysteriously refusing to move.
    const float left_wall = static_cast<float>(ds::sim::STAGE_LEFT_BOUND) - camera;
    const float right_wall = static_cast<float>(ds::sim::STAGE_RIGHT_BOUND) - camera;
    fill(renderer, left_wall - 3.0f, 0.0f, 3.0f, ground_y, STAGE_EDGE);
    fill(renderer, right_wall, 0.0f, 3.0f, ground_y, STAGE_EDGE);
}

void draw_sprite(SDL_Renderer* renderer, const SpriteQuad& quad, float origin_x, float origin_y) {
    const SDL_FRect destination{origin_x + quad.offset_x, origin_y + quad.offset_y, quad.width,
                                quad.height};

    if (quad.texture == nullptr) {
        // The placeholder path (ADR 0013). This is the ONLY place that knows
        // art might be missing; everything upstream just asked the manifest
        // what to draw.
        set_color(renderer, quad.tint);
        SDL_RenderFillRect(renderer, &destination);
        return;
    }

    const SDL_FRect source{quad.source_x, quad.source_y, quad.source_w, quad.source_h};
    SDL_SetTextureColorMod(quad.texture, quad.tint.r, quad.tint.g, quad.tint.b);
    SDL_SetTextureAlphaMod(quad.texture, quad.tint.a);
    SDL_RenderTexture(renderer, quad.texture, &source, &destination);
}

void draw_fighter(SDL_Renderer* renderer, const SpriteManifest& manifest,
                  const ds::sim::CharacterData& character, const Fighter& previous,
                  const Fighter& current, int32_t player_index, float alpha, float camera,
                  bool show_debug) {
    // Interpolate position only. State, facing, and everything discrete comes
    // from the current frame — interpolating a state machine produces
    // in-between states that never existed.
    const float x = lerp(to_float(previous.x), to_float(current.x), alpha) - camera;
    const float y = lerp(to_float(previous.y), to_float(current.y), alpha);

    SpriteList sprites{};
    manifest.fighter_sprites(current, character, player_index, sprites);

    for (int32_t i = 0; i < sprites.count; ++i) {
        draw_sprite(renderer, sprites.quads[i], x, y);
    }

    if (show_debug) {
        // The ground point, so that "where is this fighter actually" is
        // answerable when the body box and the origin disagree.
        fill(renderer, x - 1.0f, y - 1.0f, 3.0f, 3.0f, DEBUG_ORIGIN);
    }
}

// The debug box overlay. DESIGN.md 5.1 gives the colours:
//
//   Hurtbox  blue outline
//   Hitbox   red outline, filled while active
//   Pushbox  yellow outline
//
// The stack decision calls the hitbox viewer the project's debugging
// environment, and it doubles as a shipped training-mode feature. It is how
// "why did that miss?" stops needing a debugger.
//
// Boxes are drawn from the CURRENT frame without interpolation, deliberately.
// An interpolated hitbox is a lie: it would show the box somewhere it never
// was on any simulation frame, which is precisely the question this overlay
// exists to answer.
void draw_debug_boxes(SDL_Renderer* renderer, const ds::sim::MatchData& data,
                      const GameState& state, float camera) {
    for (int32_t i = 0; i < 2; ++i) {
        const Fighter& fighter = state.fighters[i];
        const ds::sim::CharacterData& character = data.characters[i];

        auto draw_box = [&](const ds::sim::Box& local, Color color, bool filled) {
            const ds::sim::Box box = ds::sim::world_box(fighter, local);
            const float x = static_cast<float>(box.x) - camera;
            const float y = static_cast<float>(box.y);
            const float w = static_cast<float>(box.w);
            const float h = static_cast<float>(box.h);

            if (filled) {
                set_color(renderer, Color{color.r, color.g, color.b, 0x80});
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                const SDL_FRect rect{x, y, w, h};
                SDL_RenderFillRect(renderer, &rect);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            }
            outline(renderer, x, y, w, h, color);
        };

        draw_box(character.pushbox, DEBUG_PUSHBOX, false);

        // The hurtbox actually presented this frame, including any per-move
        // override -- not the character default, which would mislead on exactly
        // the frames that matter.
        ds::sim::Box hurtbox = character.standing_hurtbox;
        if (fighter.state == FighterState::Crouch) {
            hurtbox = character.crouching_hurtbox;
        }
        if (fighter.move_id >= 0) {
            const ds::sim::MoveData& move =
                ds::sim::move_of(character, static_cast<ds::sim::MoveId>(fighter.move_id));
            if (!ds::sim::box_is_empty(move.hurtbox_override)) {
                hurtbox = move.hurtbox_override;
            }
        }
        draw_box(hurtbox, DEBUG_HURTBOX, false);

        // move_id, not the state: a jump attack runs while the state is
        // Airborne, and an overlay that hides those hitboxes is worst exactly
        // where it is needed most -- air-to-ground spacing is the hardest thing
        // in a fighting game to judge by eye.
        if (fighter.move_id < 0) {
            continue;
        }

        const ds::sim::MoveData& move =
            ds::sim::move_of(character, static_cast<ds::sim::MoveId>(fighter.move_id));

        for (int32_t h = 0; h < move.hitbox_count; ++h) {
            const ds::sim::HitboxSpan& span = move.hitboxes[h];
            const bool live =
                fighter.move_frame >= span.first_frame && fighter.move_frame <= span.last_frame;

            // Inactive hitboxes are drawn too, dimmed. Seeing where a move WILL
            // hit during its startup is most of what makes frame data legible.
            draw_box(span.box, live ? DEBUG_HITBOX : DEBUG_HITBOX_IDLE, live);
        }
    }
}

// DESIGN.md 5.3: chunky health bars, large centered timer.
//
// The timer is a shrinking bar rather than digits: there is no font path yet
// (ADR 0012 puts Dear ImGui in the debug UI, and shipped text is not built),
// and a fake number would be worse than an honest bar.
void draw_hud(SDL_Renderer* renderer, const GameState& state) {
    const float screen_w = static_cast<float>(ds::sim::SCREEN_WIDTH);

    for (int32_t i = 0; i < 2; ++i) {
        const float fraction = static_cast<float>(state.fighters[i].health) /
                               static_cast<float>(ds::sim::STARTING_HEALTH);
        const float clamped = fraction < 0.0f ? 0.0f : (fraction > 1.0f ? 1.0f : fraction);

        const float bar_x = i == 0 ? HUD_MARGIN : screen_w - HUD_MARGIN - HEALTH_BAR_WIDTH;
        const float bar_y = HUD_MARGIN;

        fill(renderer, bar_x, bar_y, HEALTH_BAR_WIDTH, HEALTH_BAR_HEIGHT, HEALTH_EMPTY);

        // Player 1's bar drains toward the centre of the screen and player 2's
        // toward its own side, the way every fighting game does it, so that
        // both bars empty inward.
        const float filled = HEALTH_BAR_WIDTH * clamped;
        const float fill_x = i == 0 ? bar_x : bar_x + (HEALTH_BAR_WIDTH - filled);
        fill(renderer, fill_x, bar_y, filled, HEALTH_BAR_HEIGHT, HEALTH_FILL);
        outline(renderer, bar_x, bar_y, HEALTH_BAR_WIDTH, HEALTH_BAR_HEIGHT, HEALTH_FRAME);

        // Rounds won, as pips under the bar. Best of three (DESIGN.md 4.4).
        for (int32_t pip = 0; pip < ds::sim::ROUNDS_TO_WIN; ++pip) {
            const float pip_x = i == 0 ? bar_x + static_cast<float>(pip) * (ROUND_PIP_SIZE + 3.0f)
                                       : bar_x + HEALTH_BAR_WIDTH - ROUND_PIP_SIZE -
                                             static_cast<float>(pip) * (ROUND_PIP_SIZE + 3.0f);
            const float pip_y = bar_y + HEALTH_BAR_HEIGHT + 3.0f;

            if (pip < state.rounds_won[i]) {
                fill(renderer, pip_x, pip_y, ROUND_PIP_SIZE, ROUND_PIP_SIZE, ROUND_PIP);
            } else {
                outline(renderer, pip_x, pip_y, ROUND_PIP_SIZE, ROUND_PIP_SIZE, HEALTH_FRAME);
            }
        }
    }

    const float timer_fraction =
        static_cast<float>(state.round_timer) / static_cast<float>(ds::sim::ROUND_TIMER_FRAMES);
    const float timer_width = 64.0f * timer_fraction;
    fill(renderer, (screen_w - 64.0f) * 0.5f, HUD_MARGIN, 64.0f, 6.0f, HEALTH_EMPTY);
    fill(renderer, (screen_w - timer_width) * 0.5f, HUD_MARGIN, timer_width, 6.0f, TIMER_BAR);
}

}  // namespace

void draw_frame(SDL_Renderer* renderer, const SpriteManifest& manifest,
                const ds::sim::MatchData& data, const Camera& view, const GameState& previous,
                const GameState& current, float alpha, bool show_debug, ds::sim::InputPair inputs,
                const InputHistory (&history)[2]) {
    const float camera = view.x;

    draw_stage(renderer, camera);

    for (int32_t i = 0; i < 2; ++i) {
        draw_fighter(renderer, manifest, data.characters[i], previous.fighters[i],
                     current.fighters[i], i, alpha, camera, show_debug);
    }

    if (show_debug) {
        draw_debug_boxes(renderer, data, current, camera);
        draw_readout(renderer, data, current, inputs, history);
    }

    draw_hud(renderer, current);
}

}  // namespace ds::render
