#pragma once

#include "Arduino.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

class WiFiClient {
public:
    std::size_t available() {
        if (gJourneyTimeHttp.availableIndex <
            gJourneyTimeHttp.availableScript.size()) {
            return gJourneyTimeHttp
                .availableScript[gJourneyTimeHttp.availableIndex++];
        }
        return gJourneyTimeHttp.body.size() - gJourneyTimeHttp.cursor;
    }

    std::size_t readBytes(uint8_t* destination, std::size_t requested) {
        gJourneyTimeHttp.requestedReads.push_back(requested);
        const std::size_t remaining =
            gJourneyTimeHttp.body.size() - gJourneyTimeHttp.cursor;
        const std::size_t count = std::min(requested, remaining);
        if (count != 0) {
            std::memcpy(destination,
                        gJourneyTimeHttp.body.data() + gJourneyTimeHttp.cursor,
                        count);
            gJourneyTimeHttp.cursor += count;
        }
        return count;
    }
};

class WiFiClientSecure {
public:
    void setCACert(const char*) { ++gJourneyTimeHttp.tlsVerifiedCalls; }
    void stop() { ++gJourneyTimeHttp.tlsStopCalls; }
};
