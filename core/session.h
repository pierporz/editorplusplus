#pragma once

#include <string>
#include <vector>

namespace ep {

// One open editor tab's persisted state. Position/scroll/selection are
// Scintilla byte offsets, opaque to core/ -- platform/ is the only layer
// that interprets them.
struct TabState {
  std::string path;         // UTF-8; empty if never saved (Untitled)
  std::string backup_path;  // relative path under backup/, empty if none
  int cursor_pos = 0;
  int scroll_top_line = 0;
  int sel_start = 0;
  int sel_end = 0;
};

struct SessionState {
  std::vector<TabState> tabs;
  int active_tab_index = 0;
};

// Serialized as INI (see core/ini.h), kept in session.ini next to the exe,
// separate from the user-preferences editor++.ini.
std::string SerializeSession(const SessionState& state);

// Never fails: a missing/corrupt session.ini simply deserializes to an empty
// session (no tabs), so the app just starts fresh instead of refusing to run.
SessionState DeserializeSession(const std::string& ini_text);

}  // namespace ep
