#pragma once

#include <string>

namespace transitink {

bool isPortalSaveAuthorized(const std::string& contentType,
                            const std::string& submittedToken,
                            const std::string& expectedToken);

}  // namespace transitink
