// File open/save commands -- split out of main_window.cpp to keep files
// under ~500 lines (see CLAUDE.md section 6).
#include "platform/win32/main_window.h"

#include <commdlg.h>

#include <algorithm>
#include <vector>

#include "platform/win32/file_io.h"
#include "platform/win32/syntax_highlight.h"
#include "platform/win32/text_convert.h"
#include "third_party/scintilla/include/Scintilla.h"

namespace ep::win32 {

namespace {
// Shared by the Open and Save As dialogs; index is 1-based to match
// OPENFILENAMEW::nFilterIndex.
const wchar_t kFileFilter[] =
    L"Text Files (*.txt)\0*.txt\0"
    L"JSON Files (*.json)\0*.json\0"
    L"XML Files (*.xml)\0*.xml\0"
    L"All Files (*.*)\0*.*\0";
constexpr DWORD kFilterIndexText = 1;
constexpr DWORD kFilterIndexJson = 2;
constexpr DWORD kFilterIndexXml = 3;
constexpr DWORD kFilterIndexAll = 4;
}  // namespace

void MainWindow::CmdOpen() {
  std::vector<wchar_t> buffer(32768, L'\0');
  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = m_hwnd;
  ofn.lpstrFile = buffer.data();
  ofn.nMaxFile = static_cast<DWORD>(buffer.size());
  ofn.lpstrFilter = kFileFilter;
  ofn.nFilterIndex = kFilterIndexAll;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_ALLOWMULTISELECT;
  if (!GetOpenFileNameW(&ofn)) return;

  const wchar_t* p = buffer.data();
  std::wstring first(p);
  p += first.size() + 1;
  if (*p == L'\0') {
    OpenFileFromPath(WideToUtf8(first));
    return;
  }
  std::wstring dir = first;
  while (*p != L'\0') {
    std::wstring name(p);
    p += name.size() + 1;
    OpenFileFromPath(WideToUtf8(dir + L"\\" + name));
  }
}

void MainWindow::OpenFileFromPath(const std::string& utf8_path) {
  int existing = FindTabByPath(utf8_path);
  if (existing >= 0) {
    SwitchToTab(existing);
    return;
  }

  auto file = ReadTextFileForEditing(utf8_path);
  if (!file) {
    MessageBoxW(m_hwnd, Utf8ToWide(file.Err().message).c_str(), L"editor++",
                MB_ICONERROR | MB_OK);
    return;
  }

  bool reuse_blank = m_tabs.size() == 1 && !m_tabs[0]->doc.HasPath() &&
                      !m_tabs[0]->doc.dirty && m_tabs[0]->editor.GetText().empty();
  int index = reuse_blank ? 0 : NewTab();

  bool large = file.Value().text.size() > kLargeFileThreshold;

  Tab& tab = *m_tabs[index];
  tab.doc.language = large ? "" : DetectLanguageForPath(utf8_path);
  ApplyLanguage(tab.editor, tab.doc.language);
  tab.editor.SetText(file.Value().text);
  tab.editor.MarkSaved();
  tab.doc.path = utf8_path;
  tab.doc.dirty = false;
  tab.doc.encoding = file.Value().encoding;
  tab.doc.eol = ep::DetectEol(file.Value().text);
  tab.doc.large_file = large;
  tab.editor.SetEolMode(tab.doc.eol);
  if (large) {
    tab.editor.Send(SCI_SETWRAPMODE, SC_WRAP_NONE);
    ShowStatusMessage("Large file (>10 MB): opened without syntax highlighting or word wrap");
  }
  m_session_manager.DeleteBackup(tab.doc);
  UpdateTabLabel(index);
  SwitchToTab(index);
  AddRecentFile(utf8_path);
  SaveSessionNow();
}

void MainWindow::CmdSave() {
  if (m_active < 0) return;
  Document& doc = m_tabs[m_active]->doc;
  if (!doc.HasPath()) {
    CmdSaveAs();
    return;
  }
  SaveToPath(m_active, doc.path);
}

void MainWindow::CmdSaveAs() {
  if (m_active < 0) return;
  Document& doc = m_tabs[m_active]->doc;

  wchar_t file[MAX_PATH] = {};
  if (doc.HasPath()) {
    std::wstring wide_path = Utf8ToWide(doc.path);
    size_t n = std::min(wide_path.size(), static_cast<size_t>(MAX_PATH - 1));
    wide_path.copy(file, n);
    file[n] = L'\0';
  }

  DWORD filter_index = kFilterIndexText;
  const wchar_t* def_ext = L"txt";
  if (doc.language == "json") {
    filter_index = kFilterIndexJson;
    def_ext = L"json";
  } else if (doc.language == "xml") {
    filter_index = kFilterIndexXml;
    def_ext = L"xml";
  }

  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = m_hwnd;
  ofn.lpstrFile = file;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter = kFileFilter;
  ofn.nFilterIndex = filter_index;
  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
  ofn.lpstrDefExt = def_ext;
  if (!GetSaveFileNameW(&ofn)) return;
  SaveToPath(m_active, WideToUtf8(file));
}

bool MainWindow::SaveToPath(int tab_index, const std::string& utf8_path) {
  Tab& tab = *m_tabs[tab_index];
  std::string bytes = EncodeForWriting(tab.editor.GetText(), tab.doc.encoding);
  auto result = WriteFileAtomic(utf8_path, bytes);
  if (!result) {
    MessageBoxW(m_hwnd, Utf8ToWide(result.Err().message).c_str(), L"editor++",
                MB_ICONERROR | MB_OK);
    return false;
  }
  tab.doc.path = utf8_path;
  tab.doc.dirty = false;
  tab.editor.MarkSaved();

  std::string new_language = tab.doc.large_file ? "" : DetectLanguageForPath(utf8_path);
  if (new_language != tab.doc.language) {
    tab.doc.language = new_language;
    ApplyLanguage(tab.editor, tab.doc.language);
  }

  m_session_manager.DeleteBackup(tab.doc);
  UpdateTabLabel(tab_index);
  if (tab_index == m_active) UpdateTitle();
  AddRecentFile(utf8_path);
  SaveSessionNow();
  return true;
}

}  // namespace ep::win32
