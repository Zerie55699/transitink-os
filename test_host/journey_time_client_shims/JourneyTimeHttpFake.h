#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct FakeJourneyTimeHttpState {
    std::string body;
    int declaredSize = -1;
    bool beginSucceeds = true;
    int responseCode = 200;
    bool nullStream = false;
    bool stayConnectedAfterBody = false;
    std::vector<std::size_t> availableScript;
    std::vector<std::size_t> requestedReads;
    std::size_t availableIndex = 0;
    std::size_t cursor = 0;
    uint32_t nowMs = 0;
    uint32_t delayAdvanceMs = 1;
    int delayCalls = 0;
    int beginCalls = 0;
    int getCalls = 0;
    int endCalls = 0;
    int tlsStopCalls = 0;
    int tlsVerifiedCalls = 0;
    int timeoutMs = 0;
    bool reuse = true;
    std::string requestedUrl;
};

extern FakeJourneyTimeHttpState gJourneyTimeHttp;
