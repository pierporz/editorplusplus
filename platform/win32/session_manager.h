#pragma once

#include <string>
#include <vector>

#include "platform/win32/document.h"

namespace ep::win32 {

// What SessionManager needs from a live tab to persist it. Kept separate
// from Editor so the persistence logic doesn't need a real Scintilla HWND.
struct TabSnapshot {
  Document* doc;
  int cursor_pos;
  int scroll_top_line;
  int sel_start;
  int sel_end;
};

struct RestoredTab {
  Document doc;
  int cursor_pos = 0;
  int scroll_top_line = 0;
  int sel_start = 0;
  int sel_end = 0;
  std::string text;  // content to load into the new Editor, already UTF-8
};

struct RestoredSession {
  std::vector<RestoredTab> tabs;
  int active_index = 0;
};

// Owns backup/ and session.ini under AppDataDir(). One instance lives for
// the whole app run.
class SessionManager {
 public:
  // Assigns doc.backup_id/backup_path if this is the tab's first backup,
  // then atomically (re)writes the backup file with `current_text`.
  void WriteBackup(Document& doc, const std::string& current_text);

  // Called once a tab's content is safely saved to its real path: the
  // backup is no longer needed to recover unsaved work.
  void DeleteBackup(Document& doc);

  void SaveSession(const std::vector<TabSnapshot>& tabs, int active_index);

  // Never fails: a missing/corrupt session.ini just yields an empty result,
  // so callers fall back to a single blank tab.
  RestoredSession LoadSession();

 private:
  int NextBackupId();
  static int ParseBackupId(const std::string& backup_path);

  int m_next_backup_id = 1;
};

}  // namespace ep::win32
