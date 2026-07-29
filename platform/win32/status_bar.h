#pragma once

#include <windows.h>

#include <functional>
#include <string>

namespace ep::win32 {

enum class StatusPart { Message = 0, Position, Length, Selection, Encoding, Eol, InsertMode, JulianDate };
constexpr int kStatusPartCount = 8;

// Thin wrapper around a standard Win32 status bar (msctls_statusbar32) with
// fixed-width trailing panes and a flexible leading message pane. Updated
// from MainWindow on SCN_UPDATEUI (never per keystroke, see CLAUDE.md perf
// notes) plus whenever the active tab or its encoding/EOL changes.
class StatusBar {
 public:
  bool Create(HWND parent, HINSTANCE hInstance, int controlId);
  HWND Handle() const { return m_hwnd; }

  void Resize(int parent_width);
  int Height() const;

  void SetText(StatusPart part, const std::string& utf8_text);

  // Fired on a left click inside the Encoding, EOL, or JulianDate pane;
  // screen_pt is where the resulting popup (conversion menu or calendar)
  // should be anchored.
  std::function<void(StatusPart part, POINT screen_pt)> on_part_clicked;

  // Called from MainWindow::WndProc's WM_NOTIFY case; returns true if the
  // notification was for this control and has been handled.
  bool HandleNotify(const NMHDR& hdr);

 private:
  HWND m_hwnd = nullptr;
};

}  // namespace ep::win32
