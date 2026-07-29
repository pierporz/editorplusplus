#include "platform/win32/julian_calendar_popup.h"

#include <commctrl.h>

#include <cstdio>

#include "core/julian_date.h"
#include "platform/win32/resource.h"
#include "platform/win32/text_convert.h"

namespace ep::win32 {

namespace {
constexpr wchar_t kClassName[] = L"EPPJulianCalendarPopup";
constexpr int kLabelHeight = 22;
constexpr int kMargin = 6;

LRESULT CALLBACK MonthCalSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                       UINT_PTR /*subclass_id*/, DWORD_PTR ref_data) {
  auto* self = reinterpret_cast<JulianCalendarPopup*>(ref_data);
  if (msg == WM_KEYDOWN && wParam == VK_ESCAPE) {
    self->Hide();
    return 0;
  }
  if (msg == WM_NCDESTROY) {
    RemoveWindowSubclass(hwnd, MonthCalSubclassProc, 0);
  }
  return DefSubclassProc(hwnd, msg, wParam, lParam);
}
}  // namespace

bool JulianCalendarPopup::Create(HWND owner, HINSTANCE hInstance) {
  m_hInstance = hInstance;

  static bool class_registered = false;
  if (!class_registered) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProcStatic;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_BTNFACE) + 1);
    wc.lpszClassName = kClassName;
    if (!RegisterClassW(&wc)) return false;
    class_registered = true;
  }

  m_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, kClassName, L"", WS_POPUP | WS_BORDER, 0, 0, 0, 0,
                            owner, nullptr, hInstance, this);
  return m_hwnd != nullptr;
}

void JulianCalendarPopup::Show(POINT screen_pt) {
  SYSTEMTIME today;
  GetLocalTime(&today);
  MonthCal_SetCurSel(m_month_cal, &today);
  UpdateLabel(today);

  RECT rect{};
  GetWindowRect(m_hwnd, &rect);
  int height = rect.bottom - rect.top;
  // Anchor above the click point: the status bar sits at the bottom of the
  // window, so opening downward would usually run off the screen.
  SetWindowPos(m_hwnd, HWND_TOP, screen_pt.x, screen_pt.y - height, 0, 0,
               SWP_NOSIZE | SWP_SHOWWINDOW);
  SetForegroundWindow(m_hwnd);
  SetFocus(m_month_cal);
}

void JulianCalendarPopup::Hide() { ShowWindow(m_hwnd, SW_HIDE); }

void JulianCalendarPopup::UpdateLabel(const SYSTEMTIME& date) {
  std::string julian = ep::JulianDate(date.wYear, date.wMonth, date.wDay);
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%02d/%02d/%04d  =  %s", date.wDay, date.wMonth, date.wYear,
                julian.c_str());
  SetWindowTextW(m_label, Utf8ToWide(buf).c_str());
}

LRESULT CALLBACK JulianCalendarPopup::WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam,
                                                     LPARAM lParam) {
  JulianCalendarPopup* self;
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
    self = reinterpret_cast<JulianCalendarPopup*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  } else {
    self = reinterpret_cast<JulianCalendarPopup*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }
  if (self) return self->WndProc(hwnd, msg, wParam, lParam);
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT JulianCalendarPopup::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CREATE: {
      m_label = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER, kMargin,
                                 kMargin, 200, kLabelHeight, hwnd,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_JULIAN_LABEL)),
                                 m_hInstance, nullptr);
      m_month_cal = CreateWindowExW(
          0, MONTHCAL_CLASSW, L"", WS_CHILD | WS_VISIBLE, kMargin, kMargin * 2 + kLabelHeight, 0,
          0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_JULIAN_MONTHCAL)),
          m_hInstance, nullptr);

      RECT req{};
      MonthCal_GetMinReqRect(m_month_cal, &req);
      int cal_w = req.right - req.left;
      int cal_h = req.bottom - req.top;
      MoveWindow(m_month_cal, kMargin, kMargin * 2 + kLabelHeight, cal_w, cal_h, TRUE);
      MoveWindow(m_label, kMargin, kMargin, cal_w, kLabelHeight, TRUE);

      RECT window_rect{0, 0, cal_w + kMargin * 2, cal_h + kLabelHeight + kMargin * 3};
      AdjustWindowRectEx(&window_rect, GetWindowLongW(hwnd, GWL_STYLE), FALSE,
                          GetWindowLongW(hwnd, GWL_EXSTYLE));
      SetWindowPos(hwnd, nullptr, 0, 0, window_rect.right - window_rect.left,
                   window_rect.bottom - window_rect.top, SWP_NOMOVE | SWP_NOZORDER);

      HFONT font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
      SendMessageW(m_label, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
      SendMessageW(m_month_cal, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

      SetWindowSubclass(m_month_cal, MonthCalSubclassProc, 0, reinterpret_cast<DWORD_PTR>(this));
      return 0;
    }
    case WM_NOTIFY: {
      auto* hdr = reinterpret_cast<NMHDR*>(lParam);
      if (hdr->hwndFrom == m_month_cal &&
          (hdr->code == MCN_SELCHANGE || hdr->code == MCN_SELECT)) {
        auto* sel = reinterpret_cast<NMSELCHANGE*>(lParam);
        UpdateLabel(sel->stSelStart);
        if (hdr->code == MCN_SELECT) Hide();
      }
      return 0;
    }
    case WM_ACTIVATE:
      if (LOWORD(wParam) == WA_INACTIVE) Hide();
      return 0;
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace ep::win32
