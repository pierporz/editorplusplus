#include "platform/win32/find_replace.h"

#include <algorithm>

#include "platform/win32/resource.h"
#include "platform/win32/text_convert.h"
#include "third_party/scintilla/include/Scintilla.h"

namespace ep::win32 {

namespace {

struct Range {
  Sci_Position start;
  Sci_Position end;
};

Range RangeForScope(Editor& editor, int scope) {
  if (scope == 1) {
    return {editor.Send(SCI_GETSELECTIONSTART), editor.Send(SCI_GETSELECTIONEND)};
  }
  return {0, editor.Send(SCI_GETLENGTH)};
}

// Counts matches in `range`, or -- if `replacement` is non-null -- replaces
// them all as one undo action. Returns the number of matches processed.
int ProcessMatches(Editor& editor, Range range, const std::string& needle,
                    const std::string* replacement, int flags) {
  if (needle.empty()) return 0;
  editor.Send(SCI_SETSEARCHFLAGS, flags);

  int count = 0;
  Sci_Position pos = range.start;
  Sci_Position end = range.end;
  if (replacement) editor.Send(SCI_BEGINUNDOACTION);

  while (pos <= end) {
    editor.Send(SCI_SETTARGETSTART, static_cast<WPARAM>(pos));
    editor.Send(SCI_SETTARGETEND, static_cast<WPARAM>(end));
    Sci_Position found = editor.Send(SCI_SEARCHINTARGET, static_cast<WPARAM>(needle.size()),
                                      reinterpret_cast<LPARAM>(needle.c_str()));
    if (found < 0) break;
    Sci_Position match_start = editor.Send(SCI_GETTARGETSTART);
    Sci_Position match_end = editor.Send(SCI_GETTARGETEND);
    count++;

    if (replacement) {
      Sci_Position new_len;
      if (flags & SCFIND_REGEXP) {
        new_len = editor.Send(SCI_REPLACETARGETRE, static_cast<WPARAM>(-1),
                               reinterpret_cast<LPARAM>(replacement->c_str()));
      } else {
        new_len = editor.Send(SCI_REPLACETARGET, static_cast<WPARAM>(replacement->size()),
                               reinterpret_cast<LPARAM>(replacement->c_str()));
      }
      end += new_len - (match_end - match_start);
      pos = match_start + new_len;
    } else {
      pos = match_end > match_start ? match_end : match_end + 1;
    }
  }

  if (replacement) editor.Send(SCI_ENDUNDOACTION);
  return count;
}

}  // namespace

bool FindReplaceDialog::Create(HWND parent, HINSTANCE hInstance) {
  m_hwnd = CreateDialogParamW(hInstance, MAKEINTRESOURCEW(IDD_FIND), parent, DialogProcStatic,
                               reinterpret_cast<LPARAM>(this));
  return m_hwnd != nullptr;
}

void FindReplaceDialog::Show() {
  if (!m_hwnd) return;

  if (get_active_editor) {
    if (Editor* ed = get_active_editor()) {
      Sci_Position sel_start = ed->Send(SCI_GETSELECTIONSTART);
      Sci_Position sel_end = ed->Send(SCI_GETSELECTIONEND);
      if (sel_end > sel_start && sel_end - sel_start < 200) {
        // See the comment in text_tools.cpp's GetSelectionText: the length
        // query excludes the NUL, but the real call still appends one, so
        // the buffer must be sized len+1.
        Sci_Position len = ed->Send(SCI_GETSELTEXT, 0, 0);
        std::string buf(static_cast<size_t>(len) + 1, '\0');
        ed->Send(SCI_GETSELTEXT, 0, reinterpret_cast<LPARAM>(buf.data()));
        buf.resize(static_cast<size_t>(len));
        SetDlgItemTextW(m_hwnd, IDC_FIND_TEXT, Utf8ToWide(buf).c_str());
      }
    }
  }

  ShowWindow(m_hwnd, SW_SHOW);
  SetForegroundWindow(m_hwnd);
  SetFocus(GetDlgItem(m_hwnd, IDC_FIND_TEXT));
}

bool FindReplaceDialog::TranslateDialogMessage(MSG& msg) const {
  if (!m_hwnd || !IsWindowVisible(m_hwnd)) return false;
  return IsDialogMessageW(m_hwnd, &msg) != 0;
}

INT_PTR CALLBACK FindReplaceDialog::DialogProcStatic(HWND hwnd, UINT msg, WPARAM wParam,
                                                      LPARAM lParam) {
  FindReplaceDialog* self = nullptr;
  if (msg == WM_INITDIALOG) {
    self = reinterpret_cast<FindReplaceDialog*>(lParam);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->m_hwnd = hwnd;
  } else {
    self = reinterpret_cast<FindReplaceDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  }
  if (self) return self->DialogProc(hwnd, msg, wParam, lParam);
  return FALSE;
}

INT_PTR FindReplaceDialog::DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM /*lParam*/) {
  switch (msg) {
    case WM_INITDIALOG:
      OnInitDialog();
      return TRUE;
    case WM_COMMAND: {
      SyncOptionsFromControls();
      switch (LOWORD(wParam)) {
        case IDC_BTN_FINDNEXT:
          OnFindNext();
          return TRUE;
        case IDC_BTN_FINDPREV:
          OnFindPrevious();
          return TRUE;
        case IDC_BTN_COUNT:
          OnCount();
          return TRUE;
        case IDC_BTN_REPLACE:
          OnReplace();
          return TRUE;
        case IDC_BTN_REPLACEALL:
          OnReplaceAll();
          return TRUE;
        case IDCANCEL:
          ShowWindow(hwnd, SW_HIDE);
          return TRUE;
        default:
          return FALSE;
      }
    }
    case WM_CLOSE:
      ShowWindow(hwnd, SW_HIDE);
      return TRUE;
    default:
      return FALSE;
  }
}

