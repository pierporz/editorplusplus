#include "platform/win32/app_config.h"

#include "core/ini.h"
#include "platform/win32/app_paths.h"
#include "platform/win32/file_io.h"

namespace ep::win32 {

AppConfig LoadAppConfig() {
  AppConfig config;

  auto text = ReadFileBytes(ConfigIniPath());
  if (!text) return config;  // no config yet -- defaults

  ep::IniDocument doc = ep::IniDocument::Parse(text.Value());

  config.window.x = doc.GetInt("window", "x", config.window.x);
  config.window.y = doc.GetInt("window", "y", config.window.y);
  config.window.width = doc.GetInt("window", "width", config.window.width);
  config.window.height = doc.GetInt("window", "height", config.window.height);
  config.window.maximized = doc.GetBool("window", "maximized", config.window.maximized);

  config.view.word_wrap = doc.GetBool("view", "word_wrap", config.view.word_wrap);
  config.view.line_numbers = doc.GetBool("view", "line_numbers", config.view.line_numbers);
  config.view.show_whitespace =
      doc.GetBool("view", "show_whitespace", config.view.show_whitespace);
  config.view.indent_guides = doc.GetBool("view", "indent_guides", config.view.indent_guides);
  config.view.current_line_highlight =
      doc.GetBool("view", "current_line_highlight", config.view.current_line_highlight);
  config.view.dark_theme = doc.Get("view", "theme", "light") == "dark";

  config.editor.tab_width = doc.GetInt("editor", "tab_width", config.editor.tab_width);
  config.editor.use_spaces = doc.GetBool("editor", "use_spaces", config.editor.use_spaces);
  config.editor.auto_indent = doc.GetBool("editor", "auto_indent", config.editor.auto_indent);

  config.find.match_case = doc.GetBool("find", "match_case", config.find.match_case);
  config.find.whole_word = doc.GetBool("find", "whole_word", config.find.whole_word);
  config.find.regex = doc.GetBool("find", "regex", config.find.regex);
  config.find.wrap_around = doc.GetBool("find", "wrap_around", config.find.wrap_around);
  for (int i = 0; i < 20; i++) {
    std::string entry = doc.Get("find", "history" + std::to_string(i), "");
    if (entry.empty()) break;
    config.find.history.push_back(entry);
  }

  for (int i = 0; i < 10; i++) {
    std::string entry = doc.Get("recent", "file" + std::to_string(i), "");
    if (entry.empty()) break;
    config.recent_files.push_back(entry);
  }

  if (config.window.width < 200) config.window.width = 200;
  if (config.window.height < 150) config.window.height = 150;
  if (config.editor.tab_width < 1) config.editor.tab_width = 1;

  return config;
}

void SaveAppConfig(const AppConfig& config) {
  ep::IniDocument doc;

  doc.SetInt("window", "x", config.window.x);
  doc.SetInt("window", "y", config.window.y);
  doc.SetInt("window", "width", config.window.width);
  doc.SetInt("window", "height", config.window.height);
  doc.SetBool("window", "maximized", config.window.maximized);

  doc.SetBool("view", "word_wrap", config.view.word_wrap);
  doc.SetBool("view", "line_numbers", config.view.line_numbers);
  doc.SetBool("view", "show_whitespace", config.view.show_whitespace);
  doc.SetBool("view", "indent_guides", config.view.indent_guides);
  doc.SetBool("view", "current_line_highlight", config.view.current_line_highlight);
  doc.Set("view", "font", "Consolas");
  doc.SetInt("view", "font_size", 11);
  doc.Set("view", "theme", config.view.dark_theme ? "dark" : "light");

  doc.SetInt("editor", "tab_width", config.editor.tab_width);
  doc.SetBool("editor", "use_spaces", config.editor.use_spaces);
  doc.SetBool("editor", "auto_indent", config.editor.auto_indent);

  doc.SetBool("find", "match_case", config.find.match_case);
  doc.SetBool("find", "whole_word", config.find.whole_word);
  doc.SetBool("find", "regex", config.find.regex);
  doc.SetBool("find", "wrap_around", config.find.wrap_around);
  for (size_t i = 0; i < config.find.history.size() && i < 20; i++) {
    doc.Set("find", "history" + std::to_string(i), config.find.history[i]);
  }

  for (size_t i = 0; i < config.recent_files.size() && i < 10; i++) {
    doc.Set("recent", "file" + std::to_string(i), config.recent_files[i]);
  }

  WriteFileAtomic(ConfigIniPath(), doc.Serialize());
}

}  // namespace ep::win32
