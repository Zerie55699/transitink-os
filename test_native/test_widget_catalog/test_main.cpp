#include "core/CatalogCore.h"
#include "core/StaticCatalogCore.h"
#include "TransitJsonParsers.h"

#include <unity.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace {

class StringReader final : public transitink::CatalogByteReader {
public:
    explicit StringReader(std::string value) : value_(std::move(value)) {}
    int readByte() override {
        return offset_ < value_.size() ? static_cast<unsigned char>(value_[offset_++]) : -1;
    }

private:
    std::string value_;
    std::size_t offset_ = 0;
};

void test_kmb_routes_map_exact_direction_service_and_deduplicate() {
    const std::string json = R"({"type":"RouteList","data":[
      {"co":"KMB","route":"268B","bound":"O","service_type":1,"orig_tc":"朗屏站","dest_tc":"紅磡碼頭"},
      {"co":"KMB","route":"268B","bound":"O","service_type":1,"orig_tc":"朗屏站","dest_tc":"紅磡碼頭"},
      {"co":"KMB","route":"268B","bound":"I","service_type":"2","orig_tc":"紅磡碼頭","dest_tc":"朗屏站"}
    ]})";
    StringReader reader(json);
    std::vector<transitink::BusCatalogRoute> rows;
    std::string error;
    TEST_ASSERT_TRUE(transitink::parseBusRoutes(reader, transitink::BusCatalogFormat::Kmb, rows, error));
    TEST_ASSERT_EQUAL_UINT32(2, rows.size());
    TEST_ASSERT_EQUAL_STRING("I", rows[0].directionId.c_str());
    TEST_ASSERT_EQUAL_STRING("2", rows[0].serviceType.c_str());
    TEST_ASSERT_EQUAL_STRING("O", rows[1].directionId.c_str());
    TEST_ASSERT_EQUAL_STRING("紅磡碼頭", rows[1].destinationLabelTc.c_str());
}

void test_citybus_routes_create_official_inbound_outbound_selection_rows() {
    const std::string json = R"({"type":"RouteList","data":[
      {"co":"CTB","route":"1","orig_tc":"中環","dest_tc":"跑馬地"}
    ]})";
    StringReader reader(json);
    std::vector<transitink::BusCatalogRoute> rows;
    std::string error;
    TEST_ASSERT_TRUE(transitink::parseBusRoutes(reader, transitink::BusCatalogFormat::Citybus, rows, error));
    TEST_ASSERT_EQUAL_UINT32(2, rows.size());
    TEST_ASSERT_EQUAL_STRING("I", rows[0].directionId.c_str());
    TEST_ASSERT_EQUAL_STRING("1", rows[0].serviceType.c_str());
    TEST_ASSERT_EQUAL_STRING("跑馬地", rows[0].originLabelTc.c_str());
    TEST_ASSERT_EQUAL_STRING("中環", rows[0].destinationLabelTc.c_str());
    TEST_ASSERT_EQUAL_STRING("O", rows[1].directionId.c_str());
    TEST_ASSERT_EQUAL_STRING("中環", rows[1].originLabelTc.c_str());
    TEST_ASSERT_EQUAL_STRING("跑馬地", rows[1].destinationLabelTc.c_str());
}

