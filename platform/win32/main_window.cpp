#include "platform/win32/main_window.h"

#include <commctrl.h>
#include <shellapi.h>

#include <utility>
#include <vector>

#include "platform/win32/resource.h"
#include "platform/win32/text_convert.h"
#include "third_party/scintilla/include/Scintilla.h"

namespace ep::win32 {

const wchar_t* MainWindow::ClassName() { return L"editorpp_main_window"; }

bool MainWindow::Create(HINSTANCE hInstance, int nCmdShow) {
  m_hInstance = hInstance;
  m_config = LoadAppConfig();

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WndProcStatic;
  wc.hInstance = hInstance;
  wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON));
  wc.hIconSm = wc.hIcon;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wc.lpszMenuName = MAKEINTRESOURCEW(IDR_MAINMENU);
  wc.lpszClassName = ClassName();
  if (!RegisterClassExW(&wc)) return false;

  m_haccel = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(IDR_ACCELERATORS));

  HWND hwnd = CreateWindowExW(WS_EX_ACCEPTFILES, ClassName(), L"editor++", WS_OVERLAPPEDWINDOW,
                               m_config.window.x,
                               m_config.window.y, m_config.window.width, m_config.window.height,
                               nullptr, nullptr, hInstance, this);
  if (!hwnd) return false;

  ShowWindow(hwnd, m_config.window.maximized ? SW_SHOWMAXIMIZED : nCmdShow);
  UpdateWindow(hwnd);
  return true;
}

LRESULT CALLBACK MainWindow::WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam,
                                            LPARAM lParam) {
  MainWindow* self = nullptr;
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
    self = static_cast<MainWindow*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->m_hwnd = hwnd;
  } else {
    self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }
  if (self) return self->WndProc(hwnd, msg, wParam, lParam);
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE:
      OnCreate(hwnd, m_hInstance);
      return 0;
    case WM_SIZE:
      OnSize(LOWORD(lParam), HIWORD(lParam));
      return 0;
    case WM_SETFOCUS:
      if (m_active >= 0 && m_active < static_cast<int>(m_tabs.size())) {
        m_tabs[m_active]->editor.SetFocus();
      }
      return 0;
    case WM_COMMAND:
      OnCommand(LOWORD(wParam));
      return 0;
    case WM_DRAWITEM: {
      auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
      if (dis->CtlID == static_cast<UINT>(IDC_TABBAR)) {
        m_tabbar.OnDrawItem(*dis);
        return TRUE;
      }
      return FALSE;
    }
    case WM_NOTIFY: {
      auto* hdr = reinterpret_cast<NMHDR*>(lParam);
      if (hdr->hwndFrom == m_tabbar.Handle()) {
        if (hdr->code == TCN_SELCHANGE && !m_suppress_tab_notifications) {
          SwitchToTab(m_tabbar.ActiveIndex());
        }
        return 0;
      }
      if (m_statusbar.HandleNotify(*hdr)) return 0;

      for (size_t i = 0; i < m_tabs.size(); i++) {
        if (m_tabs[i]->editor.Handle() != hdr->hwndFrom) continue;
        if (hdr->code == SCN_SAVEPOINTLEFT) {
          m_tabs[i]->doc.dirty = true;
          UpdateTabLabel(static_cast<int>(i));
          if (static_cast<int>(i) == m_active) UpdateTitle();
        } else if (hdr->code == SCN_SAVEPOINTREACHED) {
          m_tabs[i]->doc.dirty = false;
          UpdateTabLabel(static_cast<int>(i));
          if (static_cast<int>(i) == m_active) UpdateTitle();
        } else if (hdr->code == SCN_MODIFIED) {
          auto* scn = reinterpret_cast<SCNotification*>(lParam);
          if (scn->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) {
            m_tabs[i]->doc.backup_stale = true;
            ArmBackupTimer();
          }
        } else if (hdr->code == SCN_UPDATEUI) {
          if (static_cast<int>(i) == m_active) UpdateStatusBar();
        } else if (hdr->code == SCN_CHARADDED) {
          auto* scn = reinterpret_cast<SCNotification*>(lParam);
          m_tabs[i]->editor.HandleCharAdded(scn->ch);
        }
        break;
      }
      return 0;
    }
    case WM_TIMER:
      if (wParam == kBackupTimerId) {
        KillTimer(hwnd, kBackupTimerId);
        FlushAllBackups();
      }
      return 0;
    case WM_DROPFILES: {
      HDROP drop = reinterpret_cast<HDROP>(wParam);
      UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
      for (UINT i = 0; i < count; i++) {
        wchar_t path[MAX_PATH];
        if (DragQueryFileW(drop, i, path, MAX_PATH)) OpenFileFromPath(WideToUtf8(path));
      }
      DragFinish(drop);
      return 0;
    }
    case WM_COPYDATA: {
      // Sent by a second instance forwarding its command-line files to us
      // (see main.cpp's single-instance handling).
      auto* cds = reinterpret_cast<COPYDATASTRUCT*>(lParam);
      if (cds && cds->dwData == kCopyDataMagic && cds->lpData) {
        std::wstring paths(static_cast<const wchar_t*>(cds->lpData),
                            cds->cbData / sizeof(wchar_t));
        size_t start = 0;
        while (start < paths.size()) {
          size_t nl = paths.find(L'\n', start);
          if (nl == std::wstring::npos) nl = paths.size();
          if (nl > start) OpenFileFromPath(WideToUtf8(paths.substr(start, nl - start)));
          start = nl + 1;
        }
      }
      if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
      SetForegroundWindow(hwnd);
      return TRUE;
    }
    case WM_CLOSE:
      OnClose();
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(hwnd, msg, wParam, lParam);
  }
}

