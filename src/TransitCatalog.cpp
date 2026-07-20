#include "TransitCatalog.h"

namespace transitink {
namespace {

template <typename T, std::size_t N>
constexpr std::size_t itemCount(const T (&)[N]) { return N; }

const TransitCatalogItem kAelStations[] = {
    {"HOK", "香港"},
    {"KOW", "九龍"},
    {"TSY", "青衣"},
    {"AIR", "機場"},
    {"AWE", "博覽館"},
};
const TransitCatalogItem kAelDirections[] = {
    {"UP", "機場／博覽館方向"},
    {"DOWN", "香港方向"},
};

const TransitCatalogItem kTclStations[] = {
    {"HOK", "香港"},
    {"KOW", "九龍"},
    {"OLY", "奧運"},
    {"NAC", "南昌"},
    {"LAK", "茘景"},
    {"TSY", "青衣"},
    {"SUN", "欣澳"},
    {"TUC", "東涌"},
};
const TransitCatalogItem kTclDirections[] = {
    {"UP", "東涌方向"},
    {"DOWN", "香港方向"},
};

const TransitCatalogItem kTmlStations[] = {
    {"WKS", "烏溪沙"},
    {"MOS", "馬鞍山"},
    {"HEO", "恆安"},
    {"TSH", "大水坑"},
    {"SHM", "石門"},
    {"CIO", "第一城"},
    {"STW", "沙田圍"},
    {"CKT", "車公廟"},
    {"TAW", "大圍"},
    {"HIK", "顯徑"},
    {"DIH", "鑽石山"},
    {"KAT", "啟德"},
    {"SUW", "宋皇臺"},
    {"TKW", "土瓜灣"},
    {"HOM", "何文田"},
    {"HUH", "紅磡"},
    {"ETS", "尖東"},
    {"AUS", "柯士甸"},
    {"NAC", "南昌"},
    {"MEF", "美孚"},
    {"TWW", "荃灣西"},
    {"KSR", "錦上路"},
    {"YUL", "元朗"},
    {"LOP", "朗屏"},
    {"TIS", "天水圍"},
    {"SIH", "兆康"},
    {"TUM", "屯門"},
};
const TransitCatalogItem kTmlDirections[] = {
    {"UP", "屯門方向"},
    {"DOWN", "烏溪沙方向"},
};

const TransitCatalogItem kTklStations[] = {
    {"NOP", "北角"},
    {"QUB", "鰂魚涌"},
    {"YAT", "油塘"},
    {"TIK", "調景嶺"},
    {"TKO", "將軍澳"},
    {"LHP", "康城"},
    {"HAH", "坑口"},
    {"POA", "寶琳"},
};
const TransitCatalogItem kTklDirections[] = {
    {"UP", "寶琳／康城方向"},
    {"DOWN", "北角方向"},
};

const TransitCatalogItem kEalStations[] = {
    {"ADM", "金鐘"},
    {"EXC", "會展"},
    {"HUH", "紅磡"},
    {"MKK", "旺角東"},
    {"KOT", "九龍塘"},
    {"TAW", "大圍"},
    {"SHT", "沙田"},
    {"FOT", "火炭"},
    {"RAC", "馬場"},
    {"UNI", "大學"},
    {"TAP", "大埔墟"},
    {"TWO", "太和"},
    {"FAN", "粉嶺"},
    {"SHS", "上水"},
    {"LOW", "羅湖"},
    {"LMC", "落馬洲"},
};
const TransitCatalogItem kEalDirections[] = {
    {"UP", "北行方向"},
    {"DOWN", "南行方向"},
};

const TransitCatalogItem kSilStations[] = {
    {"ADM", "金鐘"},
    {"OCP", "海洋公園"},
    {"WCH", "黃竹坑"},
    {"LET", "利東"},
    {"SOH", "海怡半島"},
};
const TransitCatalogItem kSilDirections[] = {
    {"UP", "海怡半島方向"},
    {"DOWN", "金鐘方向"},
};

const TransitCatalogItem kTwlStations[] = {
    {"CEN", "中環"},
    {"ADM", "金鐘"},
    {"TST", "尖沙咀"},
    {"JOR", "佐敦"},
    {"YMT", "油麻地"},
    {"MOK", "旺角"},
    {"PRE", "太子"},
    {"SSP", "深水埗"},
    {"CSW", "長沙灣"},
    {"LCK", "茘枝角"},
    {"MEF", "美孚"},
    {"LAK", "茘景"},
    {"KWF", "葵芳"},
    {"KWH", "葵興"},
    {"TWH", "大窩口"},
    {"TSW", "荃灣"},
};
const TransitCatalogItem kTwlDirections[] = {
    {"UP", "荃灣方向"},
    {"DOWN", "中環方向"},
};

const TransitCatalogItem kIslStations[] = {
    {"KET", "堅尼地城"},
    {"HKU", "香港大學"},
    {"SYP", "西營盤"},
    {"SHW", "上環"},
    {"CEN", "中環"},
    {"ADM", "金鐘"},
    {"WAC", "灣仔"},
    {"CAB", "銅鑼灣"},
    {"TIH", "天后"},
    {"FOH", "炮台山"},
    {"NOP", "北角"},
    {"QUB", "鰂魚涌"},
    {"TAK", "太古"},
    {"SWH", "西灣河"},
    {"SKW", "筲箕灣"},
    {"HFC", "杏花邨"},
    {"CHW", "柴灣"},
};
const TransitCatalogItem kIslDirections[] = {
    {"UP", "柴灣方向"},
    {"DOWN", "堅尼地城方向"},
};

const TransitCatalogItem kKtlStations[] = {
    {"WHA", "黃埔"},
    {"HOM", "何文田"},
    {"YMT", "油麻地"},
    {"MOK", "旺角"},
    {"PRE", "太子"},
    {"SKM", "石硤尾"},
    {"KOT", "九龍塘"},
    {"LOF", "樂富"},
    {"WTS", "黃大仙"},
    {"DIH", "鑽石山"},
    {"CHH", "彩虹"},
    {"KOB", "九龍灣"},
    {"NTK", "牛頭角"},
    {"KWT", "觀塘"},
    {"LAT", "藍田"},
    {"YAT", "油塘"},
    {"TIK", "調景嶺"},
};
const TransitCatalogItem kKtlDirections[] = {
    {"UP", "調景嶺方向"},
    {"DOWN", "黃埔方向"},
};

const TransitCatalogItem kDrlStations[] = {
    {"SUN", "欣澳"},
    {"DIS", "迪士尼"},
};
const TransitCatalogItem kDrlDirections[] = {
    {"UP", "欣澳方向"},
    {"DOWN", "迪士尼方向"},
};

const TransitCatalogGroup kHeavyRailGroups[] = {
    {"AEL", "機場快綫", kAelStations, itemCount(kAelStations), kAelDirections, itemCount(kAelDirections)},
    {"TCL", "東涌綫", kTclStations, itemCount(kTclStations), kTclDirections, itemCount(kTclDirections)},
    {"TML", "屯馬綫", kTmlStations, itemCount(kTmlStations), kTmlDirections, itemCount(kTmlDirections)},
    {"TKL", "將軍澳綫", kTklStations, itemCount(kTklStations), kTklDirections, itemCount(kTklDirections)},
    {"EAL", "東鐵綫", kEalStations, itemCount(kEalStations), kEalDirections, itemCount(kEalDirections)},
    {"SIL", "南港島綫", kSilStations, itemCount(kSilStations), kSilDirections, itemCount(kSilDirections)},
    {"TWL", "荃灣綫", kTwlStations, itemCount(kTwlStations), kTwlDirections, itemCount(kTwlDirections)},
    {"ISL", "港島綫", kIslStations, itemCount(kIslStations), kIslDirections, itemCount(kIslDirections)},
    {"KTL", "觀塘綫", kKtlStations, itemCount(kKtlStations), kKtlDirections, itemCount(kKtlDirections)},
    {"DRL", "迪士尼綫", kDrlStations, itemCount(kDrlStations), kDrlDirections, itemCount(kDrlDirections)},
};

const TransitCatalogItem kLr505Stations[] = {
    {"920", "三聖"},
    {"265", "兆麟"},
    {"270", "安定"},
    {"280", "市中心"},
    {"295", "屯門"},
    {"60", "建安"},
    {"190", "山景(南)"},
    {"180", "山景(北)"},
    {"170", "石排"},
    {"160", "新圍"},
    {"150", "良景"},
    {"140", "田景"},
    {"130", "建生"},
    {"120", "青松"},
    {"110", "麒麟"},
    {"100", "兆康"},
    {"200", "鳴琴"},
};
const TransitCatalogItem kLr505Directions[] = {
    {"100", "兆康"},
    {"920", "三聖"},
};

const TransitCatalogItem kLr507Stations[] = {
    {"1", "屯門碼頭"},
    {"240", "兆禧"},
    {"250", "海皇路"},
    {"260", "豐景園"},
    {"265", "兆麟"},
    {"270", "安定"},
    {"280", "市中心"},
    {"295", "屯門"},
    {"70", "河田"},
    {"75", "蔡意橋"},
    {"230", "銀圍"},
    {"220", "大興(南)"},
    {"212", "大興(北)"},
    {"160", "新圍"},
    {"150", "良景"},
    {"140", "田景"},
};
const TransitCatalogItem kLr507Directions[] = {
    {"1", "屯門碼頭"},
    {"140", "田景"},
};

const TransitCatalogItem kLr610Stations[] = {
    {"1", "屯門碼頭"},
    {"10", "美樂"},
    {"15", "蝴蝶"},
    {"20", "輕鐵車廠"},
    {"30", "龍門"},
    {"40", "青山村"},
    {"50", "青雲"},
    {"200", "鳴琴"},
    {"170", "石排"},
    {"212", "大興(北)"},
    {"220", "大興(南)"},
    {"230", "銀圍"},
    {"80", "澤豐"},
    {"90", "屯門醫院"},
    {"100", "兆康"},
    {"350", "藍地"},
    {"360", "泥圍"},
    {"370", "鍾屋村"},
    {"380", "洪水橋"},
    {"390", "塘坊村"},
    {"400", "屏山"},
    {"560", "水邊圍"},
    {"570", "豐年路"},
    {"580", "康樂路"},
    {"590", "大棠路"},
    {"600", "元朗"},
};
const TransitCatalogItem kLr610Directions[] = {
    {"1", "屯門碼頭"},
    {"600", "元朗"},
};

const TransitCatalogItem kLr614Stations[] = {
    {"1", "屯門碼頭"},
    {"240", "兆禧"},
    {"250", "海皇路"},
    {"260", "豐景園"},
    {"265", "兆麟"},
    {"270", "安定"},
    {"280", "市中心"},
    {"300", "杯渡"},
    {"310", "何福堂"},
    {"320", "新墟"},
    {"330", "景峰"},
    {"340", "鳳地"},
    {"100", "兆康"},
    {"350", "藍地"},
    {"360", "泥圍"},
    {"370", "鍾屋村"},
    {"380", "洪水橋"},
    {"390", "塘坊村"},
    {"400", "屏山"},
    {"560", "水邊圍"},
    {"570", "豐年路"},
    {"580", "康樂路"},
    {"590", "大棠路"},
    {"600", "元朗"},
};
const TransitCatalogItem kLr614Directions[] = {
    {"1", "屯門碼頭"},
    {"600", "元朗"},
};

const TransitCatalogItem kLr614PStations[] = {
    {"1", "屯門碼頭"},
    {"240", "兆禧"},
    {"250", "海皇路"},
    {"260", "豐景園"},
    {"265", "兆麟"},
    {"270", "安定"},
    {"280", "市中心"},
    {"300", "杯渡"},
    {"310", "何福堂"},
    {"320", "新墟"},
    {"330", "景峰"},
    {"340", "鳳地"},
    {"100", "兆康"},
};
const TransitCatalogItem kLr614PDirections[] = {
    {"1", "屯門碼頭"},
    {"100", "兆康"},
};

const TransitCatalogItem kLr615Stations[] = {
    {"1", "屯門碼頭"},
    {"10", "美樂"},
    {"15", "蝴蝶"},
    {"20", "輕鐵車廠"},
    {"30", "龍門"},
    {"40", "青山村"},
    {"50", "青雲"},
    {"200", "鳴琴"},
    {"170", "石排"},
    {"160", "新圍"},
    {"150", "良景"},
    {"140", "田景"},
    {"130", "建生"},
    {"120", "青松"},
    {"100", "兆康"},
    {"350", "藍地"},
    {"360", "泥圍"},
    {"370", "鍾屋村"},
    {"380", "洪水橋"},
    {"390", "塘坊村"},
    {"400", "屏山"},
    {"560", "水邊圍"},
    {"570", "豐年路"},
    {"580", "康樂路"},
    {"590", "大棠路"},
    {"600", "元朗"},
};
const TransitCatalogItem kLr615Directions[] = {
    {"1", "屯門碼頭"},
    {"600", "元朗"},
};

const TransitCatalogItem kLr615PStations[] = {
    {"100", "兆康"},
    {"110", "麒麟"},
    {"120", "青松"},
    {"130", "建生"},
    {"140", "田景"},
    {"150", "良景"},
    {"160", "新圍"},
    {"170", "石排"},
    {"200", "鳴琴"},
    {"50", "青雲"},
    {"40", "青山村"},
    {"30", "龍門"},
    {"20", "輕鐵車廠"},
    {"15", "蝴蝶"},
    {"10", "美樂"},
    {"1", "屯門碼頭"},
};
const TransitCatalogItem kLr615PDirections[] = {
    {"1", "屯門碼頭"},
    {"100", "兆康"},
};

const TransitCatalogItem kLr705Stations[] = {
    {"430", "天水圍"},
    {"435", "天慈"},
    {"450", "天湖"},
    {"455", "銀座"},
    {"500", "天榮"},
    {"510", "天悅"},
    {"520", "天秀"},
    {"530", "濕地公園"},
    {"540", "天恒"},
    {"550", "天逸"},
    {"480", "天富"},
    {"468", "頌富"},
    {"460", "天瑞"},
    {"448", "樂湖"},
    {"445", "天耀"},
};
const TransitCatalogItem kLr705Directions[] = {
    {"430", "天水圍"},
};

const TransitCatalogItem kLr706Stations[] = {
    {"430", "天水圍"},
    {"445", "天耀"},
    {"448", "樂湖"},
    {"460", "天瑞"},
    {"468", "頌富"},
    {"480", "天富"},
    {"550", "天逸"},
    {"540", "天恒"},
    {"530", "濕地公園"},
    {"520", "天秀"},
    {"510", "天悅"},
    {"500", "天榮"},
    {"455", "銀座"},
    {"450", "天湖"},
    {"435", "天慈"},
};
const TransitCatalogItem kLr706Directions[] = {
    {"430", "天水圍"},
};

const TransitCatalogItem kLr751Stations[] = {
    {"275", "友愛"},
    {"270", "安定"},
    {"280", "市中心"},
    {"295", "屯門"},
    {"70", "河田"},
    {"75", "蔡意橋"},
    {"80", "澤豐"},
    {"90", "屯門醫院"},
    {"100", "兆康"},
    {"350", "藍地"},
    {"360", "泥圍"},
    {"370", "鍾屋村"},
    {"380", "洪水橋"},
    {"425", "坑尾村"},
    {"430", "天水圍"},
    {"435", "天慈"},
    {"450", "天湖"},
    {"455", "銀座"},
    {"500", "天榮"},
    {"490", "翠湖"},
    {"468", "頌富"},
    {"480", "天富"},
    {"550", "天逸"},
};
const TransitCatalogItem kLr751Directions[] = {
    {"275", "友愛"},
    {"550", "天逸"},
};

const TransitCatalogItem kLr761PStations[] = {
    {"550", "天逸"},
    {"480", "天富"},
    {"468", "頌富"},
    {"460", "天瑞"},
    {"448", "樂湖"},
    {"445", "天耀"},
    {"425", "坑尾村"},
    {"390", "塘坊村"},
    {"400", "屏山"},
    {"560", "水邊圍"},
    {"570", "豐年路"},
    {"580", "康樂路"},
    {"590", "大棠路"},
    {"600", "元朗"},
};
const TransitCatalogItem kLr761PDirections[] = {
    {"550", "天逸"},
    {"600", "元朗"},
};

const TransitCatalogGroup kLightRailGroups[] = {
    {"505", "505", kLr505Stations, itemCount(kLr505Stations), kLr505Directions, itemCount(kLr505Directions)},
    {"507", "507", kLr507Stations, itemCount(kLr507Stations), kLr507Directions, itemCount(kLr507Directions)},
    {"610", "610", kLr610Stations, itemCount(kLr610Stations), kLr610Directions, itemCount(kLr610Directions)},
    {"614", "614", kLr614Stations, itemCount(kLr614Stations), kLr614Directions, itemCount(kLr614Directions)},
    {"614P", "614P", kLr614PStations, itemCount(kLr614PStations), kLr614PDirections, itemCount(kLr614PDirections)},
    {"615", "615", kLr615Stations, itemCount(kLr615Stations), kLr615Directions, itemCount(kLr615Directions)},
    {"615P", "615P", kLr615PStations, itemCount(kLr615PStations), kLr615PDirections, itemCount(kLr615PDirections)},
    {"705", "705", kLr705Stations, itemCount(kLr705Stations), kLr705Directions, itemCount(kLr705Directions)},
    {"706", "706", kLr706Stations, itemCount(kLr706Stations), kLr706Directions, itemCount(kLr706Directions)},
    {"751", "751", kLr751Stations, itemCount(kLr751Stations), kLr751Directions, itemCount(kLr751Directions)},
    {"761P", "761P", kLr761PStations, itemCount(kLr761PStations), kLr761PDirections, itemCount(kLr761PDirections)},
};

struct LightRailDestination {
    const char* routeId;
    const char* destinationText;
    const char* directionId;
};

const LightRailDestination kLightRailDestinations[] = {
    {"505", "兆康", "100"},
    {"505", "Siu Hong", "100"},
    {"505", "三聖", "920"},
    {"505", "Sam Shing", "920"},
    {"507", "屯門碼頭", "1"},
    {"507", "Tuen Mun Ferry Pier", "1"},
    {"507", "田景", "140"},
    {"507", "Tin King", "140"},
    {"610", "屯門碼頭", "1"},
    {"610", "Tuen Mun Ferry Pier", "1"},
    {"610", "元朗", "600"},
    {"610", "Yuen Long", "600"},
    {"614", "屯門碼頭", "1"},
    {"614", "Tuen Mun Ferry Pier", "1"},
    {"614", "元朗", "600"},
    {"614", "Yuen Long", "600"},
    {"614P", "屯門碼頭", "1"},
    {"614P", "Tuen Mun Ferry Pier", "1"},
    {"614P", "兆康", "100"},
    {"614P", "Siu Hong", "100"},
    {"615", "屯門碼頭", "1"},
    {"615", "Tuen Mun Ferry Pier", "1"},
    {"615", "元朗", "600"},
    {"615", "Yuen Long", "600"},
    {"615P", "屯門碼頭", "1"},
    {"615P", "Tuen Mun Ferry Pier", "1"},
    {"615P", "兆康", "100"},
    {"615P", "Siu Hong", "100"},
    {"705", "天水圍", "430"},
    {"705", "Tin Shui Wai", "430"},
    {"706", "天水圍", "430"},
    {"706", "Tin Shui Wai", "430"},
    {"751", "友愛", "275"},
    {"751", "Yau Oi", "275"},
    {"751", "天逸", "550"},
    {"751", "Tin Yat", "550"},
    {"761P", "天逸", "550"},
    {"761P", "Tin Yat", "550"},
    {"761P", "元朗", "600"},
    {"761P", "Yuen Long", "600"},
};

// Official Journey Time Indicators v2 snapshot provenance:
// URL: https://resource.data.one.gov.hk/td/jss/Journeytimev2.xml
// Retrieved UTC: 2026-07-10T14:16:27Z
// Bytes: 27285; SHA-256:
// 20f96b2a4c2484ae0dc41ce3e8594e4860dbe6850919943d7b2a346f8acc16a7
// Validated rows: 83 unique pairs, 35 locations, 27 destinations
// (80 type 1 records and 3 type 2 records). Labels are from the official
// Traditional Chinese data specification retrieved in the same validation run.
const TransitCatalogItem kJourneyTimeLocations[] = {
    {"H1", "告士打道東行近稅務大樓"},
    {"H2", "堅拿道天橋北行近香港仔隧道出口"},
    {"H3", "東區走廊西行近城市花園"},
    {"H4", "黃泥涌道北行近皇后大道東"},
    {"H5", "興發街北行近維多利亞公園"},
    {"H6", "淺水灣道北行近香島道"},
    {"H7", "黃竹坑道北行近香港鄉村俱樂部"},
    {"H8", "黃竹坑道東行近香港仔運動場"},
    {"H9", "鴨脷洲橋道北行近黃竹坑道"},
    {"H11", "東區走廊西行近鯉景灣"},
    {"K01", "渡船街南行近富榮花園"},
    {"K02", "加士居道東行近香港理工大學"},
    {"K03", "窩打老道南行近九龍醫院"},
    {"K04", "公主道南行近愛民邨"},
    {"K05", "啟福道北行近油站"},
    {"K06", "漆咸道北南行近佛光街遊樂場"},
    {"K07", "西九龍公路西行近港鐵南昌站"},
    {"K08", "啟祥道西行近九龍灣消防總局"},
    {"N01", "洪天路南行近洪志路"},
    {"N02", "朗天路南行近柏麗豪園"},
    {"N03", "元朗公路東行近十八鄉交匯處"},
    {"N05", "大埔公路東行近廣福邨"},
    {"N06", "青沙公路西行近城門河道"},
    {"N07", "福民路北行近普通道"},
    {"N08", "寶順路南行近頌明苑"},
    {"N09", "環保大道西行近香港單車館"},
    {"N10", "寶康路南行近九巴將軍澳車廠"},
    {"N11", "寶邑路西行近調景嶺體育館"},
    {"N12", "寶順路南行近調景嶺體育館"},
    {"N13", "翠嶺路東行近調景嶺體育館"},
    {"SJ1", "大埔公路南行近沙田馬場"},
    {"SJ2", "大老山隧道公路南行近石門"},
    {"SJ3", "吐露港公路南行近科學園"},
    {"SJ4", "新田公路南行近錦繡花園"},
    {"SJ5", "屯門公路南行近井財街"},
};

const TransitCatalogItem kJourneyTimeDestinations[] = {
    {"CH", "紅磡海底隧道"},
    {"EH", "東區海底隧道"},
    {"WH", "西區海底隧道"},
    {"ABT", "灣仔經香港仔隧道"},
    {"WNCG", "灣仔經黃泥涌峽道"},
    {"PFL", "中區經薄扶林道"},
    {"ACTT", "機場經三號幹線"},
    {"TMCLK", "機場經屯門赤鱲角隧道"},
    {"ATL", "機場經大欖隧道"},
    {"ATSCA", "機場經八號幹線"},
    {"SSCPR", "上水經青山公路"},
    {"SSYLH", "上水經九號幹線"},
    {"LRT", "九龍(中)經獅子山隧道"},
    {"SMT", "荃灣經城門隧道"},
    {"TCT", "九龍(東)經大老山隧道"},
    {"TKTL", "汀九經大欖隧道"},
    {"TKTM", "汀九經屯門公路"},
    {"TLH", "沙田經吐露港公路"},
    {"TPR", "沙田經大埔公路"},
    {"KTPR", "九龍經大埔公路"},
    {"TSCA", "九龍(西)經八號幹線"},
    {"TWCP", "荃灣(西)經青山公路"},
    {"TWTM", "荃灣(西)經屯門公路"},
    {"CWBR", "九龍經清水灣道"},
    {"MOS", "九龍經二號幹線"},
    {"TKOLTT", "九龍經將軍澳藍田隧道"},
    {"TKOT", "九龍經將軍澳隧道"},
};

const JourneyTimeCatalogPair kJourneyTimePairs[] = {
    {"H1", "CH"},       {"H1", "EH"},       {"H2", "CH"},
    {"H2", "EH"},       {"H2", "WH"},       {"H3", "CH"},
    {"H3", "WH"},       {"H4", "CH"},       {"H4", "EH"},
    {"H4", "WH"},       {"H5", "CH"},       {"H5", "EH"},
    {"H5", "WH"},       {"H6", "ABT"},      {"H6", "WNCG"},
    {"H7", "ABT"},      {"H7", "PFL"},      {"H7", "WNCG"},
    {"H8", "ABT"},      {"H8", "WNCG"},     {"H9", "ABT"},
    {"H9", "PFL"},      {"H11", "CH"},      {"H11", "EH"},
    {"K01", "CH"},      {"K01", "WH"},      {"K02", "CH"},
    {"K02", "EH"},      {"K03", "CH"},      {"K03", "EH"},
    {"K03", "WH"},      {"K04", "CH"},      {"K04", "WH"},
    {"K05", "CH"},      {"K05", "WH"},      {"K06", "CH"},
    {"K06", "WH"},      {"K07", "ACTT"},    {"K07", "ATSCA"},
    {"K08", "CH"},      {"K08", "EH"},      {"K08", "WH"},
    {"N01", "ATL"},     {"N01", "TMCLK"},   {"N02", "ATL"},
    {"N02", "TMCLK"},   {"N03", "SSCPR"},   {"N03", "SSYLH"},
    {"N05", "TLH"},     {"N05", "TPR"},     {"N06", "KTPR"},
    {"N06", "TSCA"},    {"N07", "CWBR"},    {"N07", "MOS"},
    {"N08", "TKOLTT"},  {"N08", "TKOT"},    {"N09", "TKOLTT"},
    {"N09", "TKOT"},    {"N10", "TKOLTT"},  {"N10", "TKOT"},
    {"N11", "TKOLTT"},  {"N11", "TKOT"},    {"N11", "EH"},
    {"N12", "EH"},      {"N12", "TKOLTT"},  {"N13", "TKOLTT"},
    {"N13", "TKOT"},    {"N13", "EH"},      {"SJ1", "LRT"},
    {"SJ1", "SMT"},     {"SJ1", "TSCA"},    {"SJ2", "LRT"},
    {"SJ2", "TCT"},     {"SJ2", "TSCA"},    {"SJ3", "LRT"},
    {"SJ3", "TCT"},     {"SJ3", "TSCA"},    {"SJ4", "ATL"},
    {"SJ4", "TMCLK"},   {"SJ4", "TKTM"},    {"SJ4", "TKTL"},
    {"SJ5", "TWCP"},    {"SJ5", "TWTM"},
};

}  // namespace

TransitCatalogView heavyRailCatalog() {
    return {kHeavyRailGroups, itemCount(kHeavyRailGroups)};
}

TransitCatalogView lightRailCatalog() {
    return {kLightRailGroups, itemCount(kLightRailGroups)};
}

JourneyTimeCatalogView journeyTimeCatalog() {
    return {kJourneyTimeLocations, itemCount(kJourneyTimeLocations),
            kJourneyTimeDestinations, itemCount(kJourneyTimeDestinations),
            kJourneyTimePairs, itemCount(kJourneyTimePairs)};
}

const TransitCatalogItem* findJourneyTimeLocation(
    const std::string& locationId) {
    for (const auto& item : kJourneyTimeLocations) {
        if (locationId == item.id) return &item;
    }
    return nullptr;
}

const TransitCatalogItem* findJourneyTimeDestination(
    const std::string& destinationId) {
    for (const auto& item : kJourneyTimeDestinations) {
        if (destinationId == item.id) return &item;
    }
    return nullptr;
}

bool isJourneyTimePairValid(const std::string& locationId,
                            const std::string& destinationId) {
    for (const auto& pair : kJourneyTimePairs) {
        if (locationId == pair.locationId && destinationId == pair.destinationId) {
            return true;
        }
    }
    return false;
}

const TransitCatalogGroup* findTransitCatalogGroup(
    RailMode mode, const std::string& groupId) {
    if (mode == RailMode::HeavyRail) {
        for (const auto& group : kHeavyRailGroups) {
            if (groupId == group.id) return &group;
        }
    } else {
        for (const auto& group : kLightRailGroups) {
            if (groupId == group.id) return &group;
        }
    }
    return nullptr;
}

const TransitCatalogItem* findTransitCatalogStation(
    RailMode mode, const std::string& groupId, const std::string& stationId) {
    const TransitCatalogGroup* group = findTransitCatalogGroup(mode, groupId);
    if (group == nullptr) return nullptr;
    for (std::size_t index = 0; index < group->stationCount; ++index) {
        if (stationId == group->stations[index].id) return &group->stations[index];
    }
    return nullptr;
}

const TransitCatalogItem* findTransitCatalogDirection(
    RailMode mode, const std::string& groupId, const std::string& directionId) {
    const TransitCatalogGroup* group = findTransitCatalogGroup(mode, groupId);
    if (group == nullptr) return nullptr;
    for (std::size_t index = 0; index < group->directionCount; ++index) {
        if (directionId == group->directions[index].id) {
            return &group->directions[index];
        }
    }
    return nullptr;
}

bool lightRailDirectionIdForDestination(
    const std::string& routeId, const std::string& destinationText,
    std::string& directionId) {
    directionId.clear();
    for (const auto& entry : kLightRailDestinations) {
        if (routeId == entry.routeId && destinationText == entry.destinationText) {
            directionId = entry.directionId;
            return true;
        }
    }
    return false;
}

}  // namespace transitink
