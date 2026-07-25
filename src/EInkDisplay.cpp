#include "EInkDisplay.h"

#include <Adafruit_GFX.h>
#include <WiFi.h>
#include <qrcode.h>

#include <algorithm>
#include <cstring>

#include "BatteryMonitor.h"
#include "HkGlyphFont.h"
#include "ProductConfig.h"
#include "core/DisplayTextCore.h"
#include "core/UiText.h"
#include "hardware/BoardProfile.h"
#include "hardware/SelectedDisplayDriver.h"

namespace {

constexpr uint16_t kColorBlack = 0;
constexpr uint16_t kColorWhite = 1;
constexpr int EINK_WIDTH = transitink::hardware::kBoardProfile.display.width;
constexpr int EINK_HEIGHT = transitink::hardware::kBoardProfile.display.height;
constexpr size_t kBytesPerRow = EINK_WIDTH / 8;
constexpr size_t kFrameBufferSize = kBytesPerRow * EINK_HEIGHT;
static_assert(EINK_WIDTH == 400 && EINK_HEIGHT == 300,
              "the current dashboard renderer requires a 400x300 display");
constexpr int kCustomGlyphWidth = 16;
constexpr int kCustomGlyphHeight = 16;
constexpr int kCustomGlyphBaselineOffset = 14;
constexpr int kMaxPartialRefreshes = 8;
constexpr int kLaneTextX = 12;
constexpr int kLaneBaseTextWidth = 204;
constexpr int kLaneValueAreaX = 224;
constexpr int kLaneValueWidth = 76;
constexpr int kLaneValueRightSpacing = 88;
constexpr int kLaneRightEdge = EINK_WIDTH - 12;
constexpr int kLaneColumnGap = 12;

struct DisplayRegion {
    int x;
    int y;
    int w;
    int h;
};

static_assert(transitink::kWidgetsPerPage == 4,
              "dashboard requires exactly four widgets per page");

constexpr bool regionFitsPanel(const DisplayRegion& region) {
    return region.x >= 0 && region.y >= 0 && region.w > 0 && region.h > 0 &&
           region.x + region.w <= EINK_WIDTH && region.y + region.h <= EINK_HEIGHT;
}

constexpr DisplayRegion kStatusRegion{0, 0, EINK_WIDTH, 42};
constexpr DisplayRegion kLaneRegions[transitink::kWidgetsPerPage] = {
    {0, 42, EINK_WIDTH, 57},
    {0, 99, EINK_WIDTH, 57},
    {0, 156, EINK_WIDTH, 57},
    {0, 213, EINK_WIDTH, 57},
};
constexpr DisplayRegion kFooterRegion{0, 270, EINK_WIDTH, 30};
static_assert(regionFitsPanel(kStatusRegion), "status region bounds");
static_assert(regionFitsPanel(kLaneRegions[0]), "lane 0 region bounds");
static_assert(regionFitsPanel(kLaneRegions[1]), "lane 1 region bounds");
static_assert(regionFitsPanel(kLaneRegions[2]), "lane 2 region bounds");
static_assert(regionFitsPanel(kLaneRegions[3]), "lane 3 region bounds");
static_assert(regionFitsPanel(kFooterRegion), "footer region bounds");
static_assert(kStatusRegion.y + kStatusRegion.h == kLaneRegions[0].y, "status/lane boundary");
static_assert(kLaneRegions[0].y + kLaneRegions[0].h == kLaneRegions[1].y, "lane 0/1 boundary");
static_assert(kLaneRegions[1].y + kLaneRegions[1].h == kLaneRegions[2].y, "lane 1/2 boundary");
static_assert(kLaneRegions[2].y + kLaneRegions[2].h == kLaneRegions[3].y, "lane 2/3 boundary");
static_assert(kLaneRegions[3].y + kLaneRegions[3].h == kFooterRegion.y, "lane/footer boundary");
static_assert(kFooterRegion.y + kFooterRegion.h == EINK_HEIGHT, "dashboard height");

const DisplayRegion& laneRegion(uint8_t slot) {
    return kLaneRegions[slot];
}

constexpr float kForceFullPartialDiffRatio = 0.30f;

uint8_t frameBuffer[kFrameBufferSize];
uint8_t previousFrameBuffer[kFrameBufferSize];
BatteryMonitor batteryMonitor;
bool previousFrameValid = false;
bool dashboardFrameActive = false;
int partialRefreshCount = 0;

struct PartialDiffStats {
    uint32_t changedBits = 0;
    uint32_t totalBits = 0;

