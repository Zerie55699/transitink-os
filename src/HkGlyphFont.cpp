#include "HkGlyphFont.h"

namespace {

transitink::DisplayFont selectedFont = transitink::DisplayFont::NotoSans;

}  // namespace

void setActiveDisplayFont(transitink::DisplayFont font) {
    selectedFont = transitink::isDisplayFontSupported(font)
                       ? font
                       : transitink::DisplayFont::NotoSans;
}

transitink::DisplayFont activeDisplayFont() {
    return selectedFont;
}

const HkGlyph* findHkGlyph(uint16_t codepoint) {
    const HkGlyph* glyphs = selectedFont == transitink::DisplayFont::Unifont
                                ? kUnifontGlyphs
                                : kHkGlyphs;
    const size_t glyphCount = selectedFont == transitink::DisplayFont::Unifont
                                  ? kUnifontGlyphCount
                                  : kHkGlyphCount;
    size_t low = 0;
    size_t high = glyphCount;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (glyphs[mid].codepoint == codepoint) {
            return &glyphs[mid];
        }
        if (glyphs[mid].codepoint < codepoint) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return nullptr;
}
