#pragma once

#include <Arduino.h>

namespace transitink {

struct FirmwareUpdateManifest {
    String version;
    String firmwarePath;
    String sha256;
    std::size_t size = 0;
    bool updateAvailable = false;
};

class FirmwareUpdateService {
public:
    bool check(FirmwareUpdateManifest& manifest, String& error);
    bool install(const String& expectedVersion,
                 FirmwareUpdateManifest& manifest,
                 String& error);
    static bool confirmRunningFirmware();

private:
    bool fetchManifest(FirmwareUpdateManifest& manifest, String& error);
    bool downloadAndStage(const FirmwareUpdateManifest& manifest, String& error);
};

}  // namespace transitink