    float ratio() const {
        return totalBits == 0 ? 0.0f : static_cast<float>(changedBits) / static_cast<float>(totalBits);
    }
};

class MonoCanvas : public Adafruit_GFX {
public:
    MonoCanvas() : Adafruit_GFX(EINK_WIDTH, EINK_HEIGHT) {}

    void clear() {
        std::memset(frameBuffer, 0xFF, sizeof(frameBuffer));
    }

    void drawPixel(int16_t x, int16_t y, uint16_t color) override {
        if (x < 0 || y < 0 || x >= EINK_WIDTH || y >= EINK_HEIGHT) {
            return;
        }
        size_t index = static_cast<size_t>(y) * kBytesPerRow + static_cast<size_t>(x / 8);
        uint8_t mask = static_cast<uint8_t>(0x80 >> (x & 7));
        if (color == kColorBlack) {
            frameBuffer[index] &= static_cast<uint8_t>(~mask);
        } else {
            frameBuffer[index] |= mask;
        }
    }
};

MonoCanvas canvas;
transitink::hardware::SelectedDisplayDriver panel;

bool isUtf8Continuation(uint8_t value) {
    return (value & 0xC0) == 0x80;
}

uint16_t decodeUtf8Codepoint(const char* text, size_t length, size_t& offset) {
    uint8_t first = static_cast<uint8_t>(text[offset]);
    if (first < 0x80) {
        ++offset;
        return first;
    }
    if ((first & 0xE0) == 0xC0 && offset + 1 < length) {
        uint8_t second = static_cast<uint8_t>(text[offset + 1]);
        if (isUtf8Continuation(second)) {
            offset += 2;
            return static_cast<uint16_t>(((first & 0x1F) << 6) | (second & 0x3F));
        }
    }
    if ((first & 0xF0) == 0xE0 && offset + 2 < length) {
        uint8_t second = static_cast<uint8_t>(text[offset + 1]);
        uint8_t third = static_cast<uint8_t>(text[offset + 2]);
        if (isUtf8Continuation(second) && isUtf8Continuation(third)) {
            offset += 3;
            return static_cast<uint16_t>(((first & 0x0F) << 12) | ((second & 0x3F) << 6) |
                                         (third & 0x3F));
        }
    }
    ++offset;
    return '?';
}

uint16_t drawCustomGlyph(int x, int y, uint16_t codepoint) {
    const HkGlyph* glyph = findHkGlyph(codepoint);
    if (glyph == nullptr) {
        return 0;
    }
    int top = y - kCustomGlyphBaselineOffset;
    for (int row = 0; row < kCustomGlyphHeight; ++row) {
        uint16_t bits = glyph->rows[row];
        for (int col = 0; col < glyph->width; ++col) {
            if ((bits & (0x8000 >> col)) != 0) {
                canvas.drawPixel(x + col, top + row, kColorBlack);
            }
        }
    }
    return glyph->width;
}

int displayCodepointWidth(uint32_t codepoint, void*) {
    if (codepoint > 0xFFFFU || codepoint == '\r' || codepoint == '\n') {
        return 0;
    }
    const uint16_t bmpCodepoint = static_cast<uint16_t>(codepoint);
    const HkGlyph* customGlyph = findHkGlyph(bmpCodepoint);
    if (customGlyph != nullptr) {
        return customGlyph->width;
    }
    return 0;
}

int measureTextWidth(const String& text) {
    int width = 0;
    const char* raw = text.c_str();
    const size_t length = text.length();
    for (size_t offset = 0; offset < length;) {
        const uint16_t codepoint = decodeUtf8Codepoint(raw, length, offset);
        if (codepoint == '\r' || codepoint == '\n') {
            continue;
        }
        const HkGlyph* customGlyph = findHkGlyph(codepoint);
        if (customGlyph != nullptr) {
            width += customGlyph->width;
        }
    }
    return width;
}

void drawText(int x, int y, const String& text) {
    int cursorX = x;
    const char* raw = text.c_str();
    size_t length = text.length();
    for (size_t offset = 0; offset < length;) {
        uint16_t codepoint = decodeUtf8Codepoint(raw, length, offset);
        if (codepoint == '\r' || codepoint == '\n') {
            continue;
        }
        uint16_t width = drawCustomGlyph(cursorX, y, codepoint);
        if (width > 0) {
            cursorX += width;
        }
    }
}

transitink::DisplayTextPlan displayTextPlan(const String& text, int maxWidth) {
    transitink::DisplayTextPlan plan;
    if (maxWidth <= 0) {
        return plan;
    }
    std::string source;
    source.reserve(text.length());
    source.assign(text.c_str(), text.length());
    return transitink::planTruncatedUtf8(
        source, maxWidth, displayCodepointWidth, nullptr);
}

void drawTruncatedText(int x, int y, const String& text, int maxWidth) {
    const transitink::DisplayTextPlan plan = displayTextPlan(text, maxWidth);
    if (!plan.text.empty()) {
        drawText(x, y, String(plan.text.c_str()));
    }
}

void drawRightAlignedTruncatedText(int rightX, int y, const String& text,
                                   int maxWidth) {
    const transitink::DisplayTextPlan plan = displayTextPlan(text, maxWidth);
    if (!plan.text.empty()) {
        drawText(rightX - plan.pixelWidth, y, String(plan.text.c_str()));
    }
}

String compactLaneTitle(const String& title, int maxWidth) {
    if (transitink::currentUiLocale() != transitink::UiLocale::EnGb ||
        measureTextWidth(title) <= maxWidth) {
        return title;
    }
    const std::string source(title.c_str(), title.length());
    const std::string compact =
        transitink::withoutTrailingParentheticalQualifier(source);
    return compact == source ? title : String(compact.c_str());
}

int adaptiveLaneTextWidth(const String& firstValue, int firstValueRight) {
    const transitink::DisplayTextPlan plan =
        displayTextPlan(firstValue, kLaneValueWidth);
    if (plan.text.empty()) {
        return kLaneBaseTextWidth;
    }
    const int available =
        firstValueRight - plan.pixelWidth - kLaneColumnGap - kLaneTextX;
    return std::max(kLaneBaseTextWidth, available);
}

void drawWifiIcon(int x, int y, bool connected) {
    for (int i = 0; i < 3; ++i) {
        int height = 4 + i * 4;
        int barX = x + i * 6;
        int barY = y + 14 - height;
        canvas.drawRect(barX, barY, 4, height, kColorBlack);
        if (connected) {
            canvas.fillRect(barX + 1, barY + 1, 2, height - 2, kColorBlack);
        }
    }
    if (!connected) {
        canvas.drawLine(x, y + 14, x + 18, y, kColorBlack);
    }
}

void drawChargingBolt(int x, int y) {
    canvas.drawLine(x + 5, y, x + 1, y + 7, kColorBlack);
    canvas.drawLine(x + 1, y + 7, x + 6, y + 7, kColorBlack);
    canvas.drawLine(x + 6, y + 7, x + 3, y + 14, kColorBlack);
}

void drawBatteryIcon(int x, int y, const bus_eta::BatterySnapshot& status) {
    canvas.drawRect(x, y + 3, 24, 12, kColorBlack);
    canvas.fillRect(x + 24, y + 7, 3, 4, kColorBlack);
    if (status.valid) {
        const int fillWidth = (status.percent * 18 + 99) / 100;
        if (fillWidth > 0) {
            canvas.fillRect(x + 3, y + 6, fillWidth, 6, kColorBlack);
        }
    } else {
        canvas.drawLine(x + 3, y + 14, x + 21, y + 4, kColorBlack);
    }
    if (status.full) {
        canvas.fillRect(x + 3, y + 6, 18, 6, kColorBlack);
    }
    if (status.charging) {
        drawChargingBolt(x - 10, y + 3);
    }
}

void drawStatusBar() {
    bus_eta::BatterySnapshot status = batteryMonitor.read();
    drawWifiIcon(EINK_WIDTH - 72, 8, WiFi.status() == WL_CONNECTED);
    drawBatteryIcon(EINK_WIDTH - 36, 7, status);
}

String batteryStatusText() {
    bus_eta::BatterySnapshot status = batteryMonitor.read();
    if (!status.valid) {
        return transitink::uiText(transitink::UiTextId::BatteryUnavailable);
    }
    String text =
        String(transitink::uiText(transitink::UiTextId::BatteryLabel)) +
        String(status.percent) + "%";
    if (status.full) {
        return text + transitink::uiText(transitink::UiTextId::BatteryFull);
    }
    if (status.charging) {
        return text +
               transitink::uiText(transitink::UiTextId::BatteryCharging);
    }
    if (status.powerPresent) {
        return text +
               transitink::uiText(transitink::UiTextId::BatteryExternalPower);
    }
    return text;
}

void drawMultilineText(int x, int y, const String& text, int lineHeight = 28) {
    int start = 0;
    int lineY = y;
    while (start <= text.length()) {
        int newline = text.indexOf('\n', start);
        String line = newline >= 0 ? text.substring(start, newline) : text.substring(start);
        drawText(x, lineY, line);
        if (newline < 0) {
            break;
        }
        start = newline + 1;
        lineY += lineHeight;
    }
}

void drawQrCode(int x, int y, const String& text) {
    if (text.isEmpty()) {
        return;
    }
    constexpr uint8_t kQrVersion = 3;
    constexpr uint8_t kQrScale = 4;
    QRCode qrcode;
    uint8_t qrcodeData[qrcode_getBufferSize(kQrVersion)];
    qrcode_initText(&qrcode, qrcodeData, kQrVersion, ECC_LOW, text.c_str());
    const int quiet = kQrScale * 2;
    const int size = qrcode.size * kQrScale + quiet * 2;
    canvas.fillRect(x, y, size, size, kColorWhite);
    canvas.drawRect(x, y, size, size, kColorBlack);
    for (uint8_t row = 0; row < qrcode.size; ++row) {
        for (uint8_t col = 0; col < qrcode.size; ++col) {
            if (qrcode_getModule(&qrcode, col, row)) {
                canvas.fillRect(x + quiet + col * kQrScale, y + quiet + row * kQrScale, kQrScale, kQrScale, kColorBlack);
            }
        }
    }
}

String currentClockText() {
    struct tm tmInfo;
    const time_t now = time(nullptr);
    localtime_r(&now, &tmInfo);
    char buffer[32];
    if (transitink::currentUiLocale() == transitink::UiLocale::EnGb) {
        snprintf(buffer, sizeof(buffer), "%s %02d/%02d %02d:%02d",
                 transitink::weekdayText(tmInfo.tm_wday), tmInfo.tm_mday,
                 tmInfo.tm_mon + 1, tmInfo.tm_hour, tmInfo.tm_min);
    } else {
        snprintf(buffer, sizeof(buffer), "%02d/%02d %s %02d:%02d",
                 tmInfo.tm_mon + 1, tmInfo.tm_mday,
                 transitink::weekdayText(tmInfo.tm_wday), tmInfo.tm_hour,
                 tmInfo.tm_min);
    }
    return String(buffer);
}

void drawClockAndStatusBar(uint8_t pageIndex = 0, uint8_t pageCount = 1) {
    drawStatusBar();
    drawText(12, 24, currentClockText());
    if (pageCount > 1) {
        drawText(286, 24,
                 String(static_cast<unsigned int>(pageIndex) + 1) + "/" +
                     String(pageCount));
    }
}

void markNonDashboardFrame() {
    dashboardFrameActive = false;
}

void clearRegion(const DisplayRegion& region) {
    canvas.fillRect(region.x, region.y, region.w, region.h, kColorWhite);
}

void drawLaneDivider(const DisplayRegion& region, bool visible) {
    if (visible) {
        canvas.drawFastHLine(region.x, region.y + region.h - 1, region.w, kColorBlack);
    }
}

bool shouldDrawLaneDivider(const transitink::WidgetPageSnapshotSet& snapshots, uint8_t slot) {
    if (slot >= transitink::kWidgetsPerPage ||
        snapshots[slot].type == transitink::WidgetType::Disabled) {
        return false;
    }
    for (uint8_t index = slot + 1; index < transitink::kWidgetsPerPage; ++index) {
        if (snapshots[index].type != transitink::WidgetType::Disabled) {
            return true;
        }
    }
    return false;
}

bool hasEnabledWidget(const transitink::WidgetPageSnapshotSet& snapshots) {
    return std::any_of(
        snapshots.begin(), snapshots.end(), [](const transitink::WidgetSnapshot& snapshot) {
            return snapshot.type != transitink::WidgetType::Disabled;
        });
}

void drawNoWidgetsHint() {
    const String title =
        transitink::uiText(transitink::UiTextId::NoWidgets);
    const String action =
        transitink::uiText(transitink::UiTextId::OpenSettings);
    drawText(std::max(12, (EINK_WIDTH - measureTextWidth(title)) / 2), 138, title);
    drawText(std::max(12, (EINK_WIDTH - measureTextWidth(action)) / 2), 174, action);
}

void drawWidgetLane(uint8_t slot, const transitink::WidgetSnapshot& snapshot, bool sleeping, bool drawDivider) {
    const DisplayRegion& region = laneRegion(slot);
    if (snapshot.type == transitink::WidgetType::Disabled) {
        return;
    }

    const std::size_t valueLimit = snapshot.type == transitink::WidgetType::JourneyTime ? 1U : 2U;
    const int firstValueRight =
        valueLimit == 1U ? kLaneRightEdge
                         : kLaneRightEdge - kLaneValueRightSpacing;
    const int valueY = region.y + 20;
    const int contextY = region.y + 42;
    int titleWidth = kLaneBaseTextWidth;
    int subtitleWidth = kLaneBaseTextWidth;

    if (sleeping) {
        titleWidth = adaptiveLaneTextWidth("-", firstValueRight);
        subtitleWidth = titleWidth;
    } else if (snapshot.valueCount > 0) {
        titleWidth = adaptiveLaneTextWidth(
            String(snapshot.values[0].text.c_str()), firstValueRight);
        subtitleWidth = titleWidth;
        if (snapshot.freshness == transitink::Freshness::Stale ||
            snapshot.type == transitink::WidgetType::JourneyTime) {
            subtitleWidth = kLaneBaseTextWidth;
        }
    }

    const String title =
        compactLaneTitle(String(snapshot.title.c_str()), titleWidth);
    drawTruncatedText(kLaneTextX, valueY, title, titleWidth);
    drawTruncatedText(
        kLaneTextX, contextY, String(snapshot.subtitle.c_str()), subtitleWidth);

    if (sleeping) {
        for (std::size_t valueIndex = 0; valueIndex < valueLimit; ++valueIndex) {
            const int valueRight =
                firstValueRight +
                static_cast<int>(valueIndex) * kLaneValueRightSpacing;
            drawRightAlignedTruncatedText(
                valueRight, valueY, "-", kLaneValueWidth);
        }
        drawLaneDivider(region, drawDivider);
        return;
    }

    if (snapshot.freshness == transitink::Freshness::Stale && snapshot.valueCount == 0) {
        drawTruncatedText(
            kLaneValueAreaX, valueY,
            transitink::uiText(transitink::UiTextId::DataUnavailable), 164);
        drawLaneDivider(region, drawDivider);
        return;
    }
    if (snapshot.freshness == transitink::Freshness::Fresh &&
        (snapshot.state == transitink::WidgetState::Empty || snapshot.state == transitink::WidgetState::Error)) {
        const String message = snapshot.providerMessage.empty() && snapshot.fetchedAtEpoch == 0
                                   ? String("...")
                                   : (snapshot.providerMessage.empty()
                                          ? String(transitink::uiText(
                                                transitink::UiTextId::DataUnavailable))
                                          : String(snapshot.providerMessage.c_str()));
        drawTruncatedText(kLaneValueAreaX, valueY, message, 164);
        drawLaneDivider(region, drawDivider);
        return;
    }

    const std::size_t shownValueCount = std::min(snapshot.valueCount, valueLimit);
    for (std::size_t valueIndex = 0; valueIndex < shownValueCount; ++valueIndex) {
        const int valueRight =
            firstValueRight +
            static_cast<int>(valueIndex) * kLaneValueRightSpacing;
        drawRightAlignedTruncatedText(
            valueRight, valueY,
            String(snapshot.values[valueIndex].text.c_str()), kLaneValueWidth);
        if (snapshot.type == transitink::WidgetType::JourneyTime &&
            snapshot.freshness == transitink::Freshness::Fresh) {
            drawTruncatedText(
                kLaneValueAreaX, contextY,
                String(snapshot.values[valueIndex].context.c_str()),
                kLaneRightEdge - kLaneValueAreaX);
        }
    }
    if (snapshot.freshness == transitink::Freshness::Stale) {
        drawTruncatedText(
            kLaneValueAreaX, contextY,
            transitink::uiText(transitink::UiTextId::DataExpired), 164);
    }
    drawLaneDivider(region, drawDivider);
}

void drawWeatherFooter(const WeatherSnapshot& weather) {
    drawTruncatedText(12, 291, weatherDisplayText(weather), 376);
}

void copyPartialRegionToPrevious(int x, int y, int w, int h) {
    const int xEnd = std::min(EINK_WIDTH, x + w);
    const int yEnd = std::min(EINK_HEIGHT, y + h);
    const int alignedX = (std::max(0, x) / 8) * 8;
    const int alignedEnd = std::min(EINK_WIDTH, ((xEnd + 7) / 8) * 8);
    const int startY = std::max(0, y);
    if (alignedEnd <= alignedX || yEnd <= startY) {
        return;
    }
    const size_t startByte = static_cast<size_t>(alignedX / 8);
    const size_t byteCount = static_cast<size_t>((alignedEnd - alignedX) / 8);
    for (int row = startY; row < yEnd; ++row) {
        const size_t offset = static_cast<size_t>(row) * kBytesPerRow + startByte;
        std::memcpy(previousFrameBuffer + offset, frameBuffer + offset, byteCount);
    }
}

PartialDiffStats partialDiffStats(int x, int y, int w, int h) {
    PartialDiffStats stats;
    const int xEnd = std::min(EINK_WIDTH, x + w);
    const int yEnd = std::min(EINK_HEIGHT, y + h);
    const int alignedX = (std::max(0, x) / 8) * 8;
    const int alignedEnd = std::min(EINK_WIDTH, ((xEnd + 7) / 8) * 8);
    const int startY = std::max(0, y);
    if (alignedEnd <= alignedX || yEnd <= startY) {
        return stats;
    }
    const size_t startByte = static_cast<size_t>(alignedX / 8);
    const size_t byteCount = static_cast<size_t>((alignedEnd - alignedX) / 8);
    stats.totalBits = static_cast<uint32_t>(byteCount * 8 * static_cast<size_t>(yEnd - startY));
    for (int row = startY; row < yEnd; ++row) {
        const size_t offset = static_cast<size_t>(row) * kBytesPerRow + startByte;
        for (size_t xb = 0; xb < byteCount; ++xb) {
            const uint8_t changed = previousFrameBuffer[offset + xb] ^ frameBuffer[offset + xb];
            stats.changedBits += static_cast<uint32_t>(__builtin_popcount(static_cast<unsigned int>(changed)));
        }
    }
    return stats;
}

void flushCanvas() {
    panel.show(frameBuffer);
    std::memcpy(previousFrameBuffer, frameBuffer, sizeof(frameBuffer));
    previousFrameValid = true;
    partialRefreshCount = 0;
}

void refreshCanvasPartially(int x, int y, int w, int h) {
    if (!previousFrameValid || partialRefreshCount >= kMaxPartialRefreshes) {
        flushCanvas();
        return;
    }
    const PartialDiffStats stats = partialDiffStats(x, y, w, h);
    if (stats.changedBits == 0) {
        Serial.println("EPD partial skipped: unchanged");
        return;
    }
    if (stats.ratio() >= kForceFullPartialDiffRatio) {
        Serial.println("EPD partial promoted to full: large diff");
        flushCanvas();
        return;
    }
    panel.showPartialRegion(frameBuffer, previousFrameBuffer, x, y, w, h);
    copyPartialRegionToPrevious(x, y, w, h);
    ++partialRefreshCount;
}

}  // namespace

