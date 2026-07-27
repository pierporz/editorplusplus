#include "editor.h"

#include "third_party/scintilla/include/Scintilla.h"

namespace ep::win32 {

bool Editor::Create(HWND parent, HINSTANCE hInstance, int controlId) {
  m_hwnd = CreateWindowExW(
      WS_EX_CLIENTEDGE, L"Scintilla", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
      0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
      hInstance, nullptr);
  if (!m_hwnd) return false;

  Send(SCI_SETCODEPAGE, SC_CP_UTF8);
  Send(SCI_SETEOLMODE, SC_EOL_CRLF);
  Send(SCI_STYLESETFONT, STYLE_DEFAULT,
       reinterpret_cast<LPARAM>("Consolas"));
  Send(SCI_STYLESETSIZE, STYLE_DEFAULT, 11);
  Send(SCI_STYLECLEARALL);
  Send(SCI_SETMARGINWIDTHN, 0, 40);  // line number margin
  Send(SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
  Send(SCI_SETMODEVENTMASK, SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT);
  return true;
}

std::string Editor::GetText() const {
  Sci_Position length = Send(SCI_GETLENGTH);
  if (length <= 0) return std::string();
  std::string text(static_cast<size_t>(length), '\0');
  Send(SCI_GETTEXT, static_cast<WPARAM>(length) + 1,
       reinterpret_cast<LPARAM>(text.data()));
  return text;
}

void Editor::SetText(const std::string& utf8) {
  Send(SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(utf8.c_str()));
}

bool Editor::IsDirty() const { return Send(SCI_GETMODIFY) != 0; }

void Editor::MarkSaved() { Send(SCI_SETSAVEPOINT); }

namespace {
int ToSciEolMode(ep::Eol eol) {
  switch (eol) {
    case ep::Eol::LF:
      return SC_EOL_LF;
    case ep::Eol::CR:
      return SC_EOL_CR;
    case ep::Eol::CRLF:
    default:
      return SC_EOL_CRLF;
  }
}
}  // namespace

void Editor::SetEolMode(ep::Eol eol) { Send(SCI_SETEOLMODE, ToSciEolMode(eol)); }

void Editor::ConvertEolTo(ep::Eol eol) {
  int mode = ToSciEolMode(eol);
  Send(SCI_BEGINUNDOACTION);
  Send(SCI_CONVERTEOLS, mode);
  Send(SCI_SETEOLMODE, mode);
  Send(SCI_ENDUNDOACTION);
}

void Editor::Resize(int x, int y, int width, int height) {
  if (!m_hwnd) return;
  MoveWindow(m_hwnd, x, y, width, height, TRUE);
}

void Editor::SetFocus() {
  if (m_hwnd) ::SetFocus(m_hwnd);
}

void Editor::SetTabSettings(int width, bool use_spaces) {
  Send(SCI_SETTABWIDTH, static_cast<WPARAM>(width));
  Send(SCI_SETUSETABS, use_spaces ? 0 : 1);
}

void Editor::HandleCharAdded(int ch) {
  if (!m_auto_indent || ch != '\n') return;

  Sci_Position pos = Send(SCI_GETCURRENTPOS);
  Sci_Position line = Send(SCI_LINEFROMPOSITION, static_cast<WPARAM>(pos));
  if (line <= 0) return;

  Sci_Position prev_len = Send(SCI_LINELENGTH, static_cast<WPARAM>(line - 1));
  if (prev_len <= 0) return;
  std::string prev_line(static_cast<size_t>(prev_len), '\0');
  Send(SCI_GETLINE, static_cast<WPARAM>(line - 1), reinterpret_cast<LPARAM>(prev_line.data()));

  std::string indent;
  for (char c : prev_line) {
    if (c != ' ' && c != '\t') break;
    indent.push_back(c);
  }
  if (indent.empty()) return;

  Send(SCI_INSERTTEXT, static_cast<WPARAM>(pos), reinterpret_cast<LPARAM>(indent.c_str()));
  Send(SCI_GOTOPOS, static_cast<WPARAM>(pos) + indent.size());
}

LRESULT Editor::Send(UINT msg, WPARAM wParam, LPARAM lParam) const {
  return SendMessageW(m_hwnd, msg, wParam, lParam);
}

}  // namespace ep::win32
