#include "core/UiText.h"

#include <array>
#include <string_view>
#include <utility>

namespace transitink {
namespace {

UiLocale activeLocale = UiLocale::ZhHk;

using TextTable = std::array<const char*, static_cast<std::size_t>(UiTextId::Count)>;

constexpr TextTable kZhHk = {
    "設定不完整",
    "暫無班次",
    "時間尚未同步",
    "暫未能取得行車時間",
    "未能更新行車時間",
    "暫未能取得資料",
    "資料已逾期",
    " 分鐘",
    "即將到站",
    "到站預報暫停",
    "交通擠塞",
    "行車緩慢",
    "交通暢順",
    "隧道封閉",
    "電量：未能讀取",
    "電量：",
    "（已充滿）",
    "（充電中）",
    "（外接電源）",
    "星期日",
    "星期一",
    "星期二",
    "星期三",
    "星期四",
    "星期五",
    "星期六",
    "尚未設定小工具",
    "按 Volume Up 開啟設定頁",
    "啟動中",
    "設定 ",
    "網絡：",
    "版本：",
    "完成後按「儲存並重啟」",
    "連線狀態",
    "Wi-Fi 未連接",
    "載入中",
    "密碼：",
    "請先用手機連接以上 Wi-Fi",
    "開啟 ",
    "本機設定頁",
    "已重設裝置",
    "放開音量鍵後重啟",
    "天氣暫無資料",
    "晴",
    "間中有陽光",
    "有驟雨",
    "多雲",
    "密雲",
    "微雨",
    "雨",
    "大雨",
    "雷暴",
    "霧",
    "雪",
    "有驟雪",
    "天氣",
};

constexpr TextTable kEnGb = {
    "Incomplete setup",
    "No arrivals",
    "Time not synced",
    "Journey time unavailable",
    "Could not update journey time",
    "Data unavailable",
    "Data out of date",
    " min",
    "Arriving",
    "ETA suspended",
    "Congested",
    "Slow traffic",
    "Traffic clear",
    "Tunnel closed",
    "Battery: unavailable",
    "Battery: ",
    " (full)",
    " (charging)",
    " (external power)",
    "Sun",
    "Mon",
    "Tue",
    "Wed",
    "Thu",
    "Fri",
    "Sat",
    "No widgets configured",
    "Press Volume Up for settings",
    "Starting",
    "Set up ",
    "Network: ",
    "Version: ",
    "Select Save and restart when done",
    "Connection status",
    "Wi-Fi disconnected",
    "Loading...",
    "Password: ",
    "Connect your phone to the Wi-Fi above",
    "Open ",
    "Local settings",
    "Device reset",
    "Release Volume to restart",
    "Weather unavailable",
    "Sunny",
    "Sunny periods",
    "Showers",
    "Cloudy",
    "Overcast",
    "Light rain",
    "Rain",
    "Heavy rain",
    "Thunderstorms",
    "Fog",
    "Snow",
    "Snow showers",
    "Weather",
};

static_assert(kZhHk.size() == static_cast<std::size_t>(UiTextId::Count));
static_assert(kEnGb.size() == static_cast<std::size_t>(UiTextId::Count));

const TextTable& tableFor(UiLocale locale) {
    return locale == UiLocale::EnGb ? kEnGb : kZhHk;
}

bool isAsciiUpper(char character) {
    return character >= 'A' && character <= 'Z';
}

bool isAsciiLower(char character) {
    return character >= 'a' && character <= 'z';
}

bool isEnglishDisplayAcronym(std::string_view word) {
    constexpr std::array<std::string_view, 25> acronyms = {
        "MTR", "KMB",  "LWB", "DLR",  "BBI", "HK",  "NT",
        "HKU", "CUHK", "HKUST", "HKIA", "IVE", "AWE",
        "HIT", "KCC",  "ICC", "TST",  "ETA", "NCR",
        "I",   "II",   "III", "IV",   "V",   "VI",
    };
    for (const std::string_view acronym : acronyms) {
        if (word == acronym) {
            return true;
        }
    }
    return false;
}

}  // namespace

const char* uiLocaleId(UiLocale locale) {
    return locale == UiLocale::EnGb ? "en-GB" : "zh-HK";
}

bool parseUiLocaleId(const std::string& value, UiLocale& locale) {
    if (value.empty() || value == "zh-HK") {
        locale = UiLocale::ZhHk;
        return true;
    }
    if (value == "en-GB") {
        locale = UiLocale::EnGb;
        return true;
    }
    return false;
}

bool isUiLocaleSupported(UiLocale locale) {
    return locale == UiLocale::ZhHk || locale == UiLocale::EnGb;
}

void setUiLocale(UiLocale locale) {
    activeLocale = locale;
}

UiLocale currentUiLocale() {
    return activeLocale;
}

const char* uiText(UiTextId id) {
    return uiText(activeLocale, id);
}

const char* uiText(UiLocale locale, UiTextId id) {
    const std::size_t index = static_cast<std::size_t>(id);
    if (index >= static_cast<std::size_t>(UiTextId::Count)) {
        return "";
    }
    return tableFor(locale)[index];
}

const char* weekdayText(int weekday) {
    if (weekday < 0 || weekday > 6) {
        return "";
    }
    return uiText(static_cast<UiTextId>(
        static_cast<uint8_t>(UiTextId::WeekdaySunday) + static_cast<uint8_t>(weekday)));
}

std::string englishDisplayLabel(std::string value) {
    bool hasUpper = false;
    for (const char character : value) {
        hasUpper = hasUpper || isAsciiUpper(character);
        if (isAsciiLower(character)) {
            return value;
        }
    }
    if (!hasUpper) {
        return value;
    }

    for (std::size_t offset = 0; offset < value.size();) {
        if (!isAsciiUpper(value[offset])) {
            ++offset;
            continue;
        }
        const std::size_t wordStart = offset;
        while (offset < value.size() && isAsciiUpper(value[offset])) {
            ++offset;
        }
        const std::string_view word(value.data() + wordStart, offset - wordStart);
        if (isEnglishDisplayAcronym(word)) {
            continue;
        }
        const bool capitalize =
            wordStart == 0 || value[wordStart - 1] != '\'';
        value[wordStart] =
            capitalize
                ? value[wordStart]
                : static_cast<char>(value[wordStart] - 'A' + 'a');
        for (std::size_t index = wordStart + 1; index < offset; ++index) {
            value[index] = static_cast<char>(value[index] - 'A' + 'a');
        }
    }
    return value;
}

std::string localizedText(const std::string& traditionalChinese,
                          const std::string& english) {
    if (currentUiLocale() == UiLocale::EnGb) {
        return english.empty() ? traditionalChinese : english;
    }
    return traditionalChinese.empty() ? english : traditionalChinese;
}

std::string localizedDisplayLabel(const std::string& traditionalChinese,
                                  const std::string& english) {
    std::string value = localizedText(traditionalChinese, english);
    return currentUiLocale() == UiLocale::EnGb
               ? englishDisplayLabel(std::move(value))
               : value;
}

}  // namespace transitink
