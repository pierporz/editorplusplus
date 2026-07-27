#pragma once

#include <string>

#include "platform/win32/editor.h"

namespace ep::win32::tools {

// Each operates on the selection if there is one, else the whole document;
// applies the result as a single undo action; and preserves scroll
// position. Returns "" on success, or a message to surface in the status
// bar on failure (in which case the buffer is left untouched).
std::string JsonPretty(Editor& editor);
std::string JsonMinify(Editor& editor);
std::string XmlPretty(Editor& editor);
std::string Base64Encode(Editor& editor);
std::string Base64Decode(Editor& editor);

}  // namespace ep::win32::tools
