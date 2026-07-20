#pragma once

#include <cstdint>
#include <string>

namespace transitink {

bool isPortalSaveAuthorized(const std::string& contentType,
                            const std::string& submittedToken,
                            const std::string& expectedToken);

bool isPortalAccessTokenAuthorized(const std::string& submittedToken,
                                   const std::string& expectedToken);

std::string generatePortalApPassword(std::uint32_t first,
                                     std::uint32_t second,
                                     std::uint32_t third);

bool isPortalRequestSourceAllowed(const std::string& host,
                                  const std::string& origin,
                                  const std::string& allowedHost,
                                  bool validateOrigin);

}  // namespace transitink
