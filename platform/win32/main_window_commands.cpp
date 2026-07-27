// Menu/command dispatch, status bar updates, and view-setting toggles --
// split out of main_window.cpp to keep files under ~500 lines (see
// CLAUDE.md section 6).
#include "platform/win32/main_window.h"

#include <algorithm>

#include "core/text_stats.h"
#include "platform/win32/resource.h"
#include "platform/win32/syntax_highlight.h"
#include "platform/win32/text_convert.h"
#include "platform/win32/text_tools.h"
#include "platform/win32/view_settings.h"
#include "third_party/scintilla/include/Scintilla.h"

namespace ep::win32 {

void MainWindow::OnCommand(int id) {
  switch (id) {
    case ID_FILE_NEW:
      SwitchToTab(NewTab());
      break;
    case ID_FILE_OPEN:
      CmdOpen();
      break;
    case ID_FILE_SAVE:
      CmdSave();
      break;
    case ID_FILE_SAVEAS:
      CmdSaveAs();
      break;
    case ID_FILE_CLOSE:
      if (m_active >= 0) CloseTab(m_active);
      break;
    case ID_FILE_CLOSEALL:
      while (m_tabs.size() > 1) CloseTab(static_cast<int>(m_tabs.size()) - 1);
      if (!m_tabs.empty()) CloseTab(0);
      break;
    case ID_FILE_EXIT:
      PostMessageW(m_hwnd, WM_CLOSE, 0, 0);
      break;
    case ID_EDIT_UNDO:
      if (m_active >= 0) m_tabs[m_active]->editor.Send(SCI_UNDO);
      break;
    case ID_EDIT_REDO:
      if (m_active >= 0) m_tabs[m_active]->editor.Send(SCI_REDO);
      break;
    case ID_EDIT_CUT:
      if (m_active >= 0) m_tabs[m_active]->editor.Send(SCI_CUT);
      break;
    case ID_EDIT_COPY:
      if (m_active >= 0) m_tabs[m_active]->editor.Send(SCI_COPY);
      break;
    case ID_EDIT_PASTE:
      if (m_active >= 0) m_tabs[m_active]->editor.Send(SCI_PASTE);
      break;
    case ID_EDIT_SELECTALL:
      if (m_active >= 0) m_tabs[m_active]->editor.Send(SCI_SELECTALL);
      break;
    case ID_EDIT_FIND:
      m_find_dialog.Show();
      break;
    case ID_EDIT_FINDNEXT:
      m_find_dialog.FindNext();
      break;
    case ID_EDIT_FINDPREV:
      m_find_dialog.FindPrevious();
      break;
    case ID_WINDOW_NEXTTAB:
      if (!m_tabs.empty()) SwitchToTab((m_active + 1) % static_cast<int>(m_tabs.size()));
      break;
    case ID_WINDOW_PREVTAB:
      if (!m_tabs.empty()) {
        SwitchToTab((m_active - 1 + static_cast<int>(m_tabs.size())) %
                    static_cast<int>(m_tabs.size()));
      }
      break;
    case ID_VIEW_WORDWRAP:
      m_config.view.word_wrap = !m_config.view.word_wrap;
      ApplyViewSettingsToAllTabs();
      break;
    case ID_VIEW_WHITESPACE:
      m_config.view.show_whitespace = !m_config.view.show_whitespace;
      ApplyViewSettingsToAllTabs();
      break;
    case ID_VIEW_LINENUMBERS:
      m_config.view.line_numbers = !m_config.view.line_numbers;
      ApplyViewSettingsToAllTabs();
      break;
    case ID_VIEW_INDENTGUIDES:
      m_config.view.indent_guides = !m_config.view.indent_guides;
      ApplyViewSettingsToAllTabs();
      break;
    case ID_VIEW_CURRENTLINE:
      m_config.view.current_line_highlight = !m_config.view.current_line_highlight;
      ApplyViewSettingsToAllTabs();
      break;
    case ID_VIEW_ZOOMRESET:
      if (m_active >= 0) m_tabs[m_active]->editor.Send(SCI_SETZOOM, 0);
      break;
    case ID_VIEW_DARKTHEME:
      ToggleDarkTheme();
      break;
    case ID_TOOLS_JSON_PRETTY:
      RunTool(tools::JsonPretty, "json");
      break;
    case ID_TOOLS_JSON_MINIFY:
      RunTool(tools::JsonMinify, "json");
      break;
    case ID_TOOLS_XML_PRETTY:
      RunTool(tools::XmlPretty, "xml");
      break;
    case ID_TOOLS_BASE64_ENCODE:
      RunTool(tools::Base64Encode);
      break;
    case ID_TOOLS_BASE64_DECODE:
      RunTool(tools::Base64Decode);
      break;
    default:
      if (id >= ID_RECENT_FILE_BASE && id < ID_RECENT_FILE_BASE + 10) {
        size_t index = static_cast<size_t>(id - ID_RECENT_FILE_BASE);
        if (index < m_config.recent_files.size()) OpenFileFromPath(m_config.recent_files[index]);
      }
      break;
  }
}

void MainWindow::RunTool(std::string (*tool)(Editor&), const char* language_on_success) {
  if (m_active < 0) return;
  Tab& tab = *m_tabs[m_active];
  bool had_selection = tab.editor.Send(SCI_GETSELECTIONSTART) != tab.editor.Send(SCI_GETSELECTIONEND);

  std::string error = tool(tab.editor);
  ShowStatusMessage(error.empty() ? "Done" : error);

  if (error.empty() && !had_selection && language_on_success &&
      tab.doc.language != language_on_success) {
    tab.doc.language = language_on_success;
    ApplyLanguage(tab.editor, tab.doc.language);
  }
}

void MainWindow::ShowStatusMessage(const std::string& utf8_message) {
  m_statusbar.SetText(StatusPart::Message, utf8_message);
}

void MainWindow::ApplyViewSettingsToAllTabs() {
  for (auto& tab : m_tabs) ApplyViewSettings(tab->editor, m_config.view);
}

void MainWindow::ApplyEditorSettingsToTab(Tab& tab) {
  ApplyViewSettings(tab.editor, m_config.view);
  tab.editor.SetTabSettings(m_config.editor.tab_width, m_config.editor.use_spaces);
  tab.editor.SetAutoIndent(m_config.editor.auto_indent);
}

void MainWindow::ToggleDarkTheme() {
  m_config.view.dark_theme = !m_config.view.dark_theme;
  m_tabbar.SetDarkTheme(m_config.view.dark_theme);
  InvalidateRect(m_tabbar.Handle(), nullptr, TRUE);
  for (auto& tab : m_tabs) {
    ApplyViewSettings(tab->editor, m_config.view);
    // Re-clears and rebuilds every style, needed for STYLE_DEFAULT's new
    // background to actually reach already-colored syntax tokens too (see
    // the comment in view_settings.cpp).
    ApplyLanguage(tab->editor, tab->doc.language);
  }
}

void MainWindow::AddRecentFile(const std::string& utf8_path) {
  auto& list = m_config.recent_files;
  auto it = std::find(list.begin(), list.end(), utf8_path);
  if (it != list.end()) list.erase(it);
  list.insert(list.begin(), utf8_path);
  if (list.size() > 10) list.resize(10);
  RebuildRecentMenu();
}

void MainWindow::RebuildRecentMenu() {
  HMENU file_menu = GetSubMenu(GetMenu(m_hwnd), 0);
  if (!file_menu) return;
  // Position 8 = the "Recent Files" popup; see resource.rc's File menu.
  HMENU recent_menu = GetSubMenu(file_menu, 8);
  if (!recent_menu) return;

  while (GetMenuItemCount(recent_menu) > 0) RemoveMenu(recent_menu, 0, MF_BYPOSITION);

  if (m_config.recent_files.empty()) {
    AppendMenuW(recent_menu, MF_STRING | MF_GRAYED, ID_RECENT_EMPTY, L"(empty)");
    return;
  }
  for (size_t i = 0; i < m_config.recent_files.size() && i < 10; i++) {
    AppendMenuW(recent_menu, MF_STRING, ID_RECENT_FILE_BASE + i,
                Utf8ToWide(m_config.recent_files[i]).c_str());
  }
}

void MainWindow::UpdateStatusBar() {
  if (m_active < 0 || m_active >= static_cast<int>(m_tabs.size())) return;
  Editor& editor = m_tabs[m_active]->editor;
  Document& doc = m_tabs[m_active]->doc;

  Sci_Position pos = editor.Send(SCI_GETCURRENTPOS);
  Sci_Position line = editor.Send(SCI_LINEFROMPOSITION, static_cast<WPARAM>(pos));
  Sci_Position col = editor.Send(SCI_GETCOLUMN, static_cast<WPARAM>(pos));
  m_statusbar.SetText(StatusPart::Position, "Ln " + std::to_string(line + 1) + ", Col " +
                                                 std::to_string(col + 1) + ", Pos " +
                                                 std::to_string(pos));

  ep::TextStats stats = ep::ComputeTextStats(editor.GetText());
  m_statusbar.SetText(StatusPart::Length, "Length: " + std::to_string(stats.char_count) +
                                               "  Lines: " + std::to_string(stats.line_count));

  Sci_Position sel_start = editor.Send(SCI_GETSELECTIONSTART);
  Sci_Position sel_end = editor.Send(SCI_GETSELECTIONEND);
  if (sel_end > sel_start) {
    Sci_Position sel_chars = editor.Send(SCI_COUNTCHARACTERS, static_cast<WPARAM>(sel_start),
                                          static_cast<LPARAM>(sel_end));
    Sci_Position sel_lines = editor.Send(SCI_LINEFROMPOSITION, static_cast<WPARAM>(sel_end)) -
                              editor.Send(SCI_LINEFROMPOSITION, static_cast<WPARAM>(sel_start)) + 1;
    m_statusbar.SetText(StatusPart::Selection, "Sel: " + std::to_string(sel_chars) + " char, " +
                                                    std::to_string(sel_lines) + " lines");
  } else {
    m_statusbar.SetText(StatusPart::Selection, "Sel: 0");
  }

  m_statusbar.SetText(StatusPart::Encoding, ep::EncodingName(doc.encoding));
  m_statusbar.SetText(StatusPart::Eol, ep::EolName(doc.eol));
  m_statusbar.SetText(StatusPart::InsertMode, editor.Send(SCI_GETOVERTYPE) ? "OVR" : "INS");
}

namespace {
// Shows `labels` as a popup menu at `screen_pt`; returns the 1-based index
// chosen, or 0 if the menu was dismissed without a choice.
int ShowChoiceMenu(HWND owner, POINT screen_pt, std::initializer_list<const wchar_t*> labels) {
  HMENU menu = CreatePopupMenu();
  UINT id = 1;
  for (const wchar_t* label : labels) AppendMenuW(menu, MF_STRING, id++, label);
  SetForegroundWindow(owner);
  int choice = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, screen_pt.x,
                               screen_pt.y, 0, owner, nullptr);
  DestroyMenu(menu);
  return choice;
}
}  // namespace