void test_citybus_h1_outbound_label_path_and_first_stop_match_live_contract() {
    const std::string routes = R"JSON({"data":[{"co":"CTB","route":"H1","orig_tc":"中環 (天星碼頭)","dest_tc":"尖沙咀"}]})JSON";
    StringReader routeReader(routes);
    std::vector<transitink::BusCatalogRoute> rows;
    std::string error;
    TEST_ASSERT_TRUE(transitink::parseBusRoutes(
        routeReader, transitink::BusCatalogFormat::Citybus, rows, error));
    const auto outbound = std::find_if(rows.begin(), rows.end(), [](const auto& row) {
        return row.directionId == "O";
    });
    TEST_ASSERT_TRUE(outbound != rows.end());
    TEST_ASSERT_EQUAL_STRING("中環 (天星碼頭)", outbound->originLabelTc.c_str());
    TEST_ASSERT_EQUAL_STRING("尖沙咀", outbound->destinationLabelTc.c_str());

    std::string path;
    TEST_ASSERT_TRUE(mapCitybusDirectionPath("O", path));
    TEST_ASSERT_EQUAL_STRING("outbound", path.c_str());
    const std::string routeStops = R"({"data":[{"co":"CTB","route":"H1","dir":"O","seq":1,"stop":"001171"}]})";
    StringReader stopReader(routeStops);
    std::vector<transitink::BusCatalogStop> stops;
    TEST_ASSERT_TRUE(transitink::parseBusRouteStops(
        stopReader, "H1", "O", "1", stops, error));
    TEST_ASSERT_EQUAL_UINT32(1, stops.size());
    TEST_ASSERT_EQUAL_STRING("001171", stops[0].stopId.c_str());
}

void test_route_stops_join_complete_stop_catalog_and_sort_sequence() {
    const std::string stopsJson = R"({"data":[
      {"stop":"002402","name_tc":"林士街"},
      {"stop":"002403","name_tc":"港澳碼頭"},
      {"stop":"002403","name_tc":"港澳碼頭"}
    ]})";
    const std::string routeStopsJson = R"({"data":[
      {"route":"1","dir":"I","seq":2,"stop":"002402"},
      {"route":"1","dir":"I","seq":1,"stop":"002403"}
    ]})";
    StringReader labelsReader(stopsJson);
    StringReader routeReader(routeStopsJson);
    std::vector<transitink::BusStopLabel> labels;
    std::vector<transitink::BusCatalogStop> stops;
    std::string error;
    TEST_ASSERT_TRUE(transitink::parseBusStopLabels(labelsReader, labels, error));
    TEST_ASSERT_EQUAL_UINT32(2, labels.size());
    TEST_ASSERT_TRUE(transitink::parseBusRouteStops(routeReader, "1", "I", "1", stops, error));
    TEST_ASSERT_TRUE(transitink::joinBusStopLabels(stops, labels, error));
    TEST_ASSERT_EQUAL_UINT32(2, stops.size());
    TEST_ASSERT_EQUAL_UINT16(1, stops[0].sequence);
    TEST_ASSERT_EQUAL_STRING("港澳碼頭", stops[0].labelTc.c_str());
    TEST_ASSERT_EQUAL_STRING("林士街", stops[1].labelTc.c_str());
}

void test_kmb_route_stops_accept_live_string_sequences() {
    const std::string routeStopsJson = R"({"type":"RouteStop","data":[
      {"co":"KMB","route":"40P","bound":"O","service_type":"1","seq":"2","stop":"A1B2C3"},
      {"co":"KMB","route":"40P","bound":"O","service_type":"1","seq":"1","stop":"D4E5F6"}
    ]})";
    StringReader reader(routeStopsJson);
    std::vector<transitink::BusCatalogStop> stops;
    std::string error;
    TEST_ASSERT_TRUE(transitink::parseBusRouteStops(
        reader, "40P", "O", "1", stops, error));
    TEST_ASSERT_EQUAL_UINT32(2, stops.size());
    TEST_ASSERT_EQUAL_UINT16(1, stops[0].sequence);
    TEST_ASSERT_EQUAL_STRING("D4E5F6", stops[0].stopId.c_str());
    TEST_ASSERT_EQUAL_UINT16(2, stops[1].sequence);
}

void test_route_stops_reject_invalid_string_sequences() {
    const std::string routeStopsJson = R"({"data":[
      {"route":"40P","bound":"O","seq":"1x","stop":"A1B2C3"}
    ]})";
    StringReader reader(routeStopsJson);
    std::vector<transitink::BusCatalogStop> stops;
    std::string error;
    TEST_ASSERT_FALSE(transitink::parseBusRouteStops(
        reader, "40P", "O", "1", stops, error));
    TEST_ASSERT_EQUAL_STRING("路線站牌項目格式不正確", error.c_str());
}

