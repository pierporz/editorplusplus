#pragma once

#include <windows.h>

#include <functional>
#include <string>

namespace ep::win32 {

// Wraps a SysTabControl32 (owner-drawn) to add what the stock control
// doesn't support: a close 'x' per tab, drag-to-reorder, and double-click
// rename. Selection itself (click a tab to activate it) is still the
// control's native behavior -- the parent picks it up via the ordinary
// TCN_SELCHANGE WM_NOTIFY code.
class TabBar {
 public:
  static constexpr int kHeight = 26;

  bool Create(HWND parent, HINSTANCE hInstance, int controlId);
  HWND Handle() const { return m_hwnd; }

  void AddTab(int index, const std::string& utf8_label);
  void RemoveTab(int index);
  void SetLabel(int index, const std::string& utf8_label);
  void SetActive(int index);
  int ActiveIndex() const;
  int Count() const;
  void Resize(int x, int y, int width);

  void OnDrawItem(const DRAWITEMSTRUCT& dis);
  void SetDarkTheme(bool dark) { m_dark_theme = dark; }

  // X button or middle-click on a tab.
  std::function<void(int)> on_close_request;
  // Fired mid-drag when tab `from_index` has just been moved to sit where
  // `to_index` used to be; the caller is expected to reorder its own model
  // to match (see MainWindow::OnTabReorder).
  std::function<void(int, int)> on_reorder;
  // Fired when an in-place rename (double-click a tab) is committed with
  // Enter or by losing focus; not fired on Escape/empty text.
  std::function<void(int, const std::string&)> on_rename_request;
  // Asked for the text to prefill the rename box with for a given tab.
  std::function<std::string(int)> get_rename_seed;

 private:
  static LRESULT CALLBACK SubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                        LPARAM lParam, UINT_PTR subclass_id,
                                        DWORD_PTR ref_data);
  LRESULT HandleSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  static LRESULT CALLBACK RenameEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                                  LPARAM lParam, UINT_PTR subclass_id,
                                                  DWORD_PTR ref_data);

  RECT CloseButtonRect(int index) const;
  int HitTestCloseButton(POINT pt) const;
  void BeginRename(int index);
  void CommitRename();
  void CancelRename();

  HWND m_hwnd = nullptr;
  bool m_dark_theme = false;
  bool m_dragging = false;
  int m_drag_index = -1;
  POINT m_drag_start{0, 0};

  HWND m_rename_edit = nullptr;
  int m_rename_index = -1;
};

}  // namespace ep::win32
