#include "ConfigStore.h"

bool ConfigStore::begin() {
    return preferences_.begin("bus_eta", false);
}

bool ConfigStore::load(DeviceConfig& config) {
    String json = preferences_.getString("config", "");
    if (json.isEmpty()) {
        return false;
    }
    String error;
    return parseDeviceConfigJson(json, config, error);
}

bool ConfigStore::save(const DeviceConfig& config) {
    String json = serializeDeviceConfigJson(config);
    if (json.isEmpty()) {
        return false;
    }
    return preferences_.putString("config", json) > 0;
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
