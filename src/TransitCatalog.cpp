#include "TransitCatalog.h"

namespace transitink {
namespace {

template <typename T, std::size_t N>
constexpr std::size_t itemCount(const T (&)[N]) { return N; }

const TransitCatalogItem kAelStations[] = {
    {"HOK", "香港", "Hong Kong"},
    {"KOW", "九龍", "Kowloon"},
    {"TSY", "青衣", "Tsing Yi"},
    {"AIR", "機場", "Airport"},
    {"AWE", "博覽館", "AsiaWorld-Expo"},
};
const TransitCatalogItem kAelDirections[] = {
    {"UP", "機場／博覽館方向", "Airport / AsiaWorld-Expo bound"},
    {"DOWN", "香港方向", "Hong Kong bound"},
};

const TransitCatalogItem kTclStations[] = {
    {"HOK", "香港", "Hong Kong"},
    {"KOW", "九龍", "Kowloon"},
    {"OLY", "奧運", "Olympic"},
    {"NAC", "南昌", "Nam Cheong"},
    {"LAK", "茘景", "Lai King"},
    {"TSY", "青衣", "Tsing Yi"},
    {"SUN", "欣澳", "Sunny Bay"},
    {"TUC", "東涌", "Tung Chung"},
};
const TransitCatalogItem kTclDirections[] = {
    {"UP", "東涌方向", "Tung Chung bound"},
    {"DOWN", "香港方向", "Hong Kong bound"},
};

const TransitCatalogItem kTmlStations[] = {
    {"WKS", "烏溪沙", "Wu Kai Sha"},
    {"MOS", "馬鞍山", "Ma On Shan"},
    {"HEO", "恆安", "Heng On"},
    {"TSH", "大水坑", "Tai Shui Hang"},
    {"SHM", "石門", "Shek Mun"},
    {"CIO", "第一城", "City One"},
    {"STW", "沙田圍", "Sha Tin Wai"},
    {"CKT", "車公廟", "Che Kung Temple"},
    {"TAW", "大圍", "Tai Wai"},
    {"HIK", "顯徑", "Hin Keng"},
    {"DIH", "鑽石山", "Diamond Hill"},
    {"KAT", "啟德", "Kai Tak"},
    {"SUW", "宋皇臺", "Sung Wong Toi"},
    {"TKW", "土瓜灣", "To Kwa Wan"},
    {"HOM", "何文田", "Ho Man Tin"},
    {"HUH", "紅磡", "Hung Hom"},
    {"ETS", "尖東", "East Tsim Sha Tsui"},
    {"AUS", "柯士甸", "Austin"},
    {"NAC", "南昌", "Nam Cheong"},
    {"MEF", "美孚", "Mei Foo"},
    {"TWW", "荃灣西", "Tsuen Wan West"},
    {"KSR", "錦上路", "Kam Sheung Road"},
    {"YUL", "元朗", "Yuen Long"},
    {"LOP", "朗屏", "Long Ping"},
    {"TIS", "天水圍", "Tin Shui Wai"},
    {"SIH", "兆康", "Siu Hong"},
    {"TUM", "屯門", "Tuen Mun"},
};
const TransitCatalogItem kTmlDirections[] = {
    {"UP", "屯門方向", "Tuen Mun bound"},
    {"DOWN", "烏溪沙方向", "Wu Kai Sha bound"},
};

const TransitCatalogItem kTklStations[] = {
    {"NOP", "北角", "North Point"},
    {"QUB", "鰂魚涌", "Quarry Bay"},
    {"YAT", "油塘", "Yau Tong"},
    {"TIK", "調景嶺", "Tiu Keng Leng"},
    {"TKO", "將軍澳", "Tseung Kwan O"},
    {"LHP", "康城", "LOHAS Park"},
    {"HAH", "坑口", "Hang Hau"},
    {"POA", "寶琳", "Po Lam"},
};
const TransitCatalogItem kTklDirections[] = {
    {"UP", "寶琳／康城方向", "Po Lam / LOHAS Park bound"},
    {"DOWN", "北角方向", "North Point bound"},
};

const TransitCatalogItem kEalStations[] = {
    {"ADM", "金鐘", "Admiralty"},
    {"EXC", "會展", "Exhibition Centre"},
    {"HUH", "紅磡", "Hung Hom"},
    {"MKK", "旺角東", "Mong Kok East"},
    {"KOT", "九龍塘", "Kowloon Tong"},
    {"TAW", "大圍", "Tai Wai"},
    {"SHT", "沙田", "Sha Tin"},
    {"FOT", "火炭", "Fo Tan"},
    {"RAC", "馬場"},
    {"UNI", "大學", "University"},
    {"TAP", "大埔墟", "Tai Po Market"},
    {"TWO", "太和", "Tai Wo"},
    {"FAN", "粉嶺", "Fanling"},
    {"SHS", "上水", "Sheung Shui"},
    {"LOW", "羅湖", "Lo Wu"},
    {"LMC", "落馬洲", "Lok Ma Chau"},
};
const TransitCatalogItem kEalDirections[] = {
    {"UP", "北行方向", "Northbound"},
    {"DOWN", "南行方向", "Southbound"},
};

const TransitCatalogItem kSilStations[] = {
    {"ADM", "金鐘", "Admiralty"},
    {"OCP", "海洋公園", "Ocean Park"},
    {"WCH", "黃竹坑", "Wong Chuk Hang"},
    {"LET", "利東", "Lei Tung"},
    {"SOH", "海怡半島", "South Horizons"},
};
const TransitCatalogItem kSilDirections[] = {
    {"UP", "海怡半島方向", "South Horizons bound"},
    {"DOWN", "金鐘方向", "Admiralty bound"},
};

const TransitCatalogItem kTwlStations[] = {
    {"CEN", "中環", "Central"},
    {"ADM", "金鐘", "Admiralty"},
    {"TST", "尖沙咀", "Tsim Sha Tsui"},
    {"JOR", "佐敦", "Jordan"},
    {"YMT", "油麻地", "Yau Ma Tei"},
    {"MOK", "旺角", "Mong Kok"},
    {"PRE", "太子", "Prince Edward"},
    {"SSP", "深水埗", "Sham Shui Po"},
    {"CSW", "長沙灣", "Cheung Sha Wan"},
    {"LCK", "茘枝角", "Lai Chi Kok"},
    {"MEF", "美孚", "Mei Foo"},
    {"LAK", "茘景", "Lai King"},
    {"KWF", "葵芳", "Kwai Fong"},
    {"KWH", "葵興", "Kwai Hing"},
    {"TWH", "大窩口", "Tai Wo Hau"},
    {"TSW", "荃灣", "Tsuen Wan"},
};
const TransitCatalogItem kTwlDirections[] = {
    {"UP", "荃灣方向", "Tsuen Wan bound"},
    {"DOWN", "中環方向", "Central bound"},
};

const TransitCatalogItem kIslStations[] = {
    {"KET", "堅尼地城", "Kennedy Town"},
    {"HKU", "香港大學", "HKU"},
    {"SYP", "西營盤", "Sai Ying Pun"},
    {"SHW", "上環", "Sheung Wan"},
    {"CEN", "中環", "Central"},
    {"ADM", "金鐘", "Admiralty"},
    {"WAC", "灣仔", "Wan Chai"},
    {"CAB", "銅鑼灣", "Causeway Bay"},
    {"TIH", "天后", "Tin Hau"},
    {"FOH", "炮台山", "Fortress Hill"},
    {"NOP", "北角", "North Point"},
    {"QUB", "鰂魚涌", "Quarry Bay"},
    {"TAK", "太古", "Tai Koo"},
    {"SWH", "西灣河", "Sai Wan Ho"},
    {"SKW", "筲箕灣", "Shau Kei Wan"},
    {"HFC", "杏花邨", "Heng Fa Chuen"},
    {"CHW", "柴灣", "Chai Wan"},
};
const TransitCatalogItem kIslDirections[] = {
    {"UP", "柴灣方向", "Chai Wan bound"},
    {"DOWN", "堅尼地城方向", "Kennedy Town bound"},
};

const TransitCatalogItem kKtlStations[] = {
    {"WHA", "黃埔", "Whampoa"},
    {"HOM", "何文田", "Ho Man Tin"},
    {"YMT", "油麻地", "Yau Ma Tei"},
    {"MOK", "旺角", "Mong Kok"},
    {"PRE", "太子", "Prince Edward"},
    {"SKM", "石硤尾", "Shek Kip Mei"},
    {"KOT", "九龍塘", "Kowloon Tong"},
    {"LOF", "樂富", "Lok Fu"},
    {"WTS", "黃大仙", "Wong Tai Sin"},
    {"DIH", "鑽石山", "Diamond Hill"},
    {"CHH", "彩虹", "Choi Hung"},
    {"KOB", "九龍灣", "Kowloon Bay"},
    {"NTK", "牛頭角", "Ngau Tau Kok"},
    {"KWT", "觀塘", "Kwun Tong"},
    {"LAT", "藍田", "Lam Tin"},
    {"YAT", "油塘", "Yau Tong"},
    {"TIK", "調景嶺", "Tiu Keng Leng"},
};
const TransitCatalogItem kKtlDirections[] = {
    {"UP", "調景嶺方向", "Tiu Keng Leng bound"},
    {"DOWN", "黃埔方向", "Whampoa bound"},
};

const TransitCatalogItem kDrlStations[] = {
    {"SUN", "欣澳", "Sunny Bay"},
    {"DIS", "迪士尼", "Disneyland Resort"},
};
const TransitCatalogItem kDrlDirections[] = {
    {"UP", "欣澳方向", "Sunny Bay bound"},
    {"DOWN", "迪士尼方向", "Disneyland Resort bound"},
};

const TransitCatalogGroup kHeavyRailGroups[] = {
    {"AEL", "機場快綫", kAelStations, itemCount(kAelStations), kAelDirections, itemCount(kAelDirections), "Airport Express"},
    {"TCL", "東涌綫", kTclStations, itemCount(kTclStations), kTclDirections, itemCount(kTclDirections), "Tung Chung Line"},
    {"TML", "屯馬綫", kTmlStations, itemCount(kTmlStations), kTmlDirections, itemCount(kTmlDirections), "Tuen Ma Line"},
    {"TKL", "將軍澳綫", kTklStations, itemCount(kTklStations), kTklDirections, itemCount(kTklDirections), "Tseung Kwan O Line"},
    {"EAL", "東鐵綫", kEalStations, itemCount(kEalStations), kEalDirections, itemCount(kEalDirections), "East Rail Line"},
    {"SIL", "南港島綫", kSilStations, itemCount(kSilStations), kSilDirections, itemCount(kSilDirections), "South Island Line"},
    {"TWL", "荃灣綫", kTwlStations, itemCount(kTwlStations), kTwlDirections, itemCount(kTwlDirections), "Tsuen Wan Line"},
    {"ISL", "港島綫", kIslStations, itemCount(kIslStations), kIslDirections, itemCount(kIslDirections), "Island Line"},
    {"KTL", "觀塘綫", kKtlStations, itemCount(kKtlStations), kKtlDirections, itemCount(kKtlDirections), "Kwun Tong Line"},
    {"DRL", "迪士尼綫", kDrlStations, itemCount(kDrlStations), kDrlDirections, itemCount(kDrlDirections), "Disneyland Resort Line"},
};

const TransitCatalogItem kLr505Stations[] = {
    {"920", "三聖", "Sam Shing"},
    {"265", "兆麟", "Siu Lun"},
    {"270", "安定", "On Ting"},
    {"280", "市中心", "Town Centre"},
    {"295", "屯門", "Tuen Mun"},
    {"60", "建安", "Kin On"},
    {"190", "山景(南)", "Shan King (South)"},
    {"180", "山景(北)", "Shan King (North)"},
    {"170", "石排", "Shek Pai"},
    {"160", "新圍", "San Wai"},
    {"150", "良景", "Leung King"},
    {"140", "田景", "Tin King"},
    {"130", "建生", "Kin Sang"},
    {"120", "青松", "Ching Chung"},
    {"110", "麒麟", "Kei Lun"},
    {"100", "兆康", "Siu Hong"},
    {"200", "鳴琴", "Ming Kum"},
};
const TransitCatalogItem kLr505Directions[] = {
    {"100", "兆康", "Siu Hong"},
    {"920", "三聖", "Sam Shing"},
};

const TransitCatalogItem kLr507Stations[] = {
    {"1", "屯門碼頭", "Tuen Mun Ferry Pier"},
    {"240", "兆禧", "Siu Hei"},
    {"250", "海皇路", "Hoi Wong Road"},
    {"260", "豐景園", "Goodview Garden"},
    {"265", "兆麟", "Siu Lun"},
    {"270", "安定", "On Ting"},
    {"280", "市中心", "Town Centre"},
    {"295", "屯門", "Tuen Mun"},
    {"70", "河田", "Ho Tin"},
    {"75", "蔡意橋", "Choy Yee Bridge"},
    {"230", "銀圍", "Ngan Wai"},
    {"220", "大興(南)", "Tai Hing (South)"},
    {"212", "大興(北)", "Tai Hing (North)"},
    {"160", "新圍", "San Wai"},
    {"150", "良景", "Leung King"},
    {"140", "田景", "Tin King"},
};
const TransitCatalogItem kLr507Directions[] = {
    {"1", "屯門碼頭", "Tuen Mun Ferry Pier"},
    {"140", "田景", "Tin King"},
};

const TransitCatalogItem kLr610Stations[] = {
    {"1", "屯門碼頭", "Tuen Mun Ferry Pier"},
    {"10", "美樂", "Melody Garden"},
    {"15", "蝴蝶", "Butterfly"},
    {"20", "輕鐵車廠", "Light Rail Depot"},
    {"30", "龍門", "Lung Mun"},
    {"40", "青山村", "Tsing Shan Tsuen"},
    {"50", "青雲", "Tsing Wun"},
    {"200", "鳴琴", "Ming Kum"},
    {"170", "石排", "Shek Pai"},
    {"212", "大興(北)", "Tai Hing (North)"},
    {"220", "大興(南)", "Tai Hing (South)"},
    {"230", "銀圍", "Ngan Wai"},
    {"80", "澤豐", "Affluence"},
    {"90", "屯門醫院", "Tuen Mun Hospital"},
    {"100", "兆康", "Siu Hong"},
    {"350", "藍地", "Lam Tei"},
    {"360", "泥圍", "Nai Wai"},
    {"370", "鍾屋村", "Chung Uk Tsuen"},
    {"380", "洪水橋", "Hung Shui Kiu"},
    {"390", "塘坊村", "Tong Fong Tsuen"},
    {"400", "屏山", "Ping Shan"},
    {"560", "水邊圍", "Shui Pin Wai"},
    {"570", "豐年路", "Fung Nin Road"},
    {"580", "康樂路", "Hong Lok Road"},
    {"590", "大棠路", "Tai Tong Road"},
    {"600", "元朗", "Yuen Long"},
};
const TransitCatalogItem kLr610Directions[] = {
    {"1", "屯門碼頭", "Tuen Mun Ferry Pier"},
    {"600", "元朗", "Yuen Long"},
};

const TransitCatalogItem kLr614Stations[] = {
    {"1", "屯門碼頭", "Tuen Mun Ferry Pier"},
    {"240", "兆禧", "Siu Hei"},
    {"250", "海皇路", "Hoi Wong Road"},
    {"260", "豐景園", "Goodview Garden"},
    {"265", "兆麟", "Siu Lun"},
    {"270", "安定", "On Ting"},
    {"280", "市中心", "Town Centre"},
    {"300", "杯渡", "Pui To"},
    {"310", "何福堂", "Hoh Fuk Tong"},
    {"320", "新墟", "San Hui"},
    {"330", "景峰", "Prime View"},
    {"340", "鳳地", "Fung Tei"},
    {"100", "兆康", "Siu Hong"},
    {"350", "藍地", "Lam Tei"},
    {"360", "泥圍", "Nai Wai"},
    {"370", "鍾屋村", "Chung Uk Tsuen"},
    {"380", "洪水橋", "Hung Shui Kiu"},
    {"390", "塘坊村", "Tong Fong Tsuen"},
    {"400", "屏山", "Ping Shan"},
    {"560", "水邊圍", "Shui Pin Wai"},
    {"570", "豐年路", "Fung Nin Road"},
    {"580", "康樂路", "Hong Lok Road"},
    {"590", "大棠路", "Tai Tong Road"},
    {"600", "元朗", "Yuen Long"},
};
const TransitCatalogItem kLr614Directions[] = {
    {"1", "屯門碼頭", "Tuen Mun Ferry Pier"},
    {"600", "元朗", "Yuen Long"},
};

const TransitCatalogItem kLr614PStations[] = {
    {"1", "屯門碼頭", "Tuen Mun Ferry Pier"},
    {"240", "兆禧", "Siu Hei"},
    {"250", "海皇路", "Hoi Wong Road"},
    {"260", "豐景園", "Goodview Garden"},
    {"265", "兆麟", "Siu Lun"},
    {"270", "安定", "On Ting"},
    {"280", "市中心", "Town Centre"},
    {"300", "杯渡", "Pui To"},
    {"310", "何福堂", "Hoh Fuk Tong"},
    {"320", "新墟", "San Hui"},
    {"330", "景峰", "Prime View"},
    {"340", "鳳地", "Fung Tei"},
    {"100", "兆康", "Siu Hong"},
};
const TransitCatalogItem kLr614PDirections[] = {
    {"1", "屯門碼頭", "Tuen Mun Ferry Pier"},
    {"100", "兆康", "Siu Hong"},
};

const TransitCatalogItem kLr615Stations[] = {
    {"1", "屯門碼頭", "Tuen Mun Ferry Pier"},
    {"10", "美樂", "Melody Garden"},
    {"15", "蝴蝶", "Butterfly"},
    {"20", "輕鐵車廠", "Light Rail Depot"},
    {"30", "龍門", "Lung Mun"},
    {"40", "青山村", "Tsing Shan Tsuen"},
    {"50", "青雲", "Tsing Wun"},
    {"200", "鳴琴", "Ming Kum"},
    {"170", "石排", "Shek Pai"},
    {"160", "新圍", "San Wai"},
    {"150", "良景", "Leung King"},
    {"140", "田景", "Tin King"},
    {"130", "建生", "Kin Sang"},
    {"120", "青松", "Ching Chung"},
    {"100", "兆康", "Siu Hong"},
    {"350", "藍地", "Lam Tei"},
    {"360", "泥圍", "Nai Wai"},
    {"370", "鍾屋村", "Chung Uk Tsuen"},
    {"380", "洪水橋", "Hung Shui Kiu"},
    {"390", "塘坊村", "Tong Fong Tsuen"},
    {"400", "屏山", "Ping Shan"},
    {"560", "水邊圍", "Shui Pin Wai"},
    {"570", "豐年路", "Fung Nin Road"},
    {"580", "康樂路", "Hong Lok Road"},
    {"590", "大棠路", "Tai Tong Road"},
    {"600", "元朗", "Yuen Long"},
};
const TransitCatalogItem kLr615Directions[] = {
    {"1", "屯門碼頭", "Tuen Mun Ferry Pier"},
    {"600", "元朗", "Yuen Long"},
};

const TransitCatalogItem kLr615PStations[] = {
    {"100", "兆康", "Siu Hong"},
    {"110", "麒麟", "Kei Lun"},
    {"120", "青松", "Ching Chung"},
    {"130", "建生", "Kin Sang"},
    {"140", "田景", "Tin King"},
    {"150", "良景", "Leung King"},
    {"160", "新圍", "San Wai"},
    {"170", "石排", "Shek Pai"},
    {"200", "鳴琴", "Ming Kum"},
    {"50", "青雲", "Tsing Wun"},
    {"40", "青山村", "Tsing Shan Tsuen"},
    {"30", "龍門", "Lung Mun"},
    {"20", "輕鐵車廠", "Light Rail Depot"},
    {"15", "蝴蝶", "Butterfly"},
    {"10", "美樂", "Melody Garden"},
    {"1", "屯門碼頭", "Tuen Mun Ferry Pier"},
};
const TransitCatalogItem kLr615PDirections[] = {
    {"1", "屯門碼頭", "Tuen Mun Ferry Pier"},
    {"100", "兆康", "Siu Hong"},
};

const TransitCatalogItem kLr705Stations[] = {
    {"430", "天水圍", "Tin Shui Wai"},
    {"435", "天慈", "Tin Tsz"},
    {"450", "天湖", "Tin Wu"},
    {"455", "銀座", "Ginza"},
    {"500", "天榮", "Tin Wing"},
    {"510", "天悅", "Tin Yuet"},
    {"520", "天秀", "Tin Sau"},
    {"530", "濕地公園", "Wetland Park"},
    {"540", "天恒", "Tin Heng"},
    {"550", "天逸", "Tin Yat"},
    {"480", "天富", "Tin Fu"},
    {"468", "頌富", "Chung Fu"},
    {"460", "天瑞", "Tin Shui"},
    {"448", "樂湖", "Locwood"},
    {"445", "天耀", "Tin Yiu"},
};
const TransitCatalogItem kLr705Directions[] = {
    {"430", "天水圍", "Tin Shui Wai"},
};

const TransitCatalogItem kLr706Stations[] = {
    {"430", "天水圍", "Tin Shui Wai"},
    {"445", "天耀", "Tin Yiu"},
    {"448", "樂湖", "Locwood"},
    {"460", "天瑞", "Tin Shui"},
    {"468", "頌富", "Chung Fu"},
    {"480", "天富", "Tin Fu"},
    {"550", "天逸", "Tin Yat"},
    {"540", "天恒", "Tin Heng"},
    {"530", "濕地公園", "Wetland Park"},
    {"520", "天秀", "Tin Sau"},
    {"510", "天悅", "Tin Yuet"},
    {"500", "天榮", "Tin Wing"},
    {"455", "銀座", "Ginza"},
    {"450", "天湖", "Tin Wu"},
    {"435", "天慈", "Tin Tsz"},
};
const TransitCatalogItem kLr706Directions[] = {
    {"430", "天水圍", "Tin Shui Wai"},
};

const TransitCatalogItem kLr751Stations[] = {
    {"275", "友愛", "Yau Oi"},
    {"270", "安定", "On Ting"},
    {"280", "市中心", "Town Centre"},
    {"295", "屯門", "Tuen Mun"},
    {"70", "河田", "Ho Tin"},
    {"75", "蔡意橋", "Choy Yee Bridge"},
    {"80", "澤豐", "Affluence"},
    {"90", "屯門醫院", "Tuen Mun Hospital"},
    {"100", "兆康", "Siu Hong"},
    {"350", "藍地", "Lam Tei"},
    {"360", "泥圍", "Nai Wai"},
    {"370", "鍾屋村", "Chung Uk Tsuen"},
    {"380", "洪水橋", "Hung Shui Kiu"},
    {"425", "坑尾村", "Hang Mei Tsuen"},
    {"430", "天水圍", "Tin Shui Wai"},
    {"435", "天慈", "Tin Tsz"},
    {"450", "天湖", "Tin Wu"},
    {"455", "銀座", "Ginza"},
    {"500", "天榮", "Tin Wing"},
    {"490", "翠湖", "Chestwood"},
    {"468", "頌富", "Chung Fu"},
    {"480", "天富", "Tin Fu"},
    {"550", "天逸", "Tin Yat"},
};
const TransitCatalogItem kLr751Directions[] = {
    {"275", "友愛", "Yau Oi"},
    {"550", "天逸", "Tin Yat"},
};

const TransitCatalogItem kLr761PStations[] = {
    {"550", "天逸", "Tin Yat"},
    {"480", "天富", "Tin Fu"},
    {"468", "頌富", "Chung Fu"},
    {"460", "天瑞", "Tin Shui"},
    {"448", "樂湖", "Locwood"},
    {"445", "天耀", "Tin Yiu"},
    {"425", "坑尾村", "Hang Mei Tsuen"},
    {"390", "塘坊村", "Tong Fong Tsuen"},
    {"400", "屏山", "Ping Shan"},
    {"560", "水邊圍", "Shui Pin Wai"},
    {"570", "豐年路", "Fung Nin Road"},
    {"580", "康樂路", "Hong Lok Road"},
    {"590", "大棠路", "Tai Tong Road"},
    {"600", "元朗", "Yuen Long"},
};
const TransitCatalogItem kLr761PDirections[] = {
    {"550", "天逸", "Tin Yat"},
    {"600", "元朗", "Yuen Long"},
};

const TransitCatalogGroup kLightRailGroups[] = {
    {"505", "505", kLr505Stations, itemCount(kLr505Stations), kLr505Directions, itemCount(kLr505Directions), "505"},
    {"507", "507", kLr507Stations, itemCount(kLr507Stations), kLr507Directions, itemCount(kLr507Directions), "507"},
    {"610", "610", kLr610Stations, itemCount(kLr610Stations), kLr610Directions, itemCount(kLr610Directions), "610"},
    {"614", "614", kLr614Stations, itemCount(kLr614Stations), kLr614Directions, itemCount(kLr614Directions), "614"},
    {"614P", "614P", kLr614PStations, itemCount(kLr614PStations), kLr614PDirections, itemCount(kLr614PDirections), "614P"},
    {"615", "615", kLr615Stations, itemCount(kLr615Stations), kLr615Directions, itemCount(kLr615Directions), "615"},
    {"615P", "615P", kLr615PStations, itemCount(kLr615PStations), kLr615PDirections, itemCount(kLr615PDirections), "615P"},
    {"705", "705", kLr705Stations, itemCount(kLr705Stations), kLr705Directions, itemCount(kLr705Directions), "705"},
    {"706", "706", kLr706Stations, itemCount(kLr706Stations), kLr706Directions, itemCount(kLr706Directions), "706"},
    {"751", "751", kLr751Stations, itemCount(kLr751Stations), kLr751Directions, itemCount(kLr751Directions), "751"},
    {"761P", "761P", kLr761PStations, itemCount(kLr761PStations), kLr761PDirections, itemCount(kLr761PDirections), "761P"},
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