void MainWindow::OnCreate(HWND hwnd, HINSTANCE hInstance) {
  m_tabbar.Create(hwnd, hInstance, IDC_TABBAR);
  m_tabbar.SetDarkTheme(m_config.view.dark_theme);
  m_tabbar.on_close_request = [this](int index) { CloseTab(index); };
  m_tabbar.on_reorder = [this](int from, int to) { OnTabReorder(from, to); };
  m_tabbar.get_rename_seed = [this](int index) { return TabRenameSeedFor(index); };
  m_tabbar.on_rename_request = [this](int index, const std::string& label) {
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) return;
    m_tabs[index]->doc.custom_label = label;
    UpdateTabLabel(index);
    if (index == m_active) UpdateTitle();
  };

  m_statusbar.Create(hwnd, hInstance, IDC_STATUSBAR);
  m_statusbar.on_part_clicked = [this](StatusPart part, POINT pt) {
    OnStatusPartClicked(part, pt);
  };

  m_find_dialog.Create(hwnd, hInstance);
  m_find_dialog.get_active_editor = [this]() -> Editor* {
    return (m_active >= 0 && m_active < static_cast<int>(m_tabs.size()))
               ? &m_tabs[m_active]->editor
               : nullptr;
  };
  m_find_dialog.get_all_editors = [this]() {
    std::vector<Editor*> editors;
    for (auto& tab : m_tabs) editors.push_back(&tab->editor);
    return editors;
  };
  m_find_dialog.show_status = [this](const std::string& msg) { ShowStatusMessage(msg); };
  m_find_dialog.match_case = m_config.find.match_case;
  m_find_dialog.whole_word = m_config.find.whole_word;
  m_find_dialog.regex = m_config.find.regex;
  m_find_dialog.wrap_around = m_config.find.wrap_around;
  m_find_dialog.history = m_config.find.history;

  RebuildRecentMenu();
  RestoreSession();
}

void MainWindow::OnSize(int width, int height) {
  m_tabbar.Resize(0, 0, width);
  m_statusbar.Resize(width);
  int status_h = m_statusbar.Height();
  if (m_active >= 0 && m_active < static_cast<int>(m_tabs.size())) {
    m_tabs[m_active]->editor.Resize(0, TabBar::kHeight, width,
                                     height - TabBar::kHeight - status_h);
  }
}

void MainWindow::OnClose() {
  for (auto& tab : m_tabs) {
    if (tab->doc.backup_stale) {
      m_session_manager.WriteBackup(tab->doc, tab->editor.GetText());
    }
  }
  SaveSessionNow();

  WINDOWPLACEMENT wp{};
  wp.length = sizeof(wp);
  GetWindowPlacement(m_hwnd, &wp);
  m_config.window.maximized = (wp.showCmd == SW_SHOWMAXIMIZED);
  if (!m_config.window.maximized) {
    m_config.window.x = wp.rcNormalPosition.left;
    m_config.window.y = wp.rcNormalPosition.top;
    m_config.window.width = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
    m_config.window.height = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
  }

  m_config.find.match_case = m_find_dialog.match_case;
  m_config.find.whole_word = m_find_dialog.whole_word;
  m_config.find.regex = m_find_dialog.regex;
  m_config.find.wrap_around = m_find_dialog.wrap_around;
  m_config.find.history = m_find_dialog.history;

  SaveAppConfig(m_config);

  DestroyWindow(m_hwnd);
}

