#include "FirmwareUpdateService.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>

#include "ProductConfig.h"
#include "TransitTlsTrust.h"
#include "core/FirmwareUpdateCore.h"

namespace transitink {
namespace {

constexpr std::size_t kMaxManifestBytes = 2048;
constexpr unsigned long kReadTimeoutMs = 15000;

String digestHex(const unsigned char digest[32]) {
    static constexpr char kHex[] = "0123456789abcdef";
    char encoded[65];
    for (std::size_t index = 0; index < 32; ++index) {
        encoded[index * 2] = kHex[digest[index] >> 4];
        encoded[index * 2 + 1] = kHex[digest[index] & 0x0f];
    }
    encoded[64] = '\0';
    return String(encoded);
}

}  // namespace

bool FirmwareUpdateService::fetchManifest(FirmwareUpdateManifest& manifest,
                                          String& error) {
    manifest = FirmwareUpdateManifest{};
    if (WiFi.status() != WL_CONNECTED) {
        error = "裝置未連接 Wi-Fi";
        return false;
    }

    WiFiClientSecure tls;
    configureFirmwareUpdateVerifiedTls(tls);
    HTTPClient http;
    http.useHTTP10(true);
    http.setTimeout(kReadTimeoutMs);
    http.setReuse(false);
    const String url =
        String(TRANSITINK_FIRMWARE_UPDATE_BASE_URL) +
        "/ota-manifest.json?current=" + FIRMWARE_VERSION +
        "&nonce=" + String(esp_random(), HEX);
    if (!http.begin(tls, url)) {
        error = "未能建立韌體更新 HTTPS 連線";
        return false;
    }
    http.addHeader("Accept", "application/json");
    http.addHeader("Accept-Encoding", "identity");
    http.addHeader("Cache-Control", "no-cache");
    const int code = http.GET();
    const int length = http.getSize();
    if (code != HTTP_CODE_OK || length < 0 ||
        length > static_cast<int>(kMaxManifestBytes)) {
        error = code == HTTP_CODE_OK ? "韌體更新資料大小不正確"
                                     : String("韌體更新 HTTP 錯誤: ") + code;
        http.end();
        return false;
    }
    const String body = http.getString();
    http.end();
    if (body.length() != static_cast<unsigned int>(length)) {
        error = "韌體更新資料下載不完整";
        return false;
    }

    StaticJsonDocument<1024> doc;
    const DeserializationError parseError = deserializeJson(doc, body);
    if (parseError || !doc.is<JsonObject>() ||
        doc["schema_version"] != 1 ||
        String(doc["product"] | "") != FIRMWARE_PRODUCT_ID ||
        String(doc["board"] | "") != FIRMWARE_BOARD_ID ||
        !doc["version"].is<const char*>() ||
        !doc["firmware"].is<const char*>() ||
        !doc["sha256"].is<const char*>() ||
        !doc["size"].is<unsigned int>()) {
        error = "韌體更新資料格式不正確";
        return false;
    }

    manifest.version = doc["version"].as<const char*>();
    manifest.firmwarePath = doc["firmware"].as<const char*>();
    manifest.sha256 = doc["sha256"].as<const char*>();
    manifest.size = doc["size"].as<unsigned int>();
    if (!isSemanticFirmwareVersion(manifest.version.c_str()) ||
        !isSafeFirmwareAssetPath(manifest.firmwarePath.c_str()) ||
        !isSha256Digest(manifest.sha256.c_str()) || manifest.size == 0) {
        error = "韌體更新資料內容不正確";
        return false;
    }

    const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
    if (target == nullptr || manifest.size > target->size) {
        error = "韌體更新檔案超出可用分割區";
        return false;
    }
    int versionComparison = 0;
    if (!compareSemanticFirmwareVersions(
            manifest.version.c_str(), FIRMWARE_VERSION, versionComparison)) {
        error = "目前韌體版本格式不正確";
        return false;
    }
    manifest.updateAvailable = versionComparison > 0;
    error = "";
    return true;
}

bool FirmwareUpdateService::check(FirmwareUpdateManifest& manifest,
                                  String& error) {
    return fetchManifest(manifest, error);
}

bool FirmwareUpdateService::downloadAndStage(
    const FirmwareUpdateManifest& manifest,
    String& error) {
    WiFiClientSecure tls;
    configureFirmwareUpdateVerifiedTls(tls);
    HTTPClient http;
    http.useHTTP10(true);
    http.setTimeout(kReadTimeoutMs);
    http.setReuse(false);
    const String url = String(TRANSITINK_FIRMWARE_UPDATE_BASE_URL) + "/" +
                       manifest.firmwarePath;
    if (!http.begin(tls, url)) {
        error = "未能建立韌體下載 HTTPS 連線";
        return false;
    }
    http.addHeader("Accept-Encoding", "identity");
    const int code = http.GET();
    const int length = http.getSize();
    if (code != HTTP_CODE_OK ||
        length != static_cast<int>(manifest.size)) {
        error = code == HTTP_CODE_OK ? "韌體更新檔案大小不符"
                                     : String("韌體下載 HTTP 錯誤: ") + code;
        http.end();
        return false;
    }
    if (!Update.begin(manifest.size, U_FLASH)) {
        error = String("未能準備 OTA 分割區: ") + Update.errorString();
        http.end();
        return false;
    }

    mbedtls_sha256_context hash;
    mbedtls_sha256_init(&hash);
    mbedtls_sha256_starts(&hash, 0);
    NetworkClient* stream = http.getStreamPtr();
    uint8_t buffer[4096];
    std::size_t received = 0;
    unsigned long lastDataAt = millis();
    bool complete = true;
    while (received < manifest.size) {
        const int available = stream == nullptr ? 0 : stream->available();
        if (available <= 0) {
            if (stream == nullptr || (!http.connected() && available == 0) ||
                millis() - lastDataAt >= kReadTimeoutMs) {
                complete = false;
                break;
            }
            delay(1);
            continue;
        }
        const std::size_t remaining = manifest.size - received;
        const std::size_t requested =
            min<std::size_t>(sizeof(buffer),
                             min<std::size_t>(remaining, available));
        const int read = stream->read(buffer, requested);
        if (read <= 0) {
            complete = false;
            break;
        }
        lastDataAt = millis();
        if (Update.write(buffer, static_cast<std::size_t>(read)) !=
            static_cast<std::size_t>(read)) {
            complete = false;
            break;
        }
        mbedtls_sha256_update(&hash, buffer, static_cast<std::size_t>(read));
        received += static_cast<std::size_t>(read);
        yield();
    }
    http.end();

    unsigned char digest[32];
    mbedtls_sha256_finish(&hash, digest);
    mbedtls_sha256_free(&hash);
    if (!complete || received != manifest.size) {
        Update.abort();
        error = "韌體更新檔案下載或寫入不完整";
        return false;
    }
    if (digestHex(digest) != manifest.sha256) {
        Update.abort();
        error = "韌體更新 SHA-256 驗證失敗";
        return false;
    }
    if (!Update.end(false)) {
        error = String("韌體更新啟用失敗: ") + Update.errorString();
        return false;
    }
    error = "";
    return true;
}

bool FirmwareUpdateService::install(const String& expectedVersion,
                                    FirmwareUpdateManifest& manifest,
                                    String& error) {
    if (!fetchManifest(manifest, error)) {
        return false;
    }
    if (!manifest.updateAvailable ||
        expectedVersion != manifest.version) {
        error = "韌體版本已變更，請重新檢查更新";
        return false;
    }
    return downloadAndStage(manifest, error);
}

bool FirmwareUpdateService::confirmRunningFirmware() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (running == nullptr ||
        esp_ota_get_state_partition(running, &state) != ESP_OK ||
        state != ESP_OTA_IMG_PENDING_VERIFY) {
        return true;
    }
    return esp_ota_mark_app_valid_cancel_rollback() == ESP_OK;
}

}  // namespace transitink
