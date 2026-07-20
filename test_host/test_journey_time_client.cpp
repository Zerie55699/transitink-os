#include "JourneyTimeClient.h"
#include "JourneyTimeHttpFake.h"

#include <cassert>
#include <cstdint>
#include <numeric>
#include <string>

FakeJourneyTimeHttpState gJourneyTimeHttp;

namespace {

transitink::JourneyTimeWidgetConfig validConfig() {
    transitink::JourneyTimeWidgetConfig config;
    config.locationId = "K07";
    config.destinationId = "ATSCA";
    return config;
}

std::string recordXml(const std::string& locationId,
                      const std::string& destinationId) {
    return "<?xml version=\"1.0\"?><jtis_journey_list "
           "xmlns=\"http://data.one.gov.hk/td\"><jtis_journey_time>"
           "<LOCATION_ID>" + locationId + "</LOCATION_ID>"
           "<DESTINATION_ID>" + destinationId + "</DESTINATION_ID>"
           "<CAPTURE_DATE>2026-07-10T22:14:00</CAPTURE_DATE>"
           "<JOURNEY_TYPE>1</JOURNEY_TYPE><JOURNEY_DATA>7</JOURNEY_DATA>"
           "<COLOUR_ID>2</COLOUR_ID><JOURNEY_DESC/>"
           "</jtis_journey_time></jtis_journey_list>";
}

void resetFake() { gJourneyTimeHttp = {}; }

JourneyTimeFetchOutcome fetch(
    const transitink::JourneyTimeWidgetConfig& config = validConfig()) {
    JourneyTimeClient client;
    transitink::JourneyTimeRecord record;
    String error;
    return client.fetchJourneyTime(config, record, error);
}

void assertNetworkCleanup() {
    assert(gJourneyTimeHttp.endCalls == 1);
    assert(gJourneyTimeHttp.tlsStopCalls == 1);
}

void testKnownLengthPrematureEofIsFailure() {
    resetFake();
    gJourneyTimeHttp.body = recordXml("H1", "CH");
    gJourneyTimeHttp.declaredSize =
        static_cast<int>(gJourneyTimeHttp.body.size() + 10);
    assert(fetch() == JourneyTimeFetchOutcome::Failure);
    assert(gJourneyTimeHttp.timeoutMs == JourneyTimeClient::kTimeoutMs);
    assert(!gJourneyTimeHttp.reuse);
    assertNetworkCleanup();
}

void testKnownLengthNeverOverreads() {
    resetFake();
    const std::string document = recordXml("H1", "CH");
    gJourneyTimeHttp.body = document + "EXTRA";
    gJourneyTimeHttp.declaredSize = static_cast<int>(document.size());
    assert(fetch() == JourneyTimeFetchOutcome::Empty);
    assert(gJourneyTimeHttp.cursor == document.size());
    assert(std::accumulate(gJourneyTimeHttp.requestedReads.begin(),
                           gJourneyTimeHttp.requestedReads.end(),
                           std::size_t{0}) == document.size());
    assertNetworkCleanup();
}

void testUnknownLengthSuccessAndNoMatch() {
    resetFake();
    gJourneyTimeHttp.body = recordXml("K07", "ATSCA");
    assert(fetch() == JourneyTimeFetchOutcome::Matched);
    assertNetworkCleanup();

    resetFake();
    gJourneyTimeHttp.body = recordXml("H1", "CH");
    assert(fetch() == JourneyTimeFetchOutcome::Empty);
    assertNetworkCleanup();
}

void testNoAvailabilityCanRecoverBeforeTimeout() {
    resetFake();
    gJourneyTimeHttp.body = recordXml("K07", "ATSCA");
    gJourneyTimeHttp.availableScript = {0, gJourneyTimeHttp.body.size()};
    gJourneyTimeHttp.delayAdvanceMs = 100;
    assert(fetch() == JourneyTimeFetchOutcome::Matched);
    assert(gJourneyTimeHttp.delayCalls == 1);
    assertNetworkCleanup();
}

void testIdleTimeoutAndMillisWraparound() {
    resetFake();
    gJourneyTimeHttp.stayConnectedAfterBody = true;
    gJourneyTimeHttp.delayAdvanceMs = 1000;
    assert(fetch() == JourneyTimeFetchOutcome::Failure);
    assert(gJourneyTimeHttp.nowMs >= JourneyTimeClient::kTimeoutMs);
    assertNetworkCleanup();

    resetFake();
    gJourneyTimeHttp.stayConnectedAfterBody = true;
    gJourneyTimeHttp.nowMs = UINT32_MAX - 5000;
    gJourneyTimeHttp.delayAdvanceMs = 1000;
    assert(fetch() == JourneyTimeFetchOutcome::Failure);
    assert(gJourneyTimeHttp.delayCalls == 10);
    assertNetworkCleanup();
}

void testOversizeAndParserFailuresCleanUp() {
    resetFake();
    gJourneyTimeHttp.declaredSize =
        static_cast<int>(JourneyTimeClient::kMaxResponseBytes + 1);
    assert(fetch() == JourneyTimeFetchOutcome::Failure);
    assert(gJourneyTimeHttp.requestedReads.empty());
    assertNetworkCleanup();

    resetFake();
    gJourneyTimeHttp.body.assign(JourneyTimeClient::kMaxResponseBytes + 1, ' ');
    assert(fetch() == JourneyTimeFetchOutcome::Failure);
    assert(gJourneyTimeHttp.cursor > JourneyTimeClient::kMaxResponseBytes);
    assertNetworkCleanup();

    resetFake();
    gJourneyTimeHttp.body = "<jtis_journey_list>";
    assert(fetch() == JourneyTimeFetchOutcome::Failure);
    assertNetworkCleanup();
}

void testEarlyTransportFailuresAndInvalidPair() {
    resetFake();
    gJourneyTimeHttp.beginSucceeds = false;
    assert(fetch() == JourneyTimeFetchOutcome::Failure);
    assertNetworkCleanup();

    resetFake();
    gJourneyTimeHttp.responseCode = 503;
    assert(fetch() == JourneyTimeFetchOutcome::Failure);
    assertNetworkCleanup();

    resetFake();
    gJourneyTimeHttp.nullStream = true;
    assert(fetch() == JourneyTimeFetchOutcome::Failure);
    assertNetworkCleanup();

    resetFake();
    auto invalid = validConfig();
    invalid.destinationId = "CH";
    assert(fetch(invalid) == JourneyTimeFetchOutcome::Failure);
    assert(gJourneyTimeHttp.beginCalls == 0);
    assert(gJourneyTimeHttp.endCalls == 0);
    assert(gJourneyTimeHttp.tlsStopCalls == 0);
}

}  // namespace

int main() {
    assert(std::string(JourneyTimeClient::requestUrl()) ==
           "https://resource.data.one.gov.hk/td/jss/Journeytimev2.xml");
    testKnownLengthPrematureEofIsFailure();
    testKnownLengthNeverOverreads();
    testUnknownLengthSuccessAndNoMatch();
    testNoAvailabilityCanRecoverBeforeTimeout();
    testIdleTimeoutAndMillisWraparound();
    testOversizeAndParserFailuresCleanUp();
    testEarlyTransportFailuresAndInvalidPair();
    return 0;
}