void test_matching_stop_scan_retains_only_requested_labels() {
    const std::string json = R"({"data":[
      {"stop":"A","name_tc":"甲"},{"stop":"B","name_tc":"乙"},
      {"stop":"C","name_tc":"丙"},{"stop":"D","name_tc":"丁"}
    ]})";
    StringReader reader(json);
    std::vector<std::string> requested{"B", "D"};
    std::vector<transitink::BusStopLabel> labels;
    std::string error;
    TEST_ASSERT_TRUE(transitink::parseMatchingBusStopLabels(reader, requested, labels, error));
    TEST_ASSERT_EQUAL_UINT32(2, labels.size());
    TEST_ASSERT_EQUAL_STRING("B", labels[0].stopId.c_str());
    TEST_ASSERT_EQUAL_STRING("D", labels[1].stopId.c_str());
}

void test_catalog_parser_rejects_malformed_and_oversized_objects() {
    std::vector<transitink::BusCatalogRoute> rows;
    std::string error;
    std::string malformed = R"({"data":[{"route":"1")";
    StringReader malformedReader(malformed);
    TEST_ASSERT_FALSE(transitink::parseBusRoutes(
        malformedReader, transitink::BusCatalogFormat::Kmb, rows, error));
    TEST_ASSERT_FALSE(error.empty());

    std::string oversized = R"({"data":[{"route":"1","bound":"O","service_type":"1","orig_tc":")";
    oversized += std::string(transitink::kMaxCatalogObjectBytes + 1, 'x');
    oversized += R"(","dest_tc":"D"}]})";
    StringReader oversizedReader(oversized);
    TEST_ASSERT_FALSE(transitink::parseBusRoutes(
        oversizedReader, transitink::BusCatalogFormat::Kmb, rows, error));
    TEST_ASSERT_FALSE(error.empty());
}

void test_route_id_streaming_is_bounded_and_directions_only_keep_selected_route() {
    // Official KMB route catalog measured 796 unique route IDs on 2026-07-12.
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(796, transitink::kMaxCatalogRoutes);
    std::string large = R"({"data":[)";
    for (std::size_t index = 0; index <= transitink::kMaxCatalogRoutes; ++index) {
        if (index != 0) {
            large += ',';
        }
        large += "{\"route\":\"R" + std::to_string(index) +
                 "\",\"bound\":\"O\",\"service_type\":1,\"orig_tc\":\"甲\",\"dest_tc\":\"乙\"}";
    }
    large += "]}";
    StringReader routeReader(large);
    std::vector<std::string> routeIds;
    std::string error;
    TEST_ASSERT_FALSE(transitink::parseBusRouteIds(
        routeReader, transitink::BusCatalogFormat::Kmb, routeIds, error));
    TEST_ASSERT_TRUE(routeIds.empty());

    std::string mixed = R"({"data":[)";
    for (int index = 0; index < 700; ++index) {
        if (index != 0) {
            mixed += ',';
        }
        const std::string route = index == 650 ? "H1" : "X" + std::to_string(index);
        mixed += "{\"route\":\"" + route +
                 "\",\"bound\":\"O\",\"service_type\":1,\"orig_tc\":\"甲\",\"dest_tc\":\"乙\"}";
    }
    mixed += "]}";
    StringReader directionReader(mixed);
    std::vector<transitink::BusCatalogRoute> directions;
    TEST_ASSERT_TRUE(transitink::parseBusDirectionsForRoute(
        directionReader, transitink::BusCatalogFormat::Kmb, "H1", directions, error));
    TEST_ASSERT_EQUAL_UINT32(1, directions.size());
    TEST_ASSERT_EQUAL_STRING("H1", directions[0].routeId.c_str());
}