void MainWindow::OnTabReorder(int from_index, int to_index) {
  if (from_index < 0 || to_index < 0 ||
      from_index >= static_cast<int>(m_tabs.size()) ||
      to_index >= static_cast<int>(m_tabs.size())) {
    return;
  }
  std::swap(m_tabs[from_index], m_tabs[to_index]);
  if (m_active == from_index) {
    m_active = to_index;
  } else if (m_active == to_index) {
    m_active = from_index;
  }

  m_suppress_tab_notifications = true;
  for (int i = static_cast<int>(m_tabs.size()) - 1; i >= 0; i--) m_tabbar.RemoveTab(i);
  for (size_t i = 0; i < m_tabs.size(); i++) {
    m_tabbar.AddTab(static_cast<int>(i), TabLabelFor(m_tabs[i]->doc));
  }
  m_tabbar.SetActive(m_active);
  m_suppress_tab_notifications = false;
}

MainWindow::Tab& MainWindow::AllocateTab() {
  auto tab = std::make_unique<Tab>();
  tab->editor.Create(m_hwnd, m_hInstance, IDC_EDITOR);
  ShowWindow(tab->editor.Handle(), SW_HIDE);
  ApplyEditorSettingsToTab(*tab);
  m_tabs.push_back(std::move(tab));
  return *m_tabs.back();
}

int MainWindow::NewTab() {
  AllocateTab();
  int index = static_cast<int>(m_tabs.size()) - 1;
  m_tabbar.AddTab(index, "Untitled");
  return index;
}

void MainWindow::SwitchToTab(int index) {
  if (index < 0 || index >= static_cast<int>(m_tabs.size())) return;
  if (m_active >= 0 && m_active < static_cast<int>(m_tabs.size()) && m_active != index) {
    ShowWindow(m_tabs[m_active]->editor.Handle(), SW_HIDE);
  }
  m_active = index;

  RECT rc;
  GetClientRect(m_hwnd, &rc);
  int status_h = m_statusbar.Height();
  m_tabs[m_active]->editor.Resize(0, TabBar::kHeight, rc.right,
                                   rc.bottom - TabBar::kHeight - status_h);
  ShowWindow(m_tabs[m_active]->editor.Handle(), SW_SHOW);
  m_tabs[m_active]->editor.SetFocus();
  if (m_tabbar.ActiveIndex() != index) m_tabbar.SetActive(index);
  UpdateTitle();
  UpdateStatusBar();
}

int MainWindow::FindTabByPath(const std::string& utf8_path) const {
  for (size_t i = 0; i < m_tabs.size(); i++) {
    if (m_tabs[i]->doc.path == utf8_path) return static_cast<int>(i);
  }
  return -1;
}

void MainWindow::CloseTab(int index) {
  if (index < 0 || index >= static_cast<int>(m_tabs.size())) return;

  if (m_tabs.size() == 1) {
    Tab& tab = *m_tabs[0];
    m_session_manager.DeleteBackup(tab.doc);
    tab.editor.SetText("");
    tab.editor.MarkSaved();
    tab.doc = Document{};
    UpdateTabLabel(0);
    UpdateTitle();
    SaveSessionNow();
    return;
  }

  Tab& tab = *m_tabs[index];
  m_session_manager.DeleteBackup(tab.doc);
  DestroyWindow(tab.editor.Handle());
  m_suppress_tab_notifications = true;
  m_tabbar.RemoveTab(index);
  m_suppress_tab_notifications = false;
  m_tabs.erase(m_tabs.begin() + index);

  if (index == m_active) {
    int new_active = index < static_cast<int>(m_tabs.size()) ? index : index - 1;
    m_active = -1;
    SwitchToTab(new_active);
  } else if (index < m_active) {
    m_active--;
  }

  SaveSessionNow();
}

std::string MainWindow::TabLabelFor(const Document& doc) const {
  std::string base = doc.custom_label;
  if (base.empty()) {
    std::string name = doc.HasPath() ? doc.path : "Untitled";
    size_t slash = name.find_last_of("/\\");
    base = (slash == std::string::npos) ? name : name.substr(slash + 1);
  }
  return doc.dirty ? base + " *" : base;
}

void MainWindow::UpdateTabLabel(int index) {
  m_tabbar.SetLabel(index, TabLabelFor(m_tabs[index]->doc));
}

std::string MainWindow::TabRenameSeedFor(int index) const {
  const Document& doc = m_tabs[index]->doc;
  if (!doc.custom_label.empty()) return doc.custom_label;
  std::string name = doc.HasPath() ? doc.path : "Untitled";
  size_t slash = name.find_last_of("/\\");
  return (slash == std::string::npos) ? name : name.substr(slash + 1);
}

void MainWindow::UpdateTitle() {
  if (m_active < 0 || m_active >= static_cast<int>(m_tabs.size())) {
    SetWindowTextW(m_hwnd, L"editor++");
    return;
  }
  std::string title = TabLabelFor(m_tabs[m_active]->doc) + " - editor++";
  SetWindowTextW(m_hwnd, Utf8ToWide(title).c_str());
}

}  // namespace ep::win32
