#include "HkGlyphFont.h"

const HkGlyph* findHkGlyph(uint16_t codepoint) {
    size_t low = 0;
    size_t high = kHkGlyphCount;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (kHkGlyphs[mid].codepoint == codepoint) {
            return &kHkGlyphs[mid];
        }
        if (kHkGlyphs[mid].codepoint < codepoint) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return nullptr;
}
