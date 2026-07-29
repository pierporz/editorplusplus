#pragma once

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "platform/win32/app_config.h"
#include "platform/win32/document.h"
#include "platform/win32/editor.h"
#include "platform/win32/find_replace.h"
#include "platform/win32/julian_calendar_popup.h"
#include "platform/win32/session_manager.h"
#include "platform/win32/status_bar.h"
#include "platform/win32/tab_bar.h"

namespace ep::win32 {

class MainWindow {
 public:
  bool Create(HINSTANCE hInstance, int nCmdShow);
  HWND Handle() const { return m_hwnd; }
  HACCEL Accelerators() const { return m_haccel; }
  static const wchar_t* ClassName();

  // WM_COPYDATA.dwData sentinel used for single-instance file forwarding
  // (see main.cpp).
  static constexpr ULONG_PTR kCopyDataMagic = 0x45505050;  // "EPPP"

  // Switches to the tab already showing this path if there is one; reuses a
  // single pristine blank tab if that's all that's open; otherwise opens a
  // new tab.
  void OpenFileFromPath(const std::string& utf8_path);

  // Lets the app message loop route input to the Find/Replace dialog.
  bool TranslateFindDialogMessage(MSG& msg) const {
    return m_find_dialog.TranslateDialogMessage(msg);
  }

 private:
  struct Tab {
    Document doc;
    Editor editor;
  };

  static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam,
                                         LPARAM lParam);
  LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  void OnCreate(HWND hwnd, HINSTANCE hInstance);
  void OnSize(int width, int height);
  void OnCommand(int id);
  void OnClose();
  void OnTabReorder(int from_index, int to_index);
  void OnStatusPartClicked(StatusPart part, POINT screen_pt);

  Tab& AllocateTab();
  int NewTab();
  void SwitchToTab(int index);
  void CloseTab(int index);
  int FindTabByPath(const std::string& utf8_path) const;

  void CmdOpen();
  void CmdSave();
  void CmdSaveAs();
  bool SaveToPath(int tab_index, const std::string& utf8_path);
  // language_on_success: if the tool succeeds and there was no selection
  // (i.e. it ran on the whole document), switch the tab to this Lexilla
  // language so e.g. Pretty Print JSON immediately gets JSON coloring.
  // Pass nullptr for tools that shouldn't touch the language (Base64).
  void RunTool(std::string (*tool)(Editor&), const char* language_on_success = nullptr);

  std::string TabLabelFor(const Document& doc) const;
  std::string TabRenameSeedFor(int index) const;
  void UpdateTabLabel(int index);
  void UpdateTitle();
  void UpdateStatusBar();
  void ShowStatusMessage(const std::string& utf8_message);
  void ApplyViewSettingsToAllTabs();
  void ApplyEditorSettingsToTab(Tab& tab);
  void ToggleDarkTheme();

  void AddRecentFile(const std::string& utf8_path);
  void RebuildRecentMenu();

  void ArmBackupTimer();
  void FlushAllBackups();
  void SaveSessionNow();
  void RestoreSession();

  static constexpr UINT_PTR kBackupTimerId = 1;
  static constexpr UINT kBackupDebounceMs = 3000;
  static constexpr size_t kLargeFileThreshold = 10 * 1024 * 1024;

  HWND m_hwnd = nullptr;
  HINSTANCE m_hInstance = nullptr;
  HACCEL m_haccel = nullptr;
  TabBar m_tabbar;
  StatusBar m_statusbar;
  FindReplaceDialog m_find_dialog;
  JulianCalendarPopup m_julian_calendar;
  SessionManager m_session_manager;
  AppConfig m_config;
  std::vector<std::unique_ptr<Tab>> m_tabs;
  int m_active = -1;
  bool m_suppress_tab_notifications = false;
};

}  // namespace ep::win32
