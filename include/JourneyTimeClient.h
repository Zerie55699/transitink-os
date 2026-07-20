#pragma once

#include <Arduino.h>

#include <cstddef>
#include <cstdint>

#include "core/WidgetCore.h"

enum class JourneyTimeFetchOutcome : uint8_t { Matched, Empty, Failure };

class JourneyTimeClient {
public:
    static constexpr int kTimeoutMs = 10000;
    static constexpr std::size_t kReadBufferBytes = 512;
    static constexpr std::size_t kMaxResponseBytes = 65536;

    static constexpr const char* requestUrl() {
        return "https://resource.data.one.gov.hk/td/jss/Journeytimev2.xml";
    }

    JourneyTimeFetchOutcome fetchJourneyTime(
        const transitink::JourneyTimeWidgetConfig& config,
        transitink::JourneyTimeRecord& record,
        String& error);
};