void EInkDisplay::begin(bool showBootScreen) {
    Serial.println("EInkDisplay begin");
    batteryMonitor.begin();
    canvas.clear();
    panel.begin();
    if (showBootScreen) {
        showBoot(transitink::uiText(transitink::UiTextId::Booting));
    }
}

void EInkDisplay::fullRefresh() {
    flushCanvas();
}

void EInkDisplay::partialRefresh(int x, int y, int w, int h) {
    refreshCanvasPartially(x, y, w, h);
}

void EInkDisplay::showBoot(const String& message) {
    canvas.clear();
    drawStatusBar();
    drawText(18, 42, FIRMWARE_PRODUCT_NAME);
    drawText(18, 78, message);
    markNonDashboardFrame();
    fullRefresh();
}

void EInkDisplay::showConfigMode(const String& networkName, const String& details, const String& qrUrl) {
    canvas.clear();
    drawStatusBar();
    drawText(18, 38,
             String(transitink::uiText(transitink::UiTextId::SettingsPrefix)) +
                 FIRMWARE_PRODUCT_NAME);
    drawText(18, 76,
             String(transitink::uiText(transitink::UiTextId::NetworkLabel)) +
                 networkName);
    drawText(18, 100, batteryStatusText());
    drawText(18, 124,
             String(transitink::uiText(transitink::UiTextId::VersionLabel)) +
                 FIRMWARE_VERSION);
    drawMultilineText(18, 150, details, 22);
    drawQrCode(258, 92, qrUrl);
    drawText(18, 260,
             transitink::uiText(transitink::UiTextId::SaveAndRestart));
    markNonDashboardFrame();
    fullRefresh();
}

