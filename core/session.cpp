#include "core/session.h"

#include "core/ini.h"

namespace ep {

namespace {
std::string TabSection(size_t index) { return "tab" + std::to_string(index); }
}  // namespace

std::string SerializeSession(const SessionState& state) {
  IniDocument doc;
  doc.SetInt("session", "active_tab", state.active_tab_index);
  doc.SetInt("session", "tab_count", static_cast<int>(state.tabs.size()));

  for (size_t i = 0; i < state.tabs.size(); i++) {
    const TabState& t = state.tabs[i];
    std::string section = TabSection(i);
    doc.Set(section, "path", t.path);
    doc.Set(section, "backup", t.backup_path);
    doc.SetInt(section, "cursor", t.cursor_pos);
    doc.SetInt(section, "scroll", t.scroll_top_line);
    doc.SetInt(section, "sel_start", t.sel_start);
    doc.SetInt(section, "sel_end", t.sel_end);
  }

  return doc.Serialize();
}

SessionState DeserializeSession(const std::string& ini_text) {
  IniDocument doc = IniDocument::Parse(ini_text);
  SessionState state;

  int count = doc.GetInt("session", "tab_count", 0);
  if (count < 0) count = 0;

  for (int i = 0; i < count; i++) {
    std::string section = TabSection(static_cast<size_t>(i));
    TabState t;
    t.path = doc.Get(section, "path", "");
    t.backup_path = doc.Get(section, "backup", "");
    t.cursor_pos = doc.GetInt(section, "cursor", 0);
    t.scroll_top_line = doc.GetInt(section, "scroll", 0);
    t.sel_start = doc.GetInt(section, "sel_start", 0);
    t.sel_end = doc.GetInt(section, "sel_end", 0);
    state.tabs.push_back(std::move(t));
  }

  state.active_tab_index = doc.GetInt("session", "active_tab", 0);
  if (state.tabs.empty()) {
    state.active_tab_index = 0;
  } else if (state.active_tab_index < 0 ||
             state.active_tab_index >= static_cast<int>(state.tabs.size())) {
    state.active_tab_index = 0;
  }

  return state;
}

}  // namespace ep
