#pragma once

#include <Arduino.h>

#include "AppConfig.h"
#include "ConfigStore.h"
#include "core/BatteryStatus.h"

bool encodePortalConfig(const DeviceConfig& config,
                        const bus_eta::BatterySnapshot& battery,
                        const String& firmwareVersion,
                        const String& csrfToken,
                        String& outJson,
                        String& error);
bool decodePortalSave(const String& body,
                      const DeviceConfig& current,
                      DeviceConfig& outConfig,
                      String& error);
bool savePortalConfig(const String& body,
                      DeviceConfig& liveConfig,
                      ConfigStore& store,
                      String& error);
