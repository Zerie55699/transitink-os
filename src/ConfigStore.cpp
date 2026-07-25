#include "ConfigStore.h"

#include <memory>
#include <new>

namespace {

constexpr const char* kConfigBlobKey = "config_blob";
constexpr const char* kLegacyConfigStringKey = "config";

}  // namespace

bool ConfigStore::begin() {
    return preferences_.begin("bus_eta", false);
}

bool ConfigStore::load(DeviceConfig& config) {
    String json;
    const std::size_t blobSize = preferences_.getBytesLength(kConfigBlobKey);
    if (blobSize > 0) {
        if (blobSize > transitink::kConfigJsonCapacity) {
            return false;
        }
        std::unique_ptr<char[]> buffer(new (std::nothrow) char[blobSize + 1]);
        if (!buffer ||
            preferences_.getBytes(kConfigBlobKey, buffer.get(), blobSize) != blobSize) {
            return false;
        }
        buffer[blobSize] = '\0';
        json = buffer.get();
    } else {
        json = preferences_.getString(kLegacyConfigStringKey, "");
    }
    if (json.isEmpty()) {
        return false;
    }
    String error;
    return parseDeviceConfigJson(json, config, error);
}

bool ConfigStore::save(const DeviceConfig& config) {
    String json = serializeDeviceConfigJson(config);
    if (json.isEmpty() ||
        json.length() > transitink::kConfigBlobSafeBytes) {
        return false;
    }
    return preferences_.putBytes(kConfigBlobKey, json.c_str(), json.length()) ==
           json.length();
}

bool ConfigStore::sleepResumePending() {
    return preferences_.getBool("sleep_resume", false);
}

bool ConfigStore::setSleepResumePending(bool pending) {
    if (sleepResumePending() == pending) {
        return true;
    }
    return preferences_.putBool("sleep_resume", pending) == 1;
}

void ConfigStore::clear() {
    preferences_.clear();
}
