#include "render/readout.h"

#include <string>

#include "render/font.h"
#include "render/readout_text.h"
#include "sim/constants.h"

namespace ds::render {
namespace {

using namespace ds::sim;

constexpr SDL_Color TEXT{226, 230, 238, 255};
constexpr SDL_Color DIM{140, 148, 162, 255};
constexpr SDL_Color STARTUP{235, 200, 90, 255};
constexpr SDL_Color ACTIVE{240, 90, 90, 255};
constexpr SDL_Color RECOVERY{120, 150, 210, 255};
constexpr SDL_Color PANEL{12, 14, 20, 220};

constexpr float SCALE = 1.0f;
constexpr float LINE = 9.0f;
constexpr float PANEL_W = 150.0f;
constexpr float PANEL_H = 42.0f;
constexpr float MARGIN = 4.0f;

// Below the health bars and the round pips. The first version sat at the top
// and printed straight through them -- caught by screenshotting it, which is
// the only way a layout gets reviewed.
constexpr float PANEL_TOP = 38.0f;

void fill(SDL_Renderer* renderer, float x, float y, float w, float h, SDL_Color color) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    const SDL_FRect rect{x, y, w, h};
    SDL_RenderFillRect(renderer, &rect);
}

void draw_one(SDL_Renderer* renderer, const CharacterData& character, const Fighter& fighter,
              InputFrame input, float x, float y) {
    const ReadoutLines lines = readout_lines(character, fighter, input);

    SDL_Color detail_color = DIM;
    switch (lines.phase) {
        case ReadoutLines::Phase::Startup:
            detail_color = STARTUP;
            break;
        case ReadoutLines::Phase::Active:
            detail_color = ACTIVE;
            break;
        case ReadoutLines::Phase::Recovery:
            detail_color = RECOVERY;
            break;
        case ReadoutLines::Phase::None:
            break;
    }

    fill(renderer, x, y, PANEL_W, PANEL_H, PANEL);

    draw_text(renderer, x + 3.0f, y + 3.0f, lines.state, SCALE, TEXT);
    if (!lines.detail.empty()) {
        draw_text(renderer, x + 3.0f, y + 3.0f + LINE, lines.detail, SCALE, detail_color);
    }
    if (!lines.numbers.empty()) {
        draw_text(renderer, x + 3.0f, y + 3.0f + LINE * 2.0f, lines.numbers, SCALE, DIM);
    }
    draw_text(renderer, x + 3.0f, y + PANEL_H - LINE - 2.0f, lines.inputs, SCALE, TEXT);
}

}  // namespace

void draw_readout(SDL_Renderer* renderer, const MatchData& data, const GameState& state,
                  InputPair inputs) {
    const float right = static_cast<float>(SCREEN_WIDTH) - PANEL_W - MARGIN;

    // Each panel sits on its own player's side, so the reader never has to work
    // out which column belongs to them.
    draw_one(renderer, data.characters[0], state.fighters[0], inputs.players[0], MARGIN, PANEL_TOP);
    draw_one(renderer, data.characters[1], state.fighters[1], inputs.players[1], right, PANEL_TOP);
}

}  // namespace ds::render