void EInkDisplay::showWifiStatus(const String& message) {
    canvas.clear();
    drawStatusBar();
    drawText(18, 42,
             transitink::uiText(transitink::UiTextId::ConnectionStatus));
    drawMultilineText(18, 82, message);
    markNonDashboardFrame();
    fullRefresh();
}

void EInkDisplay::showDashboard(const transitink::WidgetPageSnapshotSet& snapshots,
                                const WeatherSnapshot& weather,
                                uint8_t pageIndex,
                                uint8_t pageCount) {
    widgetPageIndex_ = pageIndex;
    widgetPageCount_ = pageCount;
    canvas.clear();
    drawClockAndStatusBar(widgetPageIndex_, widgetPageCount_);
    if (hasEnabledWidget(snapshots)) {
        for (uint8_t slot = 0; slot < transitink::kWidgetsPerPage; ++slot) {
            const bool drawDivider = shouldDrawLaneDivider(snapshots, slot);
            drawWidgetLane(slot, snapshots[slot], false, drawDivider);
        }
    } else {
        drawNoWidgetsHint();
    }
    drawWeatherFooter(weather);
    fullRefresh();
    dashboardFrameActive = true;
}

void EInkDisplay::refreshWidgetLane(uint8_t slot,
                                    const transitink::WidgetPageSnapshotSet& snapshots,
                                    const WeatherSnapshot& weather) {
    if (slot >= transitink::kWidgetsPerPage) {
        return;
    }
    if (!dashboardFrameActive || !previousFrameValid) {
        showDashboard(snapshots, weather);
        return;
    }

    const DisplayRegion& region = laneRegion(slot);
    std::memcpy(frameBuffer, previousFrameBuffer, sizeof(frameBuffer));
    clearRegion(region);
    const bool drawDivider = shouldDrawLaneDivider(snapshots, slot);
    drawWidgetLane(slot, snapshots[slot], false, drawDivider);
    partialRefresh(region.x, region.y, region.w, region.h);
    dashboardFrameActive = true;
}

