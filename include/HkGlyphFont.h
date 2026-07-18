#pragma once

#include <cstddef>
#include <cstdint>

struct HkGlyph {
    uint16_t codepoint;
    uint8_t width;
    uint16_t rows[16];
};

extern const HkGlyph kHkGlyphs[];
extern const size_t kHkGlyphCount;

const HkGlyph* findHkGlyph(uint16_t codepoint);