void MainWindow::OnStatusPartClicked(StatusPart part, POINT screen_pt) {
  if (m_active < 0) return;
  Tab& tab = *m_tabs[m_active];

  if (part == StatusPart::Encoding) {
    int choice = ShowChoiceMenu(m_hwnd, screen_pt,
                                 {L"UTF-8", L"UTF-8 BOM", L"UTF-16LE", L"UTF-16BE", L"ANSI"});
    if (choice == 0) return;
    static constexpr ep::Encoding kEncodings[] = {ep::Encoding::Utf8, ep::Encoding::Utf8Bom,
                                                   ep::Encoding::Utf16LE, ep::Encoding::Utf16BE,
                                                   ep::Encoding::Ansi};
    tab.doc.encoding = kEncodings[choice - 1];
    tab.doc.dirty = true;
    UpdateTabLabel(m_active);
    UpdateTitle();
    UpdateStatusBar();
  } else if (part == StatusPart::Eol) {
    int choice = ShowChoiceMenu(m_hwnd, screen_pt,
                                 {L"CRLF (Windows)", L"LF (Unix)", L"CR (old Mac)"});
    if (choice == 0) return;
    static constexpr ep::Eol kEols[] = {ep::Eol::CRLF, ep::Eol::LF, ep::Eol::CR};
    tab.doc.eol = kEols[choice - 1];
    tab.editor.ConvertEolTo(tab.doc.eol);
    UpdateStatusBar();
  }
}

}  // namespace ep::win32
