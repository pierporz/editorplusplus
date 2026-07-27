#pragma once

#include <windows.h>

#include <functional>
#include <string>
#include <vector>

#include "platform/win32/editor.h"

namespace ep::win32 {

// Non-modal Find/Replace dialog. Owns no editors itself -- MainWindow wires
// it up via the callbacks below so this stays decoupled from tab
// management. F3/Shift+F3 keep working through FindNext()/FindPrevious()
// even while the dialog window is hidden.
class FindReplaceDialog {
 public:
  bool Create(HWND parent, HINSTANCE hInstance);
  HWND Handle() const { return m_hwnd; }

  // Shows the dialog, prefilling "Find what" from the active editor's
  // current selection if there is one.
  void Show();

  // Give the app's message loop a chance to route keyboard input (Tab
  // navigation, Enter-as-default-button) to this dialog.
  bool TranslateDialogMessage(MSG& msg) const;

  void FindNext();
  void FindPrevious();

  std::function<Editor*()> get_active_editor;
  std::function<std::vector<Editor*>()> get_all_editors;
  std::function<void(const std::string&)> show_status;

  std::vector<std::string> history;  // most recent first, capped at 20
  bool match_case = false;
  bool whole_word = false;
  bool regex = false;
  bool wrap_around = true;

 private:
  static INT_PTR CALLBACK DialogProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  INT_PTR DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  void OnInitDialog();
  void OnFindNext();
  void OnFindPrevious();
  void OnCount();
  void OnReplace();
  void OnReplaceAll();

  bool DoFind(Editor& editor, bool forward);
  int SearchFlags() const;
  std::string GetFindText() const;
  std::string GetReplaceText() const;
  void RememberSearch(const std::string& text);
  void RefreshHistoryCombo();
  void SetStatus(const std::string& text);
  void SyncOptionsFromControls();

  // scope: 0 = current document, 1 = selection, 2 = all open tabs
  int Scope() const;

  HWND m_hwnd = nullptr;
};

}  // namespace ep::win32