void test_bounded_body_and_empty_catalog_contracts() {
    transitink::BoundedBodyAccumulator accumulator(4);
    std::string error;
    const uint8_t first[] = {'a', 'b'};
    const uint8_t second[] = {'c', 'd'};
    const uint8_t overflow[] = {'e'};
    TEST_ASSERT_TRUE(accumulator.append(first, sizeof(first), error));
    TEST_ASSERT_TRUE(accumulator.append(second, sizeof(second), error));
    TEST_ASSERT_TRUE(accumulator.complete(-1, error));
    TEST_ASSERT_FALSE(accumulator.complete(5, error));
    TEST_ASSERT_FALSE(accumulator.append(overflow, sizeof(overflow), error));

    StringReader emptyRoutes(R"({"data":[]})");
    std::vector<std::string> ids;
    TEST_ASSERT_FALSE(transitink::parseBusRouteIds(
        emptyRoutes, transitink::BusCatalogFormat::Kmb, ids, error));
    StringReader emptyStops(R"({"data":[]})");
    std::vector<transitink::BusStopLabel> labels;
    TEST_ASSERT_FALSE(transitink::parseBusStopLabels(emptyStops, labels, error));

    StringReader emptyRouteStops(R"({"data":[]})");
    std::vector<transitink::BusCatalogStop> stops;
    TEST_ASSERT_TRUE(transitink::parseBusRouteStops(
        emptyRouteStops, "H1", "O", "1", stops, error));
    TEST_ASSERT_TRUE(stops.empty());
}

void test_route_catalog_validation_scans_without_returning_rows() {
    const std::string json = R"({"data":[{"route":"1","bound":"I","service_type":"1","orig_tc":"甲","dest_tc":"乙"}]})";
    StringReader reader(json);
    std::string error;
    TEST_ASSERT_TRUE(transitink::validateBusRouteCatalog(
        reader, transitink::BusCatalogFormat::Kmb, error));
    TEST_ASSERT_TRUE(error.empty());
}

void test_invalid_query_is_rejected_before_fetch_callback() {
    int calls = 0;
    std::string error;
    auto fetch = [](void* context) {
        ++*static_cast<int*>(context);
        return true;
    };
    TEST_ASSERT_FALSE(transitink::runValidatedBusCatalogQuery(
        "unknown", "1", "I", "1", fetch, &calls, error));
    TEST_ASSERT_EQUAL_INT(0, calls);
    TEST_ASSERT_FALSE(transitink::runValidatedBusCatalogQuery(
        "ctb", "1/../../x", "I", "1", fetch, &calls, error));
    TEST_ASSERT_EQUAL_INT(0, calls);
    TEST_ASSERT_TRUE(transitink::runValidatedBusCatalogQuery(
        "lwb", "A31", "O", "1", fetch, &calls, error));
    TEST_ASSERT_EQUAL_INT(1, calls);
}

std::vector<transitink::BusStopLabel> labelsFor(
    std::initializer_list<const char*> ids) {
    std::vector<transitink::BusStopLabel> labels;
    for (const char* id : ids) {
        labels.push_back({id, std::string("站名") + id});
    }
    return labels;
}

void test_citybus_hydration_decision_handles_cache_bulk_fallback_limit_and_rollback() {
    const std::vector<std::string> required{"A", "B"};
    auto decision = transitink::resolveCitybusStopLabels(
        required, labelsFor({"A", "B"}), {}, {}, false, false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::CitybusStopResolution::FreshCache),
                          static_cast<int>(decision.resolution));
    TEST_ASSERT_FALSE(decision.shouldFetch);
    TEST_ASSERT_FALSE(decision.shouldCommit);

    decision = transitink::resolveCitybusStopLabels(required, {}, {}, {}, false, false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::CitybusStopResolution::Hydrate),
                          static_cast<int>(decision.resolution));
    TEST_ASSERT_TRUE(decision.shouldFetch);

    std::vector<std::string> tooMany(transitink::kMaxCitybusRouteStops + 1, "A");
    decision = transitink::resolveCitybusStopLabels(tooMany, {}, {}, {}, false, false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::CitybusStopResolution::Unavailable),
                          static_cast<int>(decision.resolution));
    TEST_ASSERT_FALSE(decision.error.empty());

    decision = transitink::resolveCitybusStopLabels(
        required, {}, labelsFor({"A", "B"}), labelsFor({"A"}), true, false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::CitybusStopResolution::StaleCache),
                          static_cast<int>(decision.resolution));
    TEST_ASSERT_FALSE(decision.shouldCommit);
    TEST_ASSERT_EQUAL_UINT32(2, decision.labels.size());

    decision = transitink::resolveCitybusStopLabels(
        required, {}, {}, labelsFor({"A", "B"}), true, true);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(transitink::CitybusStopResolution::Hydrated),
                          static_cast<int>(decision.resolution));
    TEST_ASSERT_TRUE(decision.shouldCommit);
}

