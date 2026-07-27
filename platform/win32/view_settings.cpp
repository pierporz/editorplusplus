#include "platform/win32/view_settings.h"

#include "third_party/scintilla/include/Scintilla.h"

namespace ep::win32 {

void ApplyViewSettings(Editor& editor, const ViewSettings& settings) {
  editor.Send(SCI_SETWRAPMODE, settings.word_wrap ? SC_WRAP_WORD : SC_WRAP_NONE);

  int ws_mode = settings.show_whitespace ? SCWS_VISIBLEALWAYS : SCWS_INVISIBLE;
  editor.Send(SCI_SETVIEWWS, ws_mode);
  editor.Send(SCI_SETVIEWEOL, settings.show_whitespace ? 1 : 0);

  editor.Send(SCI_SETMARGINWIDTHN, 0, settings.line_numbers ? 40 : 0);

  editor.Send(SCI_SETINDENTATIONGUIDES, settings.indent_guides ? SC_IV_LOOKBOTH : SC_IV_NONE);

  editor.Send(SCI_SETCARETLINEVISIBLE, settings.current_line_highlight ? 1 : 0);

  // Only the background/default-text/margin/caret colors flip for dark mode;
  // per-language syntax colors (see syntax_highlight.cpp) stay the same --
  // callers must re-apply the language afterwards for the change to reach
  // token styles too (SCI_STYLECLEARALL there re-propagates from
  // STYLE_DEFAULT, which is why the ordering matters).
  if (settings.dark_theme) {
    editor.Send(SCI_STYLESETBACK, STYLE_DEFAULT, RGB(30, 30, 30));
    editor.Send(SCI_STYLESETFORE, STYLE_DEFAULT, RGB(220, 220, 220));
    editor.Send(SCI_STYLESETBACK, STYLE_LINENUMBER, RGB(40, 40, 40));
    editor.Send(SCI_STYLESETFORE, STYLE_LINENUMBER, RGB(150, 150, 150));
    editor.Send(SCI_SETCARETFORE, RGB(255, 255, 255));
    editor.Send(SCI_SETSELBACK, 1, RGB(65, 65, 100));
    editor.Send(SCI_SETCARETLINEBACK, RGB(45, 45, 45));
  } else {
    editor.Send(SCI_STYLESETBACK, STYLE_DEFAULT, RGB(255, 255, 255));
    editor.Send(SCI_STYLESETFORE, STYLE_DEFAULT, RGB(0, 0, 0));
    editor.Send(SCI_STYLESETBACK, STYLE_LINENUMBER, RGB(240, 240, 240));
    editor.Send(SCI_STYLESETFORE, STYLE_LINENUMBER, RGB(100, 100, 100));
    editor.Send(SCI_SETCARETFORE, RGB(0, 0, 0));
    editor.Send(SCI_SETSELBACK, 1, RGB(173, 214, 255));
    editor.Send(SCI_SETCARETLINEBACK, RGB(240, 240, 240));
  }
}

}  // namespace ep::win32
