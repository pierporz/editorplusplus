#pragma once

#include <string>

#include "core/encoding.h"

namespace ep::win32 {

// In-memory record of one editor buffer's identity. Content itself lives in
// the Scintilla control (see Editor); this only tracks what the buffer *is*.
struct Document {
  std::string path;  // UTF-8. Empty means "never saved" (Untitled).
  bool dirty = false;
  ep::Encoding encoding = ep::Encoding::Utf8;
  ep::Eol eol = ep::Eol::CRLF;
  std::string language;    // Lexilla lexer name, "" for plain text
  bool large_file = false;  // >10MB: opened without a lexer or word wrap

  // User-set tab label override (double-click a tab to set it); empty means
  // derive the label from path/Untitled as usual. Cosmetic only -- never
  // touches the file on disk, and isn't persisted across restarts.
  std::string custom_label;

  // Crash/close recovery (see session_manager.h): stable id assigned once
  // per tab, used to derive backup_path; backup_stale marks that the
  // in-memory content has changed since the last backup write.
  int backup_id = 0;
  std::string backup_path;  // relative to the app data dir, e.g. "backup\tab_3.bak"
  bool backup_stale = false;

  bool HasPath() const { return !path.empty(); }
};

}  // namespace ep::win32
