#pragma once

#include <cstdint>
#include <string>

namespace transitink {

enum class DisplayFont : uint8_t {
    NotoSans,
    Unifont,
};

const char* displayFontId(DisplayFont font);
bool parseDisplayFontId(const std::string& value, DisplayFont& font);
bool isDisplayFontSupported(DisplayFont font);

}  // namespace transitink