std::string entrySignature(const std::vector<transitink::StaticCatalogEntry>& entries) {
    std::string signature;
    for (const auto& entry : entries) {
        signature += entry.id + "=" + entry.labelTc + "|";
    }
    return signature;
}

void test_static_rail_catalog_projection_returns_exact_bounded_entries() {
    std::vector<transitink::StaticCatalogEntry> entries;
    std::string error;
    TEST_ASSERT_TRUE(transitink::listStaticRailLines(
        transitink::RailMode::HeavyRail, entries, error));
    TEST_ASSERT_EQUAL_STRING(
        "AEL=機場快綫|TCL=東涌綫|TML=屯馬綫|TKL=將軍澳綫|EAL=東鐵綫|"
        "SIL=南港島綫|TWL=荃灣綫|ISL=港島綫|KTL=觀塘綫|DRL=迪士尼綫|",
        entrySignature(entries).c_str());
    TEST_ASSERT_TRUE(entries.size() <= transitink::kMaxStaticCatalogEntries);

    TEST_ASSERT_TRUE(transitink::listStaticRailStations(
        transitink::RailMode::HeavyRail, "AEL", entries, error));
    TEST_ASSERT_EQUAL_STRING(
        "HOK=香港|KOW=九龍|TSY=青衣|AIR=機場|AWE=博覽館|",
        entrySignature(entries).c_str());
    TEST_ASSERT_TRUE(transitink::listStaticRailDirections(
        transitink::RailMode::HeavyRail, "AEL", "AIR", entries, error));
    TEST_ASSERT_EQUAL_STRING(
        "UP=機場／博覽館方向|DOWN=香港方向|", entrySignature(entries).c_str());

    TEST_ASSERT_TRUE(transitink::listStaticRailLines(
        transitink::RailMode::LightRail, entries, error));
    TEST_ASSERT_EQUAL_STRING(
        "505=505|507=507|610=610|614=614|614P=614P|615=615|615P=615P|"
        "705=705|706=706|751=751|761P=761P|",
        entrySignature(entries).c_str());
    TEST_ASSERT_TRUE(transitink::listStaticRailStations(
        transitink::RailMode::LightRail, "505", entries, error));
    TEST_ASSERT_EQUAL_STRING(
        "920=三聖|265=兆麟|270=安定|280=市中心|295=屯門|60=建安|"
        "190=山景(南)|180=山景(北)|170=石排|160=新圍|150=良景|140=田景|"
        "130=建生|120=青松|110=麒麟|100=兆康|200=鳴琴|",
        entrySignature(entries).c_str());
    TEST_ASSERT_TRUE(transitink::listStaticRailDirections(
        transitink::RailMode::LightRail, "505", "280", entries, error));
    TEST_ASSERT_EQUAL_STRING("100=兆康|920=三聖|", entrySignature(entries).c_str());

    TEST_ASSERT_FALSE(transitink::listStaticRailStations(
        transitink::RailMode::HeavyRail, "BAD", entries, error));
    TEST_ASSERT_TRUE(entries.empty());
    TEST_ASSERT_FALSE(transitink::listStaticRailDirections(
        transitink::RailMode::LightRail, "505", "BAD", entries, error));
    TEST_ASSERT_TRUE(entries.empty());
}

