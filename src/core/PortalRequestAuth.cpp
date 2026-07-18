#include "core/PortalRequestAuth.h"

#include <algorithm>
#include <cctype>

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

}  // namespace

bool isPortalSaveAuthorized(const std::string& contentType,
                            const std::string& submittedToken,
                            const std::string& expectedToken) {
    return isJsonContentType(contentType) && constantTimeEqual(submittedToken, expectedToken);
}

}  // namespace transitink
