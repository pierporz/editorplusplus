#pragma once

#include <windows.h>

#include <string>

#include "core/encoding.h"

namespace ep::win32 {

// Thin wrapper around one Scintilla child window (one instance per document).
class Editor {
 public:
  bool Create(HWND parent, HINSTANCE hInstance, int controlId);

  HWND Handle() const { return m_hwnd; }

  // Content is always UTF-8: the control's codepage is set to SC_CP_UTF8 in
  // Create(), so Scintilla's byte buffer already *is* UTF-8. No wide-string
  // conversion happens for document content, ever.
  std::string GetText() const;
  void SetText(const std::string& utf8);

  bool IsDirty() const;
  void MarkSaved();  // SCI_SETSAVEPOINT

  // Sets Scintilla's EOL mode (what pressing Enter inserts) without
  // touching existing line endings already in the buffer.
  void SetEolMode(ep::Eol eol);
  // Converts every existing line ending in the buffer to `eol`, as one
  // undoable action.
  void ConvertEolTo(ep::Eol eol);

  void Resize(int x, int y, int width, int height);
  void SetFocus();

  void SetTabSettings(int width, bool use_spaces);
  void SetAutoIndent(bool enabled) { m_auto_indent = enabled; }
  // Call from the SCN_CHARADDED handler; copies the previous line's leading
  // whitespace onto a freshly-started line when auto-indent is on.
  void HandleCharAdded(int ch);

  LRESULT Send(UINT msg, WPARAM wParam = 0, LPARAM lParam = 0) const;

 private:
  HWND m_hwnd = nullptr;
  bool m_auto_indent = true;
};

}  // namespace ep::win32
