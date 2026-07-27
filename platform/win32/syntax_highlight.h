#pragma once

#include <string>

#include "platform/win32/editor.h"

namespace ep::win32 {

// Maps a file extension to one of the Lexilla lexer names vendored in
// third_party/lexilla/lexers (see third_party/VERSIONS.md), or "" for plain
// text when the extension isn't recognized.
std::string DetectLanguageForPath(const std::string& utf8_path);

// Wires up the lexer, keyword lists, and color styles for `language` on
// `editor`. An empty language clears back to plain, unstyled text.
void ApplyLanguage(Editor& editor, const std::string& language);

}  // namespace ep::win32
