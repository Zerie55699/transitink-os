#pragma once

#include <cstdint>
#include <string>

namespace transitink {

using CodepointWidth = int (*)(uint32_t codepoint, void* context);

struct DisplayTextPlan {
    std::string text;
    int pixelWidth = 0;
    bool truncated = false;
};

DisplayTextPlan planTruncatedUtf8(const std::string& text,
                                  int maxWidth,
                                  CodepointWidth codepointWidth,
                                  void* context);

std::string withoutTrailingParentheticalQualifier(const std::string& text);

}  // namespace transitink
