#include "core/PortalRequestAuth.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace transitink {
namespace {

bool isJsonContentType(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    const std::string expected = "application/json";
    if (value.compare(0, expected.size(), expected) != 0) {
        return false;
    }
    return value.size() == expected.size() || value[expected.size()] == ';';
}

bool constantTimeEqual(const std::string& lhs, const std::string& rhs) {
    if (lhs.size() != rhs.size() || lhs.empty()) {
        return false;
    }
    unsigned char difference = 0;
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        difference |= static_cast<unsigned char>(lhs[index] ^ rhs[index]);
    }
    return difference == 0;
}

std::string normalizedHeaderValue(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool isAllowedHost(const std::string& host, const std::string& allowedHost) {
    const std::string normalizedHost = normalizedHeaderValue(host);
    const std::string normalizedAllowed = normalizedHeaderValue(allowedHost);
    if (normalizedHost == normalizedAllowed) {
        return true;
    }
    const std::string portPrefix = normalizedAllowed + ":";
    if (normalizedHost.compare(0, portPrefix.size(), portPrefix) != 0) {
        return false;
    }
    const std::string port = normalizedHost.substr(portPrefix.size());
    if (port.empty() || port.size() > 5 ||
        !std::all_of(port.begin(), port.end(), [](unsigned char c) {
            return std::isdigit(c) != 0;
        })) {
        return false;
    }
    const long parsed = std::strtol(port.c_str(), nullptr, 10);
    return parsed > 0 && parsed <= 65535;
}

}  // namespace

bool isPortalSaveAuthorized(const std::string& contentType,
                            const std::string& submittedToken,
                            const std::string& expectedToken) {
    return isJsonContentType(contentType) && constantTimeEqual(submittedToken, expectedToken);
}

bool isPortalAccessTokenAuthorized(const std::string& submittedToken,
                                   const std::string& expectedToken) {
    return constantTimeEqual(submittedToken, expectedToken);
}

std::string generatePortalApPassword(std::uint32_t first,
                                     std::uint32_t second,
                                     std::uint32_t third) {
    static constexpr char alphabet[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
    const std::uint32_t words[] = {first, second, third};
    std::string password;
    password.reserve(12);
    for (const std::uint32_t word : words) {
        for (unsigned int offset = 0; offset < 4; ++offset) {
            password.push_back(alphabet[(word >> (offset * 5U)) & 0x1FU]);
        }
    }
    return password;
}

bool isPortalRequestSourceAllowed(const std::string& host,
                                  const std::string& origin,
                                  const std::string& allowedHost,
                                  bool validateOrigin) {
    if (!isAllowedHost(host, allowedHost)) {
        return false;
    }
    if (!validateOrigin) {
        return true;
    }
    return !origin.empty() && normalizedHeaderValue(origin) ==
           "http://" + normalizedHeaderValue(allowedHost);
}

}  // namespace transitink
