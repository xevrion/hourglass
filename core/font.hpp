#pragma once

#include "surface.hpp"

namespace hg {

constexpr int kGlyphW = 5;
constexpr int kGlyphH = 7;

// Width in pixels of a string drawn at the given scale, one scaled pixel between glyphs.
int text_width(const char* s, int scale);
int text_height(int scale);

// Draws s with its top-left at (x, y). With rotate180 the whole block is turned
// half a revolution inside the same box, so it reads upright on a flipped board.
void draw_text(Surface& sf, int x, int y, const char* s, int scale, uint16_t colour, bool rotate180 = false);

}  // namespace hg
