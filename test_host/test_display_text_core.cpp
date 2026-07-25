#include "core/DisplayTextCore.h"

#include <cassert>
#include <cstdint>
#include <string>

namespace {

struct Widths {
    int ellipsis = 6;
    int dot = 2;
};

int glyphWidth(uint32_t codepoint, void* context) {
    const auto& widths = *static_cast<const Widths*>(context);
    if (codepoint == 0x2026) {
        return widths.ellipsis;
    }
    if (codepoint == '.') {
        return widths.dot;
    }
    if (codepoint == 0x2603 || codepoint == '\r' || codepoint == '\n') {
        return 0;
    }
    if (codepoint >= 0x4E00 && codepoint <= 0x9FFF) {
        return 8;
    }
    return 4;
}

void assertPlan(const transitink::DisplayTextPlan& plan,
                const std::string& expected,
                int expectedWidth,
                int maxWidth) {
    assert(plan.text == expected);
    assert(plan.pixelWidth == expectedWidth);
    assert(plan.pixelWidth <= maxWidth);
}

}  // namespace

int main() {
    Widths widths;

    assertPlan(transitink::planTruncatedUtf8("AB", 8, glyphWidth, &widths), "AB", 8, 8);
    assertPlan(transitink::planTruncatedUtf8("港鐵", 16, glyphWidth, &widths), "港鐵", 16, 16);
    assertPlan(transitink::planTruncatedUtf8("ABCDE", 10, glyphWidth, &widths), "A…", 10, 10);

    widths.ellipsis = 0;
    assertPlan(transitink::planTruncatedUtf8("ABCDE", 10, glyphWidth, &widths), "A...", 10, 10);
    assertPlan(transitink::planTruncatedUtf8("ABCDE", 5, glyphWidth, &widths), "..", 4, 5);
    assertPlan(transitink::planTruncatedUtf8("ABCDE", 3, glyphWidth, &widths), ".", 2, 3);
    assertPlan(transitink::planTruncatedUtf8("ABCDE", 1, glyphWidth, &widths), "", 0, 1);

    assertPlan(transitink::planTruncatedUtf8("A☃B", 8, glyphWidth, &widths), "AB", 8, 8);

    widths.ellipsis = 6;
    const auto multibyte = transitink::planTruncatedUtf8("港鐵A", 14, glyphWidth, &widths);
    assertPlan(multibyte, "港…", 14, 14);
    assert(multibyte.truncated);

    widths.ellipsis = 0;
    widths.dot = 0;
    assertPlan(transitink::planTruncatedUtf8("A", 2, glyphWidth, &widths), "", 0, 2);
    assert(transitink::planTruncatedUtf8("ABCDE", 0, glyphWidth, &widths).text.empty());
    assert(transitink::planTruncatedUtf8("ABCDE", -1, glyphWidth, &widths).text.empty());

    assert(
        transitink::withoutTrailingParentheticalQualifier(
            "36 · LEI MUK SHUE (CIRCULAR)") ==
        "36 · LEI MUK SHUE");
    assert(
        transitink::withoutTrailingParentheticalQualifier(
            "96 · Tsuen Wan (Hoi Pa Street)") ==
        "96 · Tsuen Wan");
    assert(
        transitink::withoutTrailingParentheticalQualifier(
            "751 · Tin Yat") ==
        "751 · Tin Yat");
    assert(
        transitink::withoutTrailingParentheticalQualifier(
            "Route (Circular) via Central") ==
        "Route (Circular) via Central");
    assert(
        transitink::withoutTrailingParentheticalQualifier("Route ()") ==
        "Route ()");
}
