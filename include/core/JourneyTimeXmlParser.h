#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

extern "C" {
#include "yxml.h"
}

#include "core/WidgetCore.h"

namespace transitink {

enum class XmlParseStatus : uint8_t { NeedMore, Matched, CompleteNoMatch, Error };

class JourneyTimeXmlParser {
public:
    static constexpr std::size_t kMaxInputBytes = 65536;

    JourneyTimeXmlParser(std::string locationId, std::string destinationId);
    XmlParseStatus feed(const uint8_t* data, std::size_t size);
    XmlParseStatus finish();
    const JourneyTimeRecord& record() const;

private:
    static constexpr std::size_t kFieldCount = 7;

    XmlParseStatus handleToken(yxml_ret_t token);
    bool finishRow();
    void fail();

    std::string locationId_;
    std::string destinationId_;
    JourneyTimeRecord record_;
    XmlParseStatus status_ = XmlParseStatus::NeedMore;
    std::array<char, 256> xmlStack_{};
    yxml_t xml_{};
    std::array<std::string, kFieldCount> fields_{};
    std::size_t totalBytes_ = 0;
    std::size_t depth_ = 0;
    std::size_t fieldIndex_ = 0;
    int activeField_ = -1;
    bool rootSeen_ = false;
    bool rootClosed_ = false;
    bool inRow_ = false;
};

}  // namespace transitink
