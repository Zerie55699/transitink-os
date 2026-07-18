#pragma once

#include <Arduino.h>

#include <vector>

#include "TransitJsonParsers.h"

class GmbClient {
public:
    bool fetchRouteCodes(const String& region,
                         std::vector<std::string>& routeCodes,
                         String& error);
    bool fetchDirections(
        const String& region,
        const String& routeCode,
        std::vector<transitink::GmbCatalogDirection>& directions,
        String& error);
    bool fetchStops(const String& routeId,
                    const String& routeSeq,
                    std::vector<transitink::GmbCatalogStop>& stops,
                    String& error);
    bool fetchEta(const transitink::GmbWidgetConfig& config,
                  transitink::GmbEtaPayload& payload,
                  String& error);

private:
    bool httpGetBounded(const String& url,
                        std::size_t limit,
                        String& body,
                        String& error);
};
