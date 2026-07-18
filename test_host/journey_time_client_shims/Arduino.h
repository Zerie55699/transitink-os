#pragma once

#include "JourneyTimeHttpFake.h"

#include <cstdint>
#include <string>

using String = std::string;

inline uint32_t millis() { return gJourneyTimeHttp.nowMs; }

inline void delay(uint32_t milliseconds) {
    ++gJourneyTimeHttp.delayCalls;
    gJourneyTimeHttp.nowMs += gJourneyTimeHttp.delayAdvanceMs == 0
                                  ? milliseconds
                                  : gJourneyTimeHttp.delayAdvanceMs;
}
