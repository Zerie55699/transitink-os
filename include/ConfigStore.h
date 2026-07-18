#pragma once

#include <Preferences.h>

#include "AppConfig.h"

class ConfigStore {
public:
    bool begin();
    bool load(DeviceConfig& config);
    bool save(const DeviceConfig& config);
    bool sleepResumePending();
    bool setSleepResumePending(bool pending);
    void clear();

private:
    Preferences preferences_;
};
