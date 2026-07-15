// Sha256.h
// Thin SHA256 wrapper using Win32 BCrypt API. Returns lowercase hex digest.
#pragma once

#include <string>

namespace rm {

// Returns lowercase hex SHA256 digest of the given UTF-8 string.
std::string Sha256Hex(const std::string& input);

} // namespace rm