void FindReplaceDialog::OnInitDialog() {
  CheckDlgButton(m_hwnd, IDC_MATCHCASE, match_case ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(m_hwnd, IDC_WHOLEWORD, whole_word ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(m_hwnd, IDC_REGEX, regex ? BST_CHECKED : BST_UNCHECKED);
  CheckDlgButton(m_hwnd, IDC_WRAPAROUND, wrap_around ? BST_CHECKED : BST_UNCHECKED);
  CheckRadioButton(m_hwnd, IDC_SCOPE_DOC, IDC_SCOPE_ALLTABS, IDC_SCOPE_DOC);
  RefreshHistoryCombo();
}

void FindReplaceDialog::SyncOptionsFromControls() {
  match_case = IsDlgButtonChecked(m_hwnd, IDC_MATCHCASE) == BST_CHECKED;
  whole_word = IsDlgButtonChecked(m_hwnd, IDC_WHOLEWORD) == BST_CHECKED;
  regex = IsDlgButtonChecked(m_hwnd, IDC_REGEX) == BST_CHECKED;
  wrap_around = IsDlgButtonChecked(m_hwnd, IDC_WRAPAROUND) == BST_CHECKED;
}

int FindReplaceDialog::Scope() const {
  if (IsDlgButtonChecked(m_hwnd, IDC_SCOPE_SEL) == BST_CHECKED) return 1;
  if (IsDlgButtonChecked(m_hwnd, IDC_SCOPE_ALLTABS) == BST_CHECKED) return 2;
  return 0;
}

int FindReplaceDialog::SearchFlags() const {
  int flags = 0;
  if (match_case) flags |= SCFIND_MATCHCASE;
  if (whole_word) flags |= SCFIND_WHOLEWORD;
  if (regex) flags |= SCFIND_REGEXP | SCFIND_CXX11REGEX;
  return flags;
}

std::string FindReplaceDialog::GetFindText() const {
  wchar_t buf[512];
  GetDlgItemTextW(m_hwnd, IDC_FIND_TEXT, buf, 512);
  return WideToUtf8(buf);
}

std::string FindReplaceDialog::GetReplaceText() const {
  wchar_t buf[512];
  GetDlgItemTextW(m_hwnd, IDC_REPLACE_TEXT, buf, 512);
  return WideToUtf8(buf);
}

void FindReplaceDialog::RememberSearch(const std::string& text) {
  if (text.empty()) return;
  auto it = std::find(history.begin(), history.end(), text);
  if (it != history.end()) history.erase(it);
  history.insert(history.begin(), text);
  if (history.size() > 20) history.resize(20);
  RefreshHistoryCombo();
}

void FindReplaceDialog::RefreshHistoryCombo() {
  HWND combo = GetDlgItem(m_hwnd, IDC_FIND_TEXT);
  wchar_t current[512];
  GetWindowTextW(combo, current, 512);
  SendMessageW(combo, CB_RESETCONTENT, 0, 0);
  for (const auto& h : history) {
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(h).c_str()));
  }
  SetWindowTextW(combo, current);
}

void FindReplaceDialog::SetStatus(const std::string& text) {
  if (m_hwnd) SetDlgItemTextW(m_hwnd, IDC_FIND_STATUS, Utf8ToWide(text).c_str());
  if (show_status) show_status(text);
}

