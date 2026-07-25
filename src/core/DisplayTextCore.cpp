#include "core/DisplayTextCore.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace transitink {
namespace {

struct DecodedCodepoint {
    uint32_t value;
    std::size_t byteCount;
};

bool isContinuation(uint8_t value) {
    return (value & 0xC0U) == 0x80U;
}

DecodedCodepoint decodeCodepoint(const std::string& text, std::size_t offset) {
    const auto first = static_cast<uint8_t>(text[offset]);
    if (first < 0x80U) {
        return {first, 1};
    }
    if (first >= 0xC2U && first <= 0xDFU && offset + 1 < text.size()) {
        const auto second = static_cast<uint8_t>(text[offset + 1]);
        if (isContinuation(second)) {
            return {static_cast<uint32_t>(((first & 0x1FU) << 6U) | (second & 0x3FU)), 2};
        }
    }
    if (first >= 0xE0U && first <= 0xEFU && offset + 2 < text.size()) {
        const auto second = static_cast<uint8_t>(text[offset + 1]);
        const auto third = static_cast<uint8_t>(text[offset + 2]);
        const bool validSecond = isContinuation(second) &&
                                 !(first == 0xE0U && second < 0xA0U) &&
                                 !(first == 0xEDU && second >= 0xA0U);
        if (validSecond && isContinuation(third)) {
            return {static_cast<uint32_t>(((first & 0x0FU) << 12U) |
                                          ((second & 0x3FU) << 6U) |
                                          (third & 0x3FU)),
                    3};
        }
    }
    if (first >= 0xF0U && first <= 0xF4U && offset + 3 < text.size()) {
        const auto second = static_cast<uint8_t>(text[offset + 1]);
        const auto third = static_cast<uint8_t>(text[offset + 2]);
        const auto fourth = static_cast<uint8_t>(text[offset + 3]);
        const bool validSecond = isContinuation(second) &&
                                 !(first == 0xF0U && second < 0x90U) &&
                                 !(first == 0xF4U && second >= 0x90U);
        if (validSecond && isContinuation(third) && isContinuation(fourth)) {
            return {static_cast<uint32_t>(((first & 0x07U) << 18U) |
                                          ((second & 0x3FU) << 12U) |
                                          ((third & 0x3FU) << 6U) |
                                          (fourth & 0x3FU)),
                    4};
        }
    }
    return {'?', 1};
}

void appendCodepoint(std::string& output, uint32_t codepoint) {
    if (codepoint < 0x80U) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint < 0x800U) {
        output.push_back(static_cast<char>(0xC0U | (codepoint >> 6U)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    } else if (codepoint < 0x10000U) {
        output.push_back(static_cast<char>(0xE0U | (codepoint >> 12U)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    } else {
        output.push_back(static_cast<char>(0xF0U | (codepoint >> 18U)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }
}

int measuredWidth(const std::string& text, CodepointWidth codepointWidth, void* context) {
    int total = 0;
    for (std::size_t offset = 0; offset < text.size();) {
        const DecodedCodepoint decoded = decodeCodepoint(text, offset);
        offset += decoded.byteCount;
        const int width = codepointWidth(decoded.value, context);
        if (width <= 0) {
            return -1;
        }
        if (total > std::numeric_limits<int>::max() - width) {
            return -1;
        }
        total += width;
    }
    return total;
}

}  // namespace

DisplayTextPlan planTruncatedUtf8(const std::string& text,
                                  int maxWidth,
                                  CodepointWidth codepointWidth,
                                  void* context) {
    DisplayTextPlan plan;
    if (maxWidth <= 0 || codepointWidth == nullptr) {
        return plan;
    }

    std::string output;
    output.reserve(text.size() + 3);
    int fullWidth = 0;
    for (std::size_t offset = 0; offset < text.size();) {
        const DecodedCodepoint decoded = decodeCodepoint(text, offset);
        offset += decoded.byteCount;
        const int width = codepointWidth(decoded.value, context);
        if (width <= 0) {
            continue;
        }
        if (fullWidth > maxWidth - width) {
            fullWidth = maxWidth + 1;
            break;
        }
        appendCodepoint(output, decoded.value);
        fullWidth += width;
    }
    if (fullWidth <= maxWidth) {
        plan.text = std::move(output);
        plan.pixelWidth = fullWidth;
        return plan;
    }

    static const char* const suffixes[] = {"…", "...", "..", ".", ""};
    const char* suffix = "";
    int suffixWidth = 0;
    for (const char* candidate : suffixes) {
        const int candidateWidth = measuredWidth(candidate, codepointWidth, context);
        if (candidateWidth >= 0 && candidateWidth <= maxWidth) {
            suffix = candidate;
            suffixWidth = candidateWidth;
            break;
        }
    }

    output.clear();
    int visibleWidth = 0;
    for (std::size_t offset = 0; offset < text.size();) {
        const DecodedCodepoint decoded = decodeCodepoint(text, offset);
        offset += decoded.byteCount;
        const int width = codepointWidth(decoded.value, context);
        if (width <= 0) {
            continue;
        }
        if (visibleWidth > maxWidth - suffixWidth - width) {
            break;
        }
        appendCodepoint(output, decoded.value);
        visibleWidth += width;
    }
    output.append(suffix);
    plan.text = std::move(output);
    plan.pixelWidth = visibleWidth + suffixWidth;
    plan.truncated = true;
    return plan;
}

std::string withoutTrailingParentheticalQualifier(const std::string& text) {
    if (text.size() < 4 || text.back() != ')') {
        return text;
    }
    const std::size_t qualifierStart = text.rfind(" (");
    if (qualifierStart == std::string::npos ||
        qualifierStart + 3 >= text.size()) {
        return text;
    }
    return text.substr(0, qualifierStart);
}

}  // namespace transitink
