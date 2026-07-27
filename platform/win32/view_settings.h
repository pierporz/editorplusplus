#pragma once

#include "platform/win32/editor.h"

namespace ep::win32 {

// Global (app-wide, not per-tab) view preferences -- see the [view] section
// of editor++.ini.
struct ViewSettings {
  bool word_wrap = false;
  bool show_whitespace = false;
  bool line_numbers = true;
  bool indent_guides = false;
  bool current_line_highlight = true;
  bool dark_theme = false;
};

// Pushes the full current settings onto one editor -- call on every tab
// when it's created/restored, and on every open tab after a toggle.
void ApplyViewSettings(Editor& editor, const ViewSettings& settings);

}  // namespace ep::win32
