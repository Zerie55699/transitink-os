#include "core/DisplayFontCore.h"

namespace transitink {

const char* displayFontId(DisplayFont font) {
    return font == DisplayFont::Unifont ? "unifont" : "noto_sans";
}

bool parseDisplayFontId(const std::string& value, DisplayFont& font) {
    if (value.empty() || value == "noto_sans") {
        font = DisplayFont::NotoSans;
        return true;
    }
    if (value == "unifont") {
        font = DisplayFont::Unifont;
        return true;
    }
    return false;
}

bool isDisplayFontSupported(DisplayFont font) {
    return font == DisplayFont::NotoSans || font == DisplayFont::Unifont;
}

}  // namespace transitink
