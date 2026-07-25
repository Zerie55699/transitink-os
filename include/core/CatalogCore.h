#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace transitink {

constexpr std::size_t kMaxCatalogObjectBytes = 2048;
constexpr std::size_t kMaxCatalogDownloadBytes = 2 * 1024 * 1024;
constexpr std::size_t kMaxCatalogRoutes = 1024;
constexpr std::size_t kMaxCatalogDirections = 32;
constexpr std::size_t kMaxCatalogResponseBytes = 64 * 1024;
constexpr std::size_t kMaxRouteStopResponseBytes = 64 * 1024;
constexpr std::size_t kMaxCitybusRouteStops = 96;

class CatalogByteReader {
public:
    virtual ~CatalogByteReader() = default;
    virtual int readByte() = 0;
};

class BoundedBodyAccumulator {
public:
    explicit BoundedBodyAccumulator(std::size_t limit) : limit_(limit) {}
    bool append(const uint8_t* bytes, std::size_t length, std::string& error);
    bool complete(int expectedLength, std::string& error) const;
    const std::string& body() const { return body_; }

private:
    std::size_t limit_;
    std::string body_;
};

enum class BusCatalogFormat : uint8_t { Kmb, Citybus };

enum class CatalogRefreshAction : uint8_t { UseCache, Fetch };

struct AtomicCacheRecoveryPlan {
    bool removeTemp = false;
    bool restoreBackup = false;
};

struct BusCatalogRoute {
    std::string routeId;
    std::string directionId;
    std::string serviceType;
    std::string originLabelTc;
    std::string destinationLabelTc;
    std::string originLabelEn{};
    std::string destinationLabelEn{};
};

struct BusStopLabel {
    std::string stopId;
    std::string labelTc;
    std::string labelEn{};
};

struct BusCatalogStop {
    std::string stopId;
    std::string labelTc;
    uint16_t sequence = 0;
    std::string labelEn{};
};

enum class CitybusStopResolution : uint8_t {
    FreshCache,
    Hydrate,
    Hydrated,
    StaleCache,
    Unavailable,
};

struct CitybusStopDecision {
    CitybusStopResolution resolution = CitybusStopResolution::Unavailable;
    bool shouldFetch = false;
    bool shouldCommit = false;
    std::vector<BusStopLabel> labels;
    std::string error;
};

CatalogRefreshAction decideCatalogRefresh(bool refreshRequested,
                                          bool cacheUsable,
                                          bool cacheFresh,
                                          bool clockValid);
AtomicCacheRecoveryPlan planAtomicCacheRecovery(bool dataExists,
                                                bool backupExists,
                                                bool tempExists);

using CatalogFetchCallback = bool (*)(void* context);

bool parseBusRoutes(CatalogByteReader& reader,
                    BusCatalogFormat format,
                    std::vector<BusCatalogRoute>& rows,
                    std::string& error);
bool parseBusRouteIds(CatalogByteReader& reader,
                      BusCatalogFormat format,
                      std::vector<std::string>& routeIds,
                      std::string& error);
bool parseBusDirectionsForRoute(CatalogByteReader& reader,
                                BusCatalogFormat format,
                                const std::string& routeId,
                                std::vector<BusCatalogRoute>& rows,
                                std::string& error);
bool validateBusRouteCatalog(CatalogByteReader& reader,
                             BusCatalogFormat format,
                             std::string& error);
bool parseBusStopLabels(CatalogByteReader& reader,
                        std::vector<BusStopLabel>& labels,
                        std::string& error);
bool parseMatchingBusStopLabels(CatalogByteReader& reader,
                                const std::vector<std::string>& requestedStopIds,
                                std::vector<BusStopLabel>& labels,
                                std::string& error);
bool parseBusRouteStops(CatalogByteReader& reader,
                        const std::string& route,
                        const std::string& direction,
                        const std::string& serviceType,
                        std::vector<BusCatalogStop>& stops,
                        std::string& error);
bool joinBusStopLabels(std::vector<BusCatalogStop>& stops,
                       const std::vector<BusStopLabel>& labels,
                       std::string& error);
bool runValidatedBusCatalogQuery(const std::string& operatorId,
                                 const std::string& route,
                                 const std::string& direction,
                                 const std::string& serviceType,
                                 CatalogFetchCallback fetch,
                                 void* context,
                                 std::string& error);
CitybusStopDecision resolveCitybusStopLabels(
    const std::vector<std::string>& requiredStopIds,
    const std::vector<BusStopLabel>& freshCache,
    const std::vector<BusStopLabel>& staleCache,
    const std::vector<BusStopLabel>& hydrated,
    bool hydrationAttempted,
    bool hydrationComplete);

}  // namespace transitink
