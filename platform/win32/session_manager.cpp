#include "platform/win32/session_manager.h"

#include <windows.h>

#include <algorithm>
#include <cstdlib>

#include "core/session.h"
#include "platform/win32/app_paths.h"
#include "platform/win32/file_io.h"
#include "platform/win32/text_convert.h"

namespace ep::win32 {

namespace {
constexpr char kBackupPrefix[] = "tab_";
constexpr char kBackupSuffix[] = ".bak";
}  // namespace

int SessionManager::ParseBackupId(const std::string& backup_path) {
  size_t prefix_len = sizeof(kBackupPrefix) - 1;
  size_t suffix_len = sizeof(kBackupSuffix) - 1;
  if (backup_path.size() <= prefix_len + suffix_len) return 0;
  if (backup_path.compare(0, prefix_len, kBackupPrefix) != 0) return 0;
  if (backup_path.compare(backup_path.size() - suffix_len, suffix_len, kBackupSuffix) != 0) {
    return 0;
  }
  std::string digits =
      backup_path.substr(prefix_len, backup_path.size() - prefix_len - suffix_len);
  char* end = nullptr;
  long value = std::strtol(digits.c_str(), &end, 10);
  if (end == digits.c_str() || *end != '\0') return 0;
  return static_cast<int>(value);
}

int SessionManager::NextBackupId() { return m_next_backup_id++; }

void SessionManager::WriteBackup(Document& doc, const std::string& current_text) {
  if (doc.backup_path.empty()) {
    if (doc.backup_id == 0) doc.backup_id = NextBackupId();
    doc.backup_path = std::string(kBackupPrefix) + std::to_string(doc.backup_id) + kBackupSuffix;
  }
  WriteFileAtomic(BackupDir() + "\\" + doc.backup_path, current_text);
  doc.backup_stale = false;
}

void SessionManager::DeleteBackup(Document& doc) {
  if (doc.backup_path.empty()) return;
  std::wstring full = Utf8ToWide(BackupDir() + "\\" + doc.backup_path);
  DeleteFileW(full.c_str());
  doc.backup_path.clear();
  doc.backup_stale = false;
}

void SessionManager::SaveSession(const std::vector<TabSnapshot>& tabs, int active_index) {
  ep::SessionState state;
  state.active_tab_index = active_index;
  for (const auto& snap : tabs) {
    ep::TabState t;
    t.path = snap.doc->path;
    t.backup_path = snap.doc->backup_path;
    t.cursor_pos = snap.cursor_pos;
    t.scroll_top_line = snap.scroll_top_line;
    t.sel_start = snap.sel_start;
    t.sel_end = snap.sel_end;
    state.tabs.push_back(std::move(t));
  }
  WriteFileAtomic(SessionIniPath(), ep::SerializeSession(state));
}

RestoredSession SessionManager::LoadSession() {
  RestoredSession result;

  auto text = ReadFileBytes(SessionIniPath());
  if (!text) return result;

  ep::SessionState state = ep::DeserializeSession(text.Value());
  int max_backup_id = 0;

  for (const auto& t : state.tabs) {
    RestoredTab tab;
    bool loaded = false;

    if (!t.backup_path.empty()) {
      auto backup_bytes = ReadFileBytes(BackupDir() + "\\" + t.backup_path);
      if (backup_bytes) {
        tab.text = backup_bytes.Value();
        tab.doc.backup_path = t.backup_path;
        tab.doc.backup_id = ParseBackupId(t.backup_path);
        max_backup_id = std::max(max_backup_id, tab.doc.backup_id);
        tab.doc.dirty = true;
        loaded = true;
      }
    }

    if (!loaded && !t.path.empty()) {
      auto file = ReadTextFileForEditing(t.path);
      if (!file) continue;  // file moved/deleted since last session -- drop the tab
      tab.text = file.Value().text;
      tab.doc.encoding = file.Value().encoding;
      tab.doc.dirty = false;
      loaded = true;
    }

    if (!loaded && t.path.empty() && t.backup_path.empty()) {
      loaded = true;  // blank Untitled tab with nothing to restore
    }

    if (!loaded) continue;

    tab.doc.path = t.path;
    tab.cursor_pos = t.cursor_pos;
    tab.scroll_top_line = t.scroll_top_line;
    tab.sel_start = t.sel_start;
    tab.sel_end = t.sel_end;
    result.tabs.push_back(std::move(tab));
  }

  m_next_backup_id = max_backup_id + 1;

  if (result.tabs.empty()) {
    result.active_index = 0;
  } else {
    result.active_index = state.active_tab_index;
    if (result.active_index < 0 ||
        result.active_index >= static_cast<int>(result.tabs.size())) {
      result.active_index = 0;
    }
  }

  return result;
}

}  // namespace ep::win32
