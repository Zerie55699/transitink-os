#pragma once

#include "WiFiClientSecure.h"

constexpr int HTTP_CODE_OK = 200;

class HTTPClient {
public:
    void setTimeout(int timeoutMs) { gJourneyTimeHttp.timeoutMs = timeoutMs; }
    void setReuse(bool reuse) { gJourneyTimeHttp.reuse = reuse; }

    bool begin(WiFiClientSecure&, const String& url) {
        ++gJourneyTimeHttp.beginCalls;
        gJourneyTimeHttp.requestedUrl = url;
        return gJourneyTimeHttp.beginSucceeds;
    }

    int GET() {
        ++gJourneyTimeHttp.getCalls;
        return gJourneyTimeHttp.responseCode;
    }

    int getSize() const { return gJourneyTimeHttp.declaredSize; }

    WiFiClient* getStreamPtr() {
        return gJourneyTimeHttp.nullStream ? nullptr : &stream_;
    }

    bool connected() const {
        return gJourneyTimeHttp.cursor < gJourneyTimeHttp.body.size() ||
               gJourneyTimeHttp.stayConnectedAfterBody;
    }

    void end() { ++gJourneyTimeHttp.endCalls; }

private:
    WiFiClient stream_;
};
