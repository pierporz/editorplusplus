#pragma once

#include <string>

#include "core/result.h"

namespace ep {

// Standard RFC 4648 base64 (with '+', '/' and '=' padding).
std::string Base64Encode(const std::string& bytes);

// Rejects non-alphabet characters other than whitespace (space, \t, \n, \r,
// which are skipped) and malformed padding. Does not check whether the
// decoded bytes form valid UTF-8 -- that is the caller's job.
Result<std::string> Base64Decode(const std::string& encoded);

}  // namespace ep
