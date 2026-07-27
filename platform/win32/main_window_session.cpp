// Session/backup persistence half of MainWindow -- split out of
// main_window.cpp to keep files under ~500 lines (see CLAUDE.md section 6).
#include "platform/win32/main_window.h"

#include "platform/win32/syntax_highlight.h"
#include "third_party/scintilla/include/Scintilla.h"

namespace ep::win32 {

void MainWindow::ArmBackupTimer() {
  SetTimer(m_hwnd, kBackupTimerId, kBackupDebounceMs, nullptr);
}

void MainWindow::FlushAllBackups() {
  bool any = false;
  for (auto& tab : m_tabs) {
    if (tab->doc.backup_stale) {
      m_session_manager.WriteBackup(tab->doc, tab->editor.GetText());
      any = true;
    }
  }
  if (any) SaveSessionNow();
}

void MainWindow::SaveSessionNow() {
  std::vector<TabSnapshot> snaps;
  snaps.reserve(m_tabs.size());
  for (auto& tab : m_tabs) {
    TabSnapshot snap;
    snap.doc = &tab->doc;
    snap.cursor_pos = static_cast<int>(tab->editor.Send(SCI_GETCURRENTPOS));
    snap.scroll_top_line = static_cast<int>(tab->editor.Send(SCI_GETFIRSTVISIBLELINE));
    snap.sel_start = static_cast<int>(tab->editor.Send(SCI_GETSELECTIONSTART));
    snap.sel_end = static_cast<int>(tab->editor.Send(SCI_GETSELECTIONEND));
    snaps.push_back(snap);
  }
  m_session_manager.SaveSession(snaps, m_active);
}

void MainWindow::RestoreSession() {
  RestoredSession restored = m_session_manager.LoadSession();

  for (auto& rt : restored.tabs) {
    Tab& tab = AllocateTab();
    tab.doc = rt.doc;
    tab.doc.large_file = rt.text.size() > kLargeFileThreshold;
    tab.doc.language = tab.doc.large_file ? "" : DetectLanguageForPath(tab.doc.path);
    ApplyLanguage(tab.editor, tab.doc.language);
    tab.editor.SetEolMode(tab.doc.eol);
    tab.editor.SetText(rt.text);
    if (tab.doc.large_file) tab.editor.Send(SCI_SETWRAPMODE, SC_WRAP_NONE);
    if (!rt.doc.dirty) tab.editor.MarkSaved();
    // SCI_SETSEL alone positions the caret (when sel_start == sel_end) or
    // restores a real selection; a separate SCI_GOTOPOS would just collapse
    // it back to a single point, so it's deliberately not called here.
    tab.editor.Send(SCI_SETSEL, static_cast<WPARAM>(rt.sel_start),
                     static_cast<LPARAM>(rt.sel_end));
    tab.editor.Send(SCI_SETFIRSTVISIBLELINE, static_cast<WPARAM>(rt.scroll_top_line));
    int index = static_cast<int>(m_tabs.size()) - 1;
    m_tabbar.AddTab(index, TabLabelFor(tab.doc));
  }

  if (m_tabs.empty()) {
    SwitchToTab(NewTab());
  } else {
    SwitchToTab(restored.active_index);
  }
}

}  // namespace ep::win32
