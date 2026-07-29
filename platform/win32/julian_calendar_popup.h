#pragma once

#include <windows.h>

namespace ep::win32 {

// Small borderless popup with a month calendar (SysMonthCal32) plus a label
// showing the highlighted day's Julian CYYDDD date (see core/julian_date.h).
// Opened by clicking the status bar's JulianDate panel; closes itself on
// Escape or when it loses activation (click elsewhere), like a combo
// dropdown.
class JulianCalendarPopup {
 public:
  bool Create(HWND owner, HINSTANCE hInstance);

  // Shows the popup anchored just below `screen_pt`, reset to today's date.
  void Show(POINT screen_pt);
  void Hide();

 private:
  static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  void UpdateLabel(const SYSTEMTIME& date);

  HWND m_hwnd = nullptr;
  HWND m_month_cal = nullptr;
  HWND m_label = nullptr;
  HINSTANCE m_hInstance = nullptr;
};

}  // namespace ep::win32
