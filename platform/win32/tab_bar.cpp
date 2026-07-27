#include "platform/win32/tab_bar.h"

#include <commctrl.h>
#include <windowsx.h>

#include "platform/win32/text_convert.h"

namespace ep::win32 {

namespace {
constexpr int kMinTabWidth = 90;
// Real tab width is left to the control's own auto-sizing (based on label
// text extent), which is what lets tabs widen for longer filenames instead
// of all being clipped to one fixed width. The trailing spaces reserve
// room in that auto-computed width for the close button we paint over it.
const wchar_t kCloseButtonPadding[] = L"   ";
constexpr int kCloseBoxSize = 14;
constexpr int kCloseBoxMargin = 6;
constexpr int kDragThreshold = 4;
}  // namespace

bool TabBar::Create(HWND parent, HINSTANCE hInstance, int controlId) {
  m_hwnd = CreateWindowExW(
      0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | TCS_OWNERDRAWFIXED | TCS_TOOLTIPS,
      0, 0, 0, kHeight, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
      hInstance, nullptr);
  if (!m_hwnd) return false;

  TabCtrl_SetMinTabWidth(m_hwnd, kMinTabWidth);
  SetWindowSubclass(m_hwnd, SubclassProc, 0, reinterpret_cast<DWORD_PTR>(this));
  return true;
}

void TabBar::AddTab(int index, const std::string& utf8_label) {
  std::wstring wide = Utf8ToWide(utf8_label) + kCloseButtonPadding;
  TCITEMW item{};
  item.mask = TCIF_TEXT;
  item.pszText = wide.data();
  TabCtrl_InsertItem(m_hwnd, index, &item);
}

void TabBar::RemoveTab(int index) { TabCtrl_DeleteItem(m_hwnd, index); }

void TabBar::SetLabel(int index, const std::string& utf8_label) {
  std::wstring wide = Utf8ToWide(utf8_label) + kCloseButtonPadding;
  TCITEMW item{};
  item.mask = TCIF_TEXT;
  item.pszText = wide.data();
  TabCtrl_SetItem(m_hwnd, index, &item);
  InvalidateRect(m_hwnd, nullptr, FALSE);
}

void TabBar::SetActive(int index) {
  TabCtrl_SetCurSel(m_hwnd, index);
  InvalidateRect(m_hwnd, nullptr, FALSE);
}

int TabBar::ActiveIndex() const { return TabCtrl_GetCurSel(m_hwnd); }

int TabBar::Count() const { return TabCtrl_GetItemCount(m_hwnd); }

void TabBar::Resize(int x, int y, int width) {
  MoveWindow(m_hwnd, x, y, width, kHeight, TRUE);
}

RECT TabBar::CloseButtonRect(int index) const {
  RECT item_rect{};
  TabCtrl_GetItemRect(m_hwnd, index, &item_rect);
  RECT close_rect;
  close_rect.right = item_rect.right - kCloseBoxMargin;
  close_rect.left = close_rect.right - kCloseBoxSize;
  close_rect.top = item_rect.top + (item_rect.bottom - item_rect.top - kCloseBoxSize) / 2;
  close_rect.bottom = close_rect.top + kCloseBoxSize;
  return close_rect;
}

int TabBar::HitTestCloseButton(POINT pt) const {
  TCHITTESTINFO hit{};
  hit.pt = pt;
  int index = TabCtrl_HitTest(m_hwnd, &hit);
  if (index < 0) return -1;
  RECT close_rect = CloseButtonRect(index);
  return PtInRect(&close_rect, pt) ? index : -1;
}

void TabBar::OnDrawItem(const DRAWITEMSTRUCT& dis) {
  HDC dc = dis.hDC;
  RECT rect = dis.rcItem;
  bool selected = (dis.itemState & ODS_SELECTED) != 0;

  COLORREF bg_color, text_color;
  if (m_dark_theme) {
    bg_color = selected ? RGB(45, 45, 45) : RGB(25, 25, 25);
    text_color = RGB(220, 220, 220);
  } else {
    bg_color = selected ? RGB(255, 255, 255) : RGB(224, 224, 224);
    text_color = RGB(0, 0, 0);
  }

  HBRUSH bg = CreateSolidBrush(bg_color);
  FillRect(dc, &rect, bg);
  DeleteObject(bg);
  FrameRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));

  TCITEMW item{};
  wchar_t buf[256] = {};
  item.mask = TCIF_TEXT;
  item.pszText = buf;
  item.cchTextMax = 255;
  TabCtrl_GetItem(m_hwnd, dis.itemID, &item);
  size_t len = wcslen(buf);
  while (len > 0 && buf[len - 1] == L' ') buf[--len] = L'\0';  // drop close-button padding

  RECT text_rect = rect;
  text_rect.left += 8;
  text_rect.right -= (kCloseBoxSize + kCloseBoxMargin + 4);
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, text_color);
  DrawTextW(dc, buf, -1, &text_rect, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);

  RECT close_rect = CloseButtonRect(static_cast<int>(dis.itemID));
  DrawTextW(dc, L"×", -1, &close_rect, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
}

