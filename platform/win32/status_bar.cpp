#include "platform/win32/status_bar.h"

#include <commctrl.h>

#include "platform/win32/text_convert.h"

namespace ep::win32 {

namespace {
// Fixed pixel widths for the trailing panes; the message pane (index 0)
// absorbs whatever width is left.
constexpr int kFixedWidths[] = {150, 180, 150, 90, 60, 60};
}  // namespace

bool StatusBar::Create(HWND parent, HINSTANCE hInstance, int controlId) {
  m_hwnd = CreateWindowExW(0, STATUSCLASSNAMEW, L"", WS_CHILD | WS_VISIBLE | SBT_TOOLTIPS, 0, 0,
                            0, 0, parent,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)), hInstance,
                            nullptr);
  if (!m_hwnd) return false;
  Resize(0);
  return true;
}

void StatusBar::Resize(int parent_width) {
  SendMessageW(m_hwnd, WM_SIZE, 0, 0);

  int fixed_sum = 0;
  for (int w : kFixedWidths) fixed_sum += w;
  int message_width = parent_width - fixed_sum;
  if (message_width < 60) message_width = 60;

  int rights[kStatusPartCount];
  rights[0] = message_width;
  for (int i = 0; i < 6; i++) rights[i + 1] = rights[i] + kFixedWidths[i];
  rights[kStatusPartCount - 1] = -1;  // last pane fills to the window edge

  SendMessageW(m_hwnd, SB_SETPARTS, kStatusPartCount, reinterpret_cast<LPARAM>(rights));
}

int StatusBar::Height() const {
  RECT rc;
  GetWindowRect(m_hwnd, &rc);
  return rc.bottom - rc.top;
}

void StatusBar::SetText(StatusPart part, const std::string& utf8_text) {
  std::wstring wide = Utf8ToWide(utf8_text);
  SendMessageW(m_hwnd, SB_SETTEXTW, static_cast<WPARAM>(part), reinterpret_cast<LPARAM>(wide.c_str()));
}

bool StatusBar::HandleNotify(const NMHDR& hdr) {
  if (hdr.hwndFrom != m_hwnd) return false;
  if (hdr.code != NM_CLICK) return true;

  auto* mouse = reinterpret_cast<const NMMOUSE*>(&hdr);
  int part = static_cast<int>(mouse->dwItemSpec);
  if ((part == static_cast<int>(StatusPart::Encoding) ||
       part == static_cast<int>(StatusPart::Eol)) &&
      on_part_clicked) {
    POINT pt = mouse->pt;
    ClientToScreen(m_hwnd, &pt);
    on_part_clicked(static_cast<StatusPart>(part), pt);
  }
  return true;
}

}  // namespace ep::win32
