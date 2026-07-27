#pragma once

#include <string_view>

namespace transitink {

bool isSemanticFirmwareVersion(std::string_view value);
bool compareSemanticFirmwareVersions(std::string_view left,
                                     std::string_view right,
                                     int& comparison);
bool isSafeFirmwareAssetPath(std::string_view value);
bool isSha256Digest(std::string_view value);

}  // namespace transitink