LRESULT CALLBACK TabBar::SubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                       UINT_PTR /*subclass_id*/, DWORD_PTR ref_data) {
  auto* self = reinterpret_cast<TabBar*>(ref_data);
  return self->HandleSubclass(hwnd, msg, wParam, lParam);
}

LRESULT TabBar::HandleSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_LBUTTONDOWN: {
      POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      int close_index = HitTestCloseButton(pt);
      if (close_index >= 0) {
        if (on_close_request) on_close_request(close_index);
        return 0;
      }
      TCHITTESTINFO hit{};
      hit.pt = pt;
      m_drag_index = TabCtrl_HitTest(hwnd, &hit);
      m_drag_start = pt;
      m_dragging = false;
      break;
    }
    case WM_MOUSEMOVE: {
      if (m_drag_index >= 0 && (wParam & MK_LBUTTON)) {
        POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        if (!m_dragging) {
          int dx = pt.x - m_drag_start.x;
          int dy = pt.y - m_drag_start.y;
          if (dx * dx + dy * dy >= kDragThreshold * kDragThreshold) {
            m_dragging = true;
            SetCapture(hwnd);
          }
        }
        if (m_dragging) {
          TCHITTESTINFO hit{};
          hit.pt = pt;
          int target = TabCtrl_HitTest(hwnd, &hit);
          if (target >= 0 && target != m_drag_index) {
            if (on_reorder) on_reorder(m_drag_index, target);
            m_drag_index = target;
          }
        }
      }
      break;
    }
    case WM_LBUTTONUP: {
      if (m_dragging) {
        ReleaseCapture();
        m_dragging = false;
      }
      m_drag_index = -1;
      break;
    }
    case WM_MBUTTONUP: {
      POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      TCHITTESTINFO hit{};
      hit.pt = pt;
      int index = TabCtrl_HitTest(hwnd, &hit);
      if (index >= 0 && on_close_request) on_close_request(index);
      return 0;
    }
    case WM_LBUTTONDBLCLK: {
      POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
      if (HitTestCloseButton(pt) < 0) {
        TCHITTESTINFO hit{};
        hit.pt = pt;
        int index = TabCtrl_HitTest(hwnd, &hit);
        if (index >= 0) BeginRename(index);
      }
      return 0;
    }
    default:
      break;
  }
  return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void TabBar::BeginRename(int index) {
  CommitRename();  // finish any rename already in progress first

  RECT rect{};
  TabCtrl_GetItemRect(m_hwnd, index, &rect);
  rect.left += 4;
  rect.right -= (kCloseBoxSize + kCloseBoxMargin + 4);
  if (rect.right <= rect.left) return;

  std::wstring seed = get_rename_seed ? Utf8ToWide(get_rename_seed(index)) : L"";

  m_rename_index = index;
  m_rename_edit = CreateWindowExW(
      WS_EX_CLIENTEDGE, L"EDIT", seed.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, m_hwnd, nullptr,
      reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(m_hwnd, GWLP_HINSTANCE)), nullptr);
  if (!m_rename_edit) {
    m_rename_index = -1;
    return;
  }
  SendMessageW(m_rename_edit, WM_SETFONT, SendMessageW(m_hwnd, WM_GETFONT, 0, 0), TRUE);
  SetWindowSubclass(m_rename_edit, RenameEditSubclassProc, 0, reinterpret_cast<DWORD_PTR>(this));
  SetFocus(m_rename_edit);
  SendMessageW(m_rename_edit, EM_SETSEL, 0, -1);
}

void TabBar::CommitRename() {
  if (!m_rename_edit) return;
  wchar_t buf[256] = {};
  GetWindowTextW(m_rename_edit, buf, 255);
  int index = m_rename_index;
  HWND edit = m_rename_edit;
  m_rename_edit = nullptr;
  m_rename_index = -1;
  DestroyWindow(edit);

  std::string text = WideToUtf8(buf);
  size_t begin = text.find_first_not_of(' ');
  size_t end = text.find_last_not_of(' ');
  text = (begin == std::string::npos) ? "" : text.substr(begin, end - begin + 1);
  if (!text.empty() && on_rename_request) on_rename_request(index, text);
}

void TabBar::CancelRename() {
  if (!m_rename_edit) return;
  HWND edit = m_rename_edit;
  m_rename_edit = nullptr;
  m_rename_index = -1;
  DestroyWindow(edit);
}

LRESULT CALLBACK TabBar::RenameEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                                 UINT_PTR /*subclass_id*/, DWORD_PTR ref_data) {
  auto* self = reinterpret_cast<TabBar*>(ref_data);
  switch (msg) {
    case WM_KEYDOWN:
      if (wParam == VK_RETURN) {
        self->CommitRename();
        return 0;
      }
      if (wParam == VK_ESCAPE) {
        self->CancelRename();
        return 0;
      }
      break;
    case WM_KILLFOCUS:
      self->CommitRename();
      break;
    case WM_NCDESTROY:
      RemoveWindowSubclass(hwnd, RenameEditSubclassProc, 0);
      break;
    default:
      break;
  }
  return DefSubclassProc(hwnd, msg, wParam, lParam);
}

}  // namespace ep::win32
