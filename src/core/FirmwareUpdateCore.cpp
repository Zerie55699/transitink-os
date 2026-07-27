#include "core/FirmwareUpdateCore.h"

#include <array>
#include <cstdint>

namespace transitink {
namespace {

bool parseVersion(std::string_view value, std::array<uint16_t, 3>& parts) {
    std::size_t start = 0;
    for (std::size_t index = 0; index < parts.size(); ++index) {
        const std::size_t separator =
            index + 1 == parts.size() ? value.size() : value.find('.', start);
        if (separator == std::string_view::npos || separator <= start) {
            return false;
        }
        uint32_t part = 0;
        for (std::size_t offset = start; offset < separator; ++offset) {
            const char character = value[offset];
            if (character < '0' || character > '9') {
                return false;
            }
            part = part * 10U + static_cast<uint32_t>(character - '0');
            if (part > UINT16_MAX) {
                return false;
            }
        }
        parts[index] = static_cast<uint16_t>(part);
        start = separator + 1;
    }
    return start == value.size() + 1;
}

}  // namespace

bool isSemanticFirmwareVersion(std::string_view value) {
    std::array<uint16_t, 3> parts{};
    return parseVersion(value, parts);
}

bool compareSemanticFirmwareVersions(std::string_view left,
                                     std::string_view right,
                                     int& comparison) {
    std::array<uint16_t, 3> leftParts{};
    std::array<uint16_t, 3> rightParts{};
    if (!parseVersion(left, leftParts) || !parseVersion(right, rightParts)) {
        comparison = 0;
        return false;
    }
    comparison = 0;
    for (std::size_t index = 0; index < leftParts.size(); ++index) {
        if (leftParts[index] != rightParts[index]) {
            comparison = leftParts[index] < rightParts[index] ? -1 : 1;
            break;
        }
    }
    return true;
}

bool isSafeFirmwareAssetPath(std::string_view value) {
    constexpr std::string_view kPrefix = "firmware/";
    constexpr std::string_view kSuffix = ".bin";
    if (value.size() <= kPrefix.size() + kSuffix.size() ||
        value.size() > 128 || value.substr(0, kPrefix.size()) != kPrefix ||
        value.substr(value.size() - kSuffix.size()) != kSuffix ||
        value.find("..") != std::string_view::npos ||
        value.find(':') != std::string_view::npos ||
        value.find('\\') != std::string_view::npos) {
        return false;
    }
    for (const char character : value) {
        const bool safe = (character >= 'a' && character <= 'z') ||
                          (character >= 'A' && character <= 'Z') ||
                          (character >= '0' && character <= '9') ||
                          character == '/' || character == '-' ||
                          character == '_' || character == '.';
        if (!safe) {
            return false;
        }
    }
    return true;
}

bool isSha256Digest(std::string_view value) {
    if (value.size() != 64) {
        return false;
    }
    for (const char character : value) {
        const bool digit = character >= '0' && character <= '9';
        const bool lowerHex = character >= 'a' && character <= 'f';
        if (!digit && !lowerHex) {
            return false;
        }
    }
    return true;
}

}  // namespace transitink