bool FindReplaceDialog::DoFind(Editor& editor, bool forward) {
  std::string needle = GetFindText();
  if (needle.empty()) return false;
  RememberSearch(needle);

  editor.Send(SCI_SETSEARCHFLAGS, SearchFlags());
  Sci_Position doc_len = editor.Send(SCI_GETLENGTH);
  Sci_Position sel_start = editor.Send(SCI_GETSELECTIONSTART);
  Sci_Position sel_end = editor.Send(SCI_GETSELECTIONEND);
  Sci_Position anchor = forward ? sel_end : sel_start;

  auto try_range = [&](Sci_Position start, Sci_Position end) {
    editor.Send(SCI_SETTARGETSTART, static_cast<WPARAM>(start));
    editor.Send(SCI_SETTARGETEND, static_cast<WPARAM>(end));
    Sci_Position pos = editor.Send(SCI_SEARCHINTARGET, static_cast<WPARAM>(needle.size()),
                                    reinterpret_cast<LPARAM>(needle.c_str()));
    if (pos < 0) return false;
    editor.Send(SCI_SETSEL, static_cast<WPARAM>(editor.Send(SCI_GETTARGETSTART)),
                static_cast<LPARAM>(editor.Send(SCI_GETTARGETEND)));
    editor.Send(SCI_SCROLLCARET);
    return true;
  };

  if (forward) {
    if (try_range(anchor, doc_len)) return true;
    if (wrap_around && try_range(0, anchor)) return true;
  } else {
    if (try_range(anchor, 0)) return true;
    if (wrap_around && try_range(doc_len, anchor)) return true;
  }
  return false;
}

void FindReplaceDialog::OnFindNext() { FindNext(); }
void FindReplaceDialog::OnFindPrevious() { FindPrevious(); }

void FindReplaceDialog::FindNext() {
  if (!m_hwnd || !get_active_editor) return;
  Editor* ed = get_active_editor();
  if (!ed) return;
  SetStatus(DoFind(*ed, true) ? "Found" : "Not found");
}

void FindReplaceDialog::FindPrevious() {
  if (!m_hwnd || !get_active_editor) return;
  Editor* ed = get_active_editor();
  if (!ed) return;
  SetStatus(DoFind(*ed, false) ? "Found" : "Not found");
}

void FindReplaceDialog::OnCount() {
  std::string needle = GetFindText();
  if (needle.empty()) return;
  RememberSearch(needle);

  int scope = Scope();
  int total = 0;
  if (scope == 2 && get_all_editors) {
    for (Editor* ed : get_all_editors()) {
      total += ProcessMatches(*ed, RangeForScope(*ed, 0), needle, nullptr, SearchFlags());
    }
  } else if (get_active_editor) {
    if (Editor* ed = get_active_editor()) {
      total = ProcessMatches(*ed, RangeForScope(*ed, scope), needle, nullptr, SearchFlags());
    }
  }
  SetStatus(std::to_string(total) + " occurrence(s)");
}

void FindReplaceDialog::OnReplace() {
  if (!get_active_editor) return;
  Editor* ed = get_active_editor();
  if (!ed) return;
  std::string needle = GetFindText();
  if (needle.empty()) return;
  std::string replacement = GetReplaceText();

  Sci_Position sel_start = ed->Send(SCI_GETSELECTIONSTART);
  Sci_Position sel_end = ed->Send(SCI_GETSELECTIONEND);
  if (sel_end > sel_start) {
    ed->Send(SCI_SETSEARCHFLAGS, SearchFlags());
    ed->Send(SCI_SETTARGETSTART, static_cast<WPARAM>(sel_start));
    ed->Send(SCI_SETTARGETEND, static_cast<WPARAM>(sel_end));
    Sci_Position found = ed->Send(SCI_SEARCHINTARGET, static_cast<WPARAM>(needle.size()),
                                   reinterpret_cast<LPARAM>(needle.c_str()));
    if (found == sel_start && ed->Send(SCI_GETTARGETEND) == sel_end) {
      ed->Send(SCI_BEGINUNDOACTION);
      if (SearchFlags() & SCFIND_REGEXP) {
        ed->Send(SCI_REPLACETARGETRE, static_cast<WPARAM>(-1),
                 reinterpret_cast<LPARAM>(replacement.c_str()));
      } else {
        ed->Send(SCI_REPLACETARGET, static_cast<WPARAM>(replacement.size()),
                 reinterpret_cast<LPARAM>(replacement.c_str()));
      }
      ed->Send(SCI_ENDUNDOACTION);
    }
  }
  DoFind(*ed, true);
}

void FindReplaceDialog::OnReplaceAll() {
  std::string needle = GetFindText();
  if (needle.empty()) return;
  RememberSearch(needle);
  std::string replacement = GetReplaceText();

  int scope = Scope();
  int total = 0;
  if (scope == 2 && get_all_editors) {
    for (Editor* ed : get_all_editors()) {
      total += ProcessMatches(*ed, RangeForScope(*ed, 0), needle, &replacement, SearchFlags());
    }
  } else if (get_active_editor) {
    if (Editor* ed = get_active_editor()) {
      total = ProcessMatches(*ed, RangeForScope(*ed, scope), needle, &replacement, SearchFlags());
    }
  }
  SetStatus(std::to_string(total) + " replacement(s)");
}

}  // namespace ep::win32
