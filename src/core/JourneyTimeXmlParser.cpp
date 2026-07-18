#include "core/JourneyTimeXmlParser.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace transitink {
namespace {

constexpr std::array<const char*, 7> kFieldNames = {
    "LOCATION_ID", "DESTINATION_ID", "CAPTURE_DATE", "JOURNEY_TYPE",
    "JOURNEY_DATA", "COLOUR_ID", "JOURNEY_DESC",
};
constexpr std::array<std::size_t, 7> kFieldLimits = {64, 64, 32, 16, 16, 16, 192};

std::string localName(const char* qualifiedName) {
    const std::string name = qualifiedName == nullptr ? "" : qualifiedName;
    const auto separator = name.rfind(':');
    return separator == std::string::npos ? name : name.substr(separator + 1);
}

std::string trimAsciiWhitespace(const std::string& value) {
    const auto isWhitespace = [](char ch) {
        return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
    };
    auto begin = value.begin();
    while (begin != value.end() && isWhitespace(*begin)) ++begin;
    auto end = value.end();
    while (end != begin && isWhitespace(*(end - 1))) --end;
    return {begin, end};
}

bool parseSignedInt32(const std::string& value, int32_t& parsed) {
    const std::string text = trimAsciiWhitespace(value);
    if (text.empty()) return false;
    std::size_t index = 0;
    bool negative = false;
    if (text[0] == '-') {
        negative = true;
        index = 1;
    }
    if (index == text.size()) return false;
    int64_t magnitude = 0;
    const int64_t limit = negative
                              ? -static_cast<int64_t>(std::numeric_limits<int32_t>::min())
                              : std::numeric_limits<int32_t>::max();
    for (; index < text.size(); ++index) {
        const char digit = text[index];
        if (digit < '0' || digit > '9') return false;
        magnitude = magnitude * 10 + (digit - '0');
        if (magnitude > limit) return false;
    }
    parsed = static_cast<int32_t>(negative ? -magnitude : magnitude);
    return true;
}

bool parseDigits(const std::string& value,
                 std::size_t offset,
                 std::size_t count,
                 int& parsed) {
    if (offset + count > value.size()) return false;
    parsed = 0;
    for (std::size_t index = offset; index < offset + count; ++index) {
        const char digit = value[index];
        if (digit < '0' || digit > '9') return false;
        parsed = parsed * 10 + (digit - '0');
    }
    return true;
}

bool leapYear(int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

int daysInMonth(int year, int month) {
    static constexpr int kDays[] = {31, 28, 31, 30, 31, 30,
                                    31, 31, 30, 31, 30, 31};
    return month == 2 && leapYear(year) ? 29 : kDays[month - 1];
}

int64_t daysFromCivil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
    const unsigned dayOfYear =
        (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned dayOfEra =
        yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(dayOfEra) -
           719468;
}

bool parseHongKongDateTime(const std::string& value, int64_t& epoch) {
    const std::string text = trimAsciiWhitespace(value);
    if (text.size() != 19 || text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
        text[13] != ':' || text[16] != ':') {
        return false;
    }
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (!parseDigits(text, 0, 4, year) || !parseDigits(text, 5, 2, month) ||
        !parseDigits(text, 8, 2, day) || !parseDigits(text, 11, 2, hour) ||
        !parseDigits(text, 14, 2, minute) || !parseDigits(text, 17, 2, second) ||
        month < 1 || month > 12 || day < 1 || day > daysInMonth(year, month) ||
        hour > 23 || minute > 59 || second > 59) {
        return false;
    }
    constexpr int64_t kHongKongOffsetSeconds = 8 * 60 * 60;
    epoch = daysFromCivil(year, static_cast<unsigned>(month),
                          static_cast<unsigned>(day)) *
                86400 +
            static_cast<int64_t>(hour) * 3600 + minute * 60 + second -
            kHongKongOffsetSeconds;
    return true;
}

bool validColour(int32_t colour) {
    return colour == -1 || colour == 1 || colour == 2 || colour == 3;
}

}  // namespace

JourneyTimeXmlParser::JourneyTimeXmlParser(std::string locationId,
                                           std::string destinationId)
    : locationId_(std::move(locationId)), destinationId_(std::move(destinationId)) {
    yxml_init(&xml_, xmlStack_.data(), xmlStack_.size());
    record_.valid = false;
}

XmlParseStatus JourneyTimeXmlParser::feed(const uint8_t* data, std::size_t size) {
    if (status_ != XmlParseStatus::NeedMore) return status_;
    if (data == nullptr && size != 0) {
        fail();
        return status_;
    }
    for (std::size_t index = 0; index < size; ++index) {
        if (++totalBytes_ > kMaxInputBytes) {
            fail();
            break;
        }
        const auto token = yxml_parse(&xml_, data[index]);
        if (token < YXML_OK) {
            fail();
            break;
        }
        status_ = handleToken(token);
        if (status_ != XmlParseStatus::NeedMore) break;
    }
    return status_;
}

XmlParseStatus JourneyTimeXmlParser::finish() {
    if (status_ != XmlParseStatus::NeedMore) return status_;
    if (!rootSeen_ || !rootClosed_ || depth_ != 0 || inRow_ ||
        yxml_eof(&xml_) != YXML_OK) {
        fail();
    } else {
        status_ = XmlParseStatus::CompleteNoMatch;
    }
    return status_;
}

const JourneyTimeRecord& JourneyTimeXmlParser::record() const { return record_; }

XmlParseStatus JourneyTimeXmlParser::handleToken(yxml_ret_t token) {
    if (token == YXML_ELEMSTART ||
        token == static_cast<yxml_ret_t>(YXML_ELEMSTART | YXML_CONTENT)) {
        const std::string name = localName(xml_.elem);
        if (depth_ == 0) {
            if (rootSeen_ || name != "jtis_journey_list") {
                fail();
                return status_;
            }
            rootSeen_ = true;
        } else if (depth_ == 1) {
            if (!rootSeen_ || rootClosed_ || name != "jtis_journey_time") {
                fail();
                return status_;
            }
            inRow_ = true;
            fieldIndex_ = 0;
            activeField_ = -1;
            for (auto& field : fields_) field.clear();
        } else if (depth_ == 2 && inRow_) {
            if (fieldIndex_ >= kFieldCount || name != kFieldNames[fieldIndex_]) {
                fail();
                return status_;
            }
            activeField_ = static_cast<int>(fieldIndex_);
        } else {
            fail();
            return status_;
        }
        ++depth_;
    } else if (token == YXML_DATA) {
        if (activeField_ >= 0) {
            auto& field = fields_[static_cast<std::size_t>(activeField_)];
            if (field.size() >= kFieldLimits[static_cast<std::size_t>(activeField_)]) {
                fail();
                return status_;
            }
            field.push_back(xml_.data);
        }
    } else if (token == YXML_ELEMEND) {
        if (depth_ == 0) {
            fail();
            return status_;
        }
        if (depth_ == 3 && inRow_) {
            if (activeField_ < 0) {
                fail();
                return status_;
            }
            activeField_ = -1;
            ++fieldIndex_;
        } else if (depth_ == 2 && inRow_) {
            if (fieldIndex_ != kFieldCount || !finishRow()) {
                fail();
                return status_;
            }
            inRow_ = false;
            if (record_.valid) {
                status_ = XmlParseStatus::Matched;
                return status_;
            }
        } else if (depth_ == 1) {
            rootClosed_ = true;
        } else {
            fail();
            return status_;
        }
        --depth_;
    }
    return status_;
}

bool JourneyTimeXmlParser::finishRow() {
    const std::string location = trimAsciiWhitespace(fields_[0]);
    const std::string destination = trimAsciiWhitespace(fields_[1]);
    if (location.empty() || destination.empty()) return false;

    int64_t captureEpoch = 0;
    int32_t journeyType = 0;
    int32_t journeyData = 0;
    int32_t colourId = 0;
    if (!parseHongKongDateTime(fields_[2], captureEpoch) || captureEpoch <= 0 ||
        !parseSignedInt32(fields_[3], journeyType) ||
        !parseSignedInt32(fields_[4], journeyData) ||
        !parseSignedInt32(fields_[5], colourId) || !validColour(colourId)) {
        return false;
    }

    if (journeyType == 1) {
        if (journeyData < 0 || journeyData > std::numeric_limits<uint16_t>::max()) {
            return false;
        }
    } else if (journeyType == 2) {
        if (journeyData != -1 && journeyData != 1 && journeyData != 3 &&
            journeyData != 4) {
            return false;
        }
    } else {
        return false;
    }

    record_ = {};
    record_.valid = false;
    if (location != locationId_ || destination != destinationId_) return true;
    record_.locationId = location;
    record_.destinationId = destination;
    record_.colourId = static_cast<int8_t>(colourId);
    record_.dataEpoch = captureEpoch;
    record_.valid = true;
    if (journeyType == 1) {
        record_.minutes = static_cast<uint16_t>(journeyData);
        record_.valueKind = JourneyTimeValueKind::Minutes;
    } else {
        record_.minutes = 0;
        record_.statusCode = static_cast<int16_t>(journeyData);
        record_.valueKind = journeyData == 1 || journeyData == 3
                                ? JourneyTimeValueKind::Status
                                : JourneyTimeValueKind::Unavailable;
    }
    return true;
}

void JourneyTimeXmlParser::fail() { status_ = XmlParseStatus::Error; }

}  // namespace transitink
