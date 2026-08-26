// A 5x7 bitmap font, drawn as rectangles.
//
// The project has no text-rendering path. ADR 0012 plans Dear ImGui for debug
// UI and it is not integrated, and AGENTS rule 4 forbids adding a dependency
// without asking -- so this tool would otherwise have had to print everything
// to a console the user is not looking at while they study a hitbox.
//
// Forty-odd glyphs of five bits by seven rows is not a font system. It is the
// smallest thing that makes the tool answer its own question on screen. If it
// proves useful enough to want in the game's training mode, promoting it is a
// decision for ADR 0012 to make, not something to assume by moving the file.
#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>

namespace ds::tools {

inline constexpr int32_t GLYPH_W = 5;
inline constexpr int32_t GLYPH_H = 7;

// Each glyph is seven rows; each row's low five bits are pixels, left to right
// from bit 4. Written as binary literals so the shapes are legible in source --
// the point of a hand-made font is that a wrong letter is visible here.
struct Glyph {
    char code;
    uint8_t rows[GLYPH_H];
};

inline constexpr Glyph FONT[] = {
    {' ', {0, 0, 0, 0, 0, 0, 0}},
    {'A', {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001}},
    {'B', {0b11110, 0b10001, 0b11110, 0b10001, 0b10001, 0b10001, 0b11110}},
    {'C', {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110}},
    {'D', {0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110}},
    {'E', {0b11111, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000, 0b11111}},
    {'F', {0b11111, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000, 0b10000}},
    {'G', {0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01111}},
    {'H', {0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001, 0b10001}},
    {'I', {0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}},
    {'J', {0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100}},
    {'K', {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001}},
    {'L', {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111}},
    {'M', {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001}},
    {'N', {0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001}},
    {'O', {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
    {'P', {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000}},
    {'Q', {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101}},
    {'R', {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001}},
    {'S', {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110}},
    {'T', {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100}},
    {'U', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110}},
    {'V', {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100}},
    {'W', {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b11011, 0b10001}},
    {'X', {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001}},
    {'Y', {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100}},
    {'Z', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111}},
    {'0', {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110}},
    {'1', {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}},
    {'2', {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111}},
    {'3', {0b11111, 0b00010, 0b00100, 0b00010, 0b00001, 0b10001, 0b01110}},
    {'4', {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010}},
    {'5', {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110}},
    {'6', {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110}},
    {'7', {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000}},
    {'8', {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110}},
    {'9', {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100}},
    {'-', {0, 0, 0, 0b11111, 0, 0, 0}},
    {':', {0, 0b00100, 0b00100, 0, 0b00100, 0b00100, 0}},
    {'/', {0b00001, 0b00010, 0b00010, 0b00100, 0b01000, 0b01000, 0b10000}},
    {'.', {0, 0, 0, 0, 0, 0b01100, 0b01100}},
    {',', {0, 0, 0, 0, 0b01100, 0b00100, 0b01000}},
    {'+', {0, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0}},
    {'!', {0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0, 0b00100}},
    {'(', {0b00010, 0b00100, 0b01000, 0b01000, 0b01000, 0b00100, 0b00010}},
    {')', {0b01000, 0b00100, 0b00010, 0b00010, 0b00010, 0b00100, 0b01000}},
};

inline const Glyph* find_glyph(char c) {
    if (c >= 'a' && c <= 'z') {
        c = static_cast<char>(c - 'a' + 'A');
    }
    for (const Glyph& glyph : FONT) {
        if (glyph.code == c) {
            return &glyph;
        }
    }
    return nullptr;  // Unknown characters draw as a gap, never as a wrong letter.
}

// Draws `text` with its top-left at (x, y), each font pixel `scale` screen
// pixels square. Returns the width drawn, so callers can lay out a line.
inline float draw_text(SDL_Renderer* renderer, float x, float y, const std::string& text,
                       float scale, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    float pen = x;
    for (const char c : text) {
        const Glyph* glyph = find_glyph(c);
        if (glyph != nullptr) {
            for (int32_t row = 0; row < GLYPH_H; ++row) {
                for (int32_t col = 0; col < GLYPH_W; ++col) {
                    if ((glyph->rows[row] & (1u << (GLYPH_W - 1 - col))) == 0u) {
                        continue;
                    }
                    const SDL_FRect pixel{pen + static_cast<float>(col) * scale,
                                          y + static_cast<float>(row) * scale, scale, scale};
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }
        pen += static_cast<float>(GLYPH_W + 1) * scale;
    }
    return pen - x;
}

}  // namespace ds::tools