void test_static_journey_catalog_projection_returns_exact_parent_filtered_entries() {
    std::vector<transitink::StaticCatalogEntry> entries;
    std::string error;
    TEST_ASSERT_TRUE(transitink::listStaticJourneyLocations(entries, error));
    TEST_ASSERT_EQUAL_STRING(
        "H1=告士打道東行近稅務大樓|H2=堅拿道天橋北行近香港仔隧道出口|"
        "H3=東區走廊西行近城市花園|H4=黃泥涌道北行近皇后大道東|"
        "H5=興發街北行近維多利亞公園|H6=淺水灣道北行近香島道|"
        "H7=黃竹坑道北行近香港鄉村俱樂部|H8=黃竹坑道東行近香港仔運動場|"
        "H9=鴨脷洲橋道北行近黃竹坑道|H11=東區走廊西行近鯉景灣|"
        "K01=渡船街南行近富榮花園|K02=加士居道東行近香港理工大學|"
        "K03=窩打老道南行近九龍醫院|K04=公主道南行近愛民邨|"
        "K05=啟福道北行近油站|K06=漆咸道北南行近佛光街遊樂場|"
        "K07=西九龍公路西行近港鐵南昌站|K08=啟祥道西行近九龍灣消防總局|"
        "N01=洪天路南行近洪志路|N02=朗天路南行近柏麗豪園|"
        "N03=元朗公路東行近十八鄉交匯處|N05=大埔公路東行近廣福邨|"
        "N06=青沙公路西行近城門河道|N07=福民路北行近普通道|"
        "N08=寶順路南行近頌明苑|N09=環保大道西行近香港單車館|"
        "N10=寶康路南行近九巴將軍澳車廠|N11=寶邑路西行近調景嶺體育館|"
        "N12=寶順路南行近調景嶺體育館|N13=翠嶺路東行近調景嶺體育館|"
        "SJ1=大埔公路南行近沙田馬場|SJ2=大老山隧道公路南行近石門|"
        "SJ3=吐露港公路南行近科學園|SJ4=新田公路南行近錦繡花園|"
        "SJ5=屯門公路南行近井財街|",
        entrySignature(entries).c_str());
    TEST_ASSERT_TRUE(entries.size() <= transitink::kMaxStaticCatalogEntries);

    TEST_ASSERT_TRUE(transitink::listStaticJourneyDestinations("H1", entries, error));
    TEST_ASSERT_EQUAL_STRING(
        "CH=紅磡海底隧道|EH=東區海底隧道|", entrySignature(entries).c_str());
    TEST_ASSERT_FALSE(transitink::listStaticJourneyDestinations("BAD", entries, error));
    TEST_ASSERT_TRUE(entries.empty());
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_kmb_routes_map_exact_direction_service_and_deduplicate);
    RUN_TEST(test_citybus_routes_create_official_inbound_outbound_selection_rows);
    RUN_TEST(test_citybus_h1_outbound_label_path_and_first_stop_match_live_contract);
    RUN_TEST(test_route_stops_join_complete_stop_catalog_and_sort_sequence);
    RUN_TEST(test_kmb_route_stops_accept_live_string_sequences);
    RUN_TEST(test_route_stops_reject_invalid_string_sequences);
    RUN_TEST(test_matching_stop_scan_retains_only_requested_labels);
    RUN_TEST(test_catalog_parser_rejects_malformed_and_oversized_objects);
    RUN_TEST(test_route_id_streaming_is_bounded_and_directions_only_keep_selected_route);
    RUN_TEST(test_bounded_body_and_empty_catalog_contracts);
    RUN_TEST(test_route_catalog_validation_scans_without_returning_rows);
    RUN_TEST(test_invalid_query_is_rejected_before_fetch_callback);
    RUN_TEST(test_citybus_hydration_decision_handles_cache_bulk_fallback_limit_and_rollback);
    RUN_TEST(test_static_rail_catalog_projection_returns_exact_bounded_entries);
    RUN_TEST(test_static_journey_catalog_projection_returns_exact_parent_filtered_entries);
    return UNITY_END();
}