void EInkDisplay::refreshClock(const transitink::WidgetPageSnapshotSet& snapshots,
                              const WeatherSnapshot& weather) {
    if (!dashboardFrameActive || !previousFrameValid) {
        showDashboard(snapshots, weather);
        return;
    }

    std::memcpy(frameBuffer, previousFrameBuffer, sizeof(frameBuffer));
    clearRegion(kStatusRegion);
    drawClockAndStatusBar(widgetPageIndex_, widgetPageCount_);
    partialRefresh(kStatusRegion.x, kStatusRegion.y, kStatusRegion.w, kStatusRegion.h);
    dashboardFrameActive = true;
}

void EInkDisplay::refreshWeatherFooter(const transitink::WidgetPageSnapshotSet& snapshots,
                                       const WeatherSnapshot& weather) {
    if (!dashboardFrameActive || !previousFrameValid) {
        showDashboard(snapshots, weather);
        return;
    }

    std::memcpy(frameBuffer, previousFrameBuffer, sizeof(frameBuffer));
    clearRegion(kFooterRegion);
    drawWeatherFooter(weather);
    partialRefresh(kFooterRegion.x, kFooterRegion.y, kFooterRegion.w, kFooterRegion.h);
    dashboardFrameActive = true;
}

void EInkDisplay::showSleep(const transitink::WidgetPageSnapshotSet& snapshots,
                            const WeatherSnapshot& weather,
                            uint8_t pageIndex,
                            uint8_t pageCount) {
    widgetPageIndex_ = pageIndex;
    widgetPageCount_ = pageCount;
    canvas.clear();
    drawClockAndStatusBar(widgetPageIndex_, widgetPageCount_);
    if (hasEnabledWidget(snapshots)) {
        for (uint8_t slot = 0; slot < transitink::kWidgetsPerPage; ++slot) {
            const bool drawDivider = shouldDrawLaneDivider(snapshots, slot);
            drawWidgetLane(slot, snapshots[slot], true, drawDivider);
        }
    } else {
        drawNoWidgetsHint();
    }
    drawWeatherFooter(weather);
    markNonDashboardFrame();
    fullRefresh();
}

void EInkDisplay::refreshSleepStatusAndWeather(
    const transitink::WidgetPageSnapshotSet& snapshots,
    const WeatherSnapshot& weather,
    uint8_t pageIndex,
    uint8_t pageCount) {
    widgetPageIndex_ = pageIndex;
    widgetPageCount_ = pageCount;
    if (!previousFrameValid) {
        showSleep(snapshots, weather, widgetPageIndex_, widgetPageCount_);
        return;
    }

    std::memcpy(frameBuffer, previousFrameBuffer, sizeof(frameBuffer));
    clearRegion(kStatusRegion);
    drawClockAndStatusBar(widgetPageIndex_, widgetPageCount_);
    clearRegion(kFooterRegion);
    drawWeatherFooter(weather);
    partialRefresh(kStatusRegion.x, kStatusRegion.y, kStatusRegion.w, kStatusRegion.h);
    partialRefresh(kFooterRegion.x, kFooterRegion.y, kFooterRegion.w, kFooterRegion.h);
    markNonDashboardFrame();
}

void EInkDisplay::prepareForSleep() {
    // Board-specific panel and battery power policy belongs in hardware adapters.
}
