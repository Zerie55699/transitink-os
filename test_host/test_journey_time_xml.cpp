#include "core/JourneyTimeXmlParser.h"

#include <cassert>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>

namespace {

using transitink::JourneyTimeRecord;
using transitink::JourneyTimeValueKind;
using transitink::JourneyTimeXmlParser;
using transitink::XmlParseStatus;

std::string fixture() {
    std::ifstream input("test_host/fixtures/journey_time.xml", std::ios::binary);
    assert(input.good());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

XmlParseStatus parse(const std::string& xml,
                     const std::string& locationId,
                     const std::string& destinationId,
                     std::size_t chunkSize,
                     JourneyTimeRecord* record = nullptr) {
    JourneyTimeXmlParser parser(locationId, destinationId);
    XmlParseStatus status = XmlParseStatus::NeedMore;
    for (std::size_t offset = 0; offset < xml.size() && status == XmlParseStatus::NeedMore;
         offset += chunkSize) {
        const std::size_t size = std::min(chunkSize, xml.size() - offset);
        status = parser.feed(reinterpret_cast<const uint8_t*>(xml.data() + offset), size);
    }
    if (status == XmlParseStatus::NeedMore) status = parser.finish();
    if (record != nullptr && status == XmlParseStatus::Matched) *record = parser.record();
    return status;
}

std::string oneRecord(const std::string& location,
                      const std::string& destination,
                      const std::string& date,
                      const std::string& type,
                      const std::string& data,
                      const std::string& colour,
                      const std::string& description = "") {
    return "<?xml version=\"1.0\"?><jtis_journey_list "
           "xmlns=\"http://data.one.gov.hk/td\"><jtis_journey_time>"
           "<LOCATION_ID> " + location + " </LOCATION_ID>"
           "<DESTINATION_ID> " + destination + " </DESTINATION_ID>"
           "<CAPTURE_DATE> " + date + " </CAPTURE_DATE>"
           "<JOURNEY_TYPE> " + type + " </JOURNEY_TYPE>"
           "<JOURNEY_DATA> " + data + " </JOURNEY_DATA>"
           "<COLOUR_ID> " + colour + " </COLOUR_ID>"
           "<JOURNEY_DESC>" + description + "</JOURNEY_DESC>"
           "</jtis_journey_time></jtis_journey_list>";
}

}  // namespace

int main() {
    const std::string xml = fixture();
    for (const std::size_t chunkSize : {std::size_t{1}, std::size_t{7},
                                        std::size_t{512}}) {
        JourneyTimeRecord record;
        assert(parse(xml, "K07", "ATSCA", chunkSize, &record) ==
               XmlParseStatus::Matched);
        assert(record.locationId == "K07");
        assert(record.destinationId == "ATSCA");
        assert(record.minutes == 7);
        assert(record.colourId == 2);
        assert(record.dataEpoch == 1'709'222'399);
        assert(record.valueKind == JourneyTimeValueKind::Minutes);
        assert(record.statusCode == 0);
    }

    assert(parse(xml, "K07", "CH", 7) == XmlParseStatus::CompleteNoMatch);

    for (const int colour : {-1, 1, 2, 3}) {
        JourneyTimeRecord record;
        const auto ordinary = oneRecord("H1", "CH", "2024-02-29T23:59:59", "1",
                                        "6", std::to_string(colour));
        assert(parse(ordinary, "H1", "CH", 1, &record) == XmlParseStatus::Matched);
        assert(record.valueKind == JourneyTimeValueKind::Minutes);
        assert(record.minutes == 6);
        assert(record.colourId == colour);
    }

    for (const int code : {1, 3, 4, -1}) {
        JourneyTimeRecord record;
        const auto status = oneRecord("SJ2", "TCT", "2024-02-29T23:59:59", "2",
                                      std::to_string(code), "-1");
        assert(parse(status, "SJ2", "TCT", 7, &record) == XmlParseStatus::Matched);
        assert(record.minutes == 0);
        assert(record.statusCode == code);
        assert(record.valueKind == (code == 1 || code == 3
                                        ? JourneyTimeValueKind::Status
                                        : JourneyTimeValueKind::Unavailable));
    }

    JourneyTimeRecord unavailable;
    assert(parse(xml, "SJ2", "TCT", 512, &unavailable) == XmlParseStatus::Matched);
    assert(unavailable.valueKind == JourneyTimeValueKind::Unavailable);
    assert(unavailable.statusCode == -1);
    assert(unavailable.minutes == 0);

    for (const char* invalidDate : {
             "2023-02-29T12:00:00", "2024-13-01T12:00:00",
             "2024-02-29T24:00:00", "2024-02-29T12:00:00Z"}) {
        assert(parse(oneRecord("H1", "CH", invalidDate, "1", "6", "3"),
                     "H1", "CH", 7) == XmlParseStatus::Error);
    }

    for (const char* overflow : {"2147483648", "-2147483649"}) {
        assert(parse(oneRecord("H1", "CH", "2024-02-29T23:59:59", "2",
                               overflow, "-1"),
                     "H1", "CH", 7) == XmlParseStatus::Error);
    }

    assert(parse(oneRecord(std::string(80, 'X'), "CH", "2024-02-29T23:59:59",
                           "1", "6", "3"),
                 "H1", "CH", 7) == XmlParseStatus::Error);

    std::string missing = oneRecord("H1", "CH", "2024-02-29T23:59:59", "1",
                                    "6", "3");
    const auto colourStart = missing.find("<COLOUR_ID>");
    const auto colourEnd = missing.find("</COLOUR_ID>") + std::string("</COLOUR_ID>").size();
    missing.erase(colourStart, colourEnd - colourStart);
    assert(parse(missing, "H1", "CH", 7) == XmlParseStatus::Error);

    assert(parse(xml.substr(0, xml.size() - 10), "NO", "PAIR", 512) ==
           XmlParseStatus::Error);
    assert(parse(oneRecord("H1", "CH", "2024-02-29T23:59:59", "1", "6", "3") +
                     "<!--",
                 "NO", "PAIR", 512) == XmlParseStatus::Error);

    std::string oversized(JourneyTimeXmlParser::kMaxInputBytes + 1, ' ');
    assert(parse(oversized, "H1", "CH", 512) == XmlParseStatus::Error);
    return 0;
}
