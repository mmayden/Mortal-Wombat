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
// Translucent, not merely dark. At 220 alpha these panels were effectively
// solid, and two cases nobody had screenshotted went straight behind them: a
// fighter at the apex of a jump reaches y=10, well above the readout at y=38,
// and a fighter pinned against the left wall sits at x=0..32 underneath the
// input list at x=4..66.
//
// Both were measured rather than seen, because the only screenshot taken was
// the easy case -- standing fighters, mid-stage, one row of history. A
// diagnostic overlay that hides the thing being diagnosed is the defect this
// file has now had three times.
constexpr SDL_Color PANEL{12, 14, 20, 130};

constexpr float SCALE = 1.0f;
constexpr float LINE = 9.0f;
constexpr float PANEL_W = 150.0f;
constexpr float PANEL_H = 42.0f;
constexpr float MARGIN = 4.0f;

// Below the health bars and the round pips. The first version sat at the top
// and printed straight through them -- caught by screenshotting it, which is
// the only way a layout gets reviewed.
constexpr float PANEL_TOP = 38.0f;

// Narrow enough to sit in the screen margin beside the fighters rather than
// over them. A row is at most "D LP HK 12", which fits.
constexpr float HISTORY_W = 62.0f;

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

// The scrolling list, newest at the bottom so the eye lands on the most recent
// input without scanning -- the arrangement every training mode uses.
//
// A held input is one row with a frame count, not one row per frame. That is
// what makes the list readable, and what makes an input stay on screen until
// the next one arrives.
void draw_history(SDL_Renderer* renderer, const InputHistory& history, float x, float top) {
    constexpr int32_t VISIBLE = 14;

    const int32_t shown = history.count < VISIBLE ? history.count : VISIBLE;
    if (shown <= 0) {
        return;
    }

    // Sized to its contents and kept narrow. The first version was a fixed
    // 150x130 slab that sat over both fighters -- caught by screenshotting it,
    // which is the only way a layout gets reviewed. A diagnostic that hides the
    // thing being diagnosed is worse than no diagnostic.
    const float height = static_cast<float>(shown) * LINE + 4.0f;
    fill(renderer, x, top, HISTORY_W, height, PANEL);
    const int32_t first = history.count - shown;

    for (int32_t i = 0; i < shown; ++i) {
        const InputHistoryEntry& entry = input_history_at(history, first + i);

        // Newest row bright, older ones dim, so "what did I just press" is
        // answerable without reading.
        const bool newest = (first + i) == history.count - 1;
        const SDL_Color color = newest ? TEXT : DIM;

        const float row_y = top + 2.0f + static_cast<float>(i) * LINE;
        const float used =
            draw_text(renderer, x + 3.0f, row_y, input_history_label(entry.buttons), SCALE, color);

        // Frames held. This is the number that turns "I pressed back" into
        // "I held back for forty frames".
        draw_text(renderer, x + 6.0f + used, row_y, std::to_string(entry.frames_held), SCALE, DIM);
    }
}

}  // namespace

void draw_readout(SDL_Renderer* renderer, const MatchData& data, const GameState& state,
                  InputPair inputs, const InputHistory (&history)[2]) {
    const float right = static_cast<float>(SCREEN_WIDTH) - PANEL_W - MARGIN;

    // Each panel sits on its own player's side, so the reader never has to work
    // out which column belongs to them.
    draw_one(renderer, data.characters[0], state.fighters[0], inputs.players[0], MARGIN, PANEL_TOP);
    draw_one(renderer, data.characters[1], state.fighters[1], inputs.players[1], right, PANEL_TOP);

    const float history_top = PANEL_TOP + PANEL_H + 4.0f;
    draw_history(renderer, history[0], MARGIN, history_top);
    draw_history(renderer, history[1], static_cast<float>(SCREEN_WIDTH) - HISTORY_W - MARGIN,
                 history_top);
}

}  // namespace ds::render
