#pragma once

#include <string>
#include <vector>

#include "platform/win32/view_settings.h"

namespace ep::win32 {

struct WindowGeometry {
  int x = 100;
  int y = 100;
  int width = 1200;
  int height = 800;
  bool maximized = false;
};

struct EditorSettings {
  int tab_width = 4;
  bool use_spaces = false;
  bool auto_indent = true;
};

struct FindSettings {
  bool match_case = false;
  bool whole_word = false;
  bool regex = false;
  bool wrap_around = true;
  std::vector<std::string> history;  // most recent first, max 20
};

// Mirrors the editor++.ini layout documented in CLAUDE.md section 7.
struct AppConfig {
  WindowGeometry window;
  ViewSettings view;
  EditorSettings editor;
  FindSettings find;
  std::vector<std::string> recent_files;  // most recent first, max 10
};

// Never fails: a missing/corrupt editor++.ini just yields defaults.
AppConfig LoadAppConfig();
void SaveAppConfig(const AppConfig& config);

}  // namespace ep::win32
