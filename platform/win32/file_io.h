#pragma once

#include <string>

#include "core/encoding.h"
#include "core/result.h"

namespace ep::win32 {

// Raw byte I/O only. Encoding/BOM/EOL interpretation happens in core/encoding.
ep::Result<std::string> ReadFileBytes(const std::string& utf8_path);

// Atomic write: writes to "<path>.tmp" then MoveFileEx(..., REPLACE_EXISTING)
// onto the real path, so a crash or power loss never leaves a half-written
// file where the original used to be.
ep::Result<void> WriteFileAtomic(const std::string& utf8_path,
                                  const std::string& bytes);

// Loads a file for editing: detects its encoding, strips a BOM if present,
// and transcodes ANSI/UTF-16 content to UTF-8 (the Scintilla buffer and all
// of core/ only ever see UTF-8).
struct LoadedTextFile {
  std::string text;  // UTF-8
  ep::Encoding encoding;
};
ep::Result<LoadedTextFile> ReadTextFileForEditing(const std::string& utf8_path);

// The inverse of the transcoding step above, used just before writing to
// disk: turns the Scintilla buffer's UTF-8 text into the bytes for the
// given target encoding, adding a BOM for Utf8Bom.
std::string EncodeForWriting(const std::string& utf8_text, ep::Encoding encoding);

}  // namespace ep::win32
