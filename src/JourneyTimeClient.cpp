#include "JourneyTimeClient.h"

#include "TransitCatalog.h"
#include "TransitTlsTrust.h"
#include "core/JourneyTimeXmlParser.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <algorithm>
#include <array>

namespace {

struct HttpCleanup {
    HTTPClient& http;
    WiFiClientSecure& tls;

    ~HttpCleanup() {
        http.end();
        tls.stop();
    }
};

JourneyTimeFetchOutcome failure(String& error, const char* message) {
    error = message;
    return JourneyTimeFetchOutcome::Failure;
}

}  // namespace

JourneyTimeFetchOutcome JourneyTimeClient::fetchJourneyTime(
    const transitink::JourneyTimeWidgetConfig& config,
    transitink::JourneyTimeRecord& record,
    String& error) {
    record = {};
    record.valid = false;
    error = "";
    if (!transitink::isJourneyTimePairValid(config.locationId,
                                             config.destinationId)) {
        return failure(error, "行車時間地點或目的地設定不正確");
    }

    WiFiClientSecure tls;
    transitink::configureVerifiedTls(tls);
    HTTPClient http;
    HttpCleanup cleanup{http, tls};
    http.setTimeout(JourneyTimeClient::kTimeoutMs);
    http.setReuse(false);
    if (!http.begin(tls, String(requestUrl()))) {
        return failure(error, "未能連接行車時間資料服務");
    }
    const int statusCode = http.GET();
    if (statusCode != HTTP_CODE_OK) {
        return failure(error, "行車時間資料服務回應錯誤");
    }

    int remaining = http.getSize();
    if (remaining > static_cast<int>(kMaxResponseBytes)) {
        return failure(error, "行車時間資料過大");
    }
    auto* stream = http.getStreamPtr();
    if (stream == nullptr) {
        return failure(error, "未能讀取行車時間資料");
    }

    transitink::JourneyTimeXmlParser parser(config.locationId,
                                             config.destinationId);
    std::array<uint8_t, JourneyTimeClient::kReadBufferBytes> buffer{};
    std::size_t totalBytes = 0;
    uint32_t lastProgressMs = millis();
    while (http.connected() && (remaining > 0 || remaining == -1)) {
        const std::size_t available = stream->available();
        if (available == 0) {
            if (millis() - lastProgressMs >= JourneyTimeClient::kTimeoutMs) {
                return failure(error, "行車時間資料傳輸逾時");
            }
            delay(1);
            continue;
        }
        std::size_t requested = std::min(available, buffer.size());
        if (remaining >= 0) {
            requested =
                std::min(requested, static_cast<std::size_t>(remaining));
        }
        const int bytesRead = stream->readBytes(buffer.data(), requested);
        if (bytesRead <= 0) {
            return failure(error, "行車時間資料傳輸中斷");
        }
        lastProgressMs = millis();
        totalBytes += static_cast<std::size_t>(bytesRead);
        if (totalBytes > kMaxResponseBytes) {
            return failure(error, "行車時間資料過大");
        }
        const auto parserStatus =
            parser.feed(buffer.data(), static_cast<std::size_t>(bytesRead));
        if (parserStatus == transitink::XmlParseStatus::Matched) {
            record = parser.record();
            return JourneyTimeFetchOutcome::Matched;
        }
        if (parserStatus == transitink::XmlParseStatus::Error) {
            return failure(error, "行車時間資料格式不正確");
        }
        if (remaining > 0) remaining -= bytesRead;
    }

    if (remaining > 0) {
        return failure(error, "行車時間資料傳輸中斷");
    }

    const auto parserStatus = parser.finish();
    if (parserStatus == transitink::XmlParseStatus::Matched) {
        record = parser.record();
        return JourneyTimeFetchOutcome::Matched;
    }
    if (parserStatus == transitink::XmlParseStatus::CompleteNoMatch) {
        return JourneyTimeFetchOutcome::Empty;
    }
    return failure(error, "行車時間資料格式不正確");
}
