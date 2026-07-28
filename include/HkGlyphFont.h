#pragma once

#include <cstddef>
#include <cstdint>

#include "core/DisplayFontCore.h"

struct HkGlyph {
    uint16_t codepoint;
    uint8_t width;
    uint16_t rows[16];
};

extern const HkGlyph kHkGlyphs[];
extern const size_t kHkGlyphCount;
extern const HkGlyph kUnifontGlyphs[];
extern const size_t kUnifontGlyphCount;

void setActiveDisplayFont(transitink::DisplayFont font);
transitink::DisplayFont activeDisplayFont();
const HkGlyph* findHkGlyph(uint16_t codepoint);
