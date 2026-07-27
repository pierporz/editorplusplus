#include "platform/win32/text_tools.h"

#include "core/base64.h"
#include "core/encoding.h"
#include "core/json_pretty.h"
#include "core/sql_pretty.h"
#include "core/xml_pretty.h"
#include "third_party/scintilla/include/Scintilla.h"

namespace ep::win32::tools {

namespace {

bool HasSelection(Editor& editor) {
  return editor.Send(SCI_GETSELECTIONSTART) != editor.Send(SCI_GETSELECTIONEND);
}

std::string GetSelectionText(Editor& editor) {
  // SCI_GETSELTEXT's length query returns the text length only (unlike
  // SCI_GETTEXT it never counts the NUL); the real call then always writes
  // that many bytes plus its own terminating NUL regardless of buffer size,
  // so the buffer must be sized len+1 or Scintilla overflows it by one byte.
  Sci_Position len = editor.Send(SCI_GETSELTEXT, 0, 0);
  if (len <= 0) return std::string();
  std::string buf(static_cast<size_t>(len) + 1, '\0');
  editor.Send(SCI_GETSELTEXT, 0, reinterpret_cast<LPARAM>(buf.data()));
  buf.resize(static_cast<size_t>(len));
  return buf;
}

// Replaces the selection (if any) or the whole document with `new_text` as
// one undo action, keeping the current scroll position stable. Goes through
// SCI_REPLACETARGET (which takes an explicit length) rather than
// SCI_REPLACESEL (which relies on strlen()) so an embedded NUL byte in
// decoded content -- entirely possible from Base64 Decode -- can't silently
// truncate the result.
void ReplaceActiveText(Editor& editor, bool has_selection, const std::string& new_text) {
  auto scroll = editor.Send(SCI_GETFIRSTVISIBLELINE);
  Sci_Position target_start = has_selection ? editor.Send(SCI_GETSELECTIONSTART) : 0;
  Sci_Position target_end =
      has_selection ? editor.Send(SCI_GETSELECTIONEND) : editor.Send(SCI_GETLENGTH);

  editor.Send(SCI_BEGINUNDOACTION);
  editor.Send(SCI_SETTARGETSTART, static_cast<WPARAM>(target_start));
  editor.Send(SCI_SETTARGETEND, static_cast<WPARAM>(target_end));
  editor.Send(SCI_REPLACETARGET, static_cast<WPARAM>(new_text.size()),
              reinterpret_cast<LPARAM>(new_text.data()));
  editor.Send(SCI_ENDUNDOACTION);
  editor.Send(SCI_SETFIRSTVISIBLELINE, scroll);
}

std::string ActiveInput(Editor& editor, bool has_selection) {
  return has_selection ? GetSelectionText(editor) : editor.GetText();
}

}  // namespace

std::string JsonPretty(Editor& editor) {
  bool has_sel = HasSelection(editor);
  ep::JsonFormatResult result = ep::JsonPrettyPrint(ActiveInput(editor, has_sel));
  if (!result.ok) {
    return "JSON error at line " + std::to_string(result.error.line) + ", col " +
           std::to_string(result.error.column) + ": " + result.error.message;
  }
  ReplaceActiveText(editor, has_sel, result.output);
  return "";
}

std::string JsonMinify(Editor& editor) {
  bool has_sel = HasSelection(editor);
  ep::JsonFormatResult result = ep::JsonMinify(ActiveInput(editor, has_sel));
  if (!result.ok) {
    return "JSON error at line " + std::to_string(result.error.line) + ", col " +
           std::to_string(result.error.column) + ": " + result.error.message;
  }
  ReplaceActiveText(editor, has_sel, result.output);
  return "";
}

std::string XmlPretty(Editor& editor) {
  bool has_sel = HasSelection(editor);
  std::string output = ep::XmlPrettyPrint(ActiveInput(editor, has_sel));
  ReplaceActiveText(editor, has_sel, output);
  return "";
}

std::string SqlPretty(Editor& editor) {
  bool has_sel = HasSelection(editor);
  std::string output = ep::SqlPretty(ActiveInput(editor, has_sel));
  ReplaceActiveText(editor, has_sel, output);
  return "";
}

std::string Base64Encode(Editor& editor) {
  bool has_sel = HasSelection(editor);
  std::string output = ep::Base64Encode(ActiveInput(editor, has_sel));
  ReplaceActiveText(editor, has_sel, output);
  return "";
}

std::string Base64Decode(Editor& editor) {
  bool has_sel = HasSelection(editor);
  ep::Result<std::string> result = ep::Base64Decode(ActiveInput(editor, has_sel));
  if (!result) return "Base64 decode error: " + result.Err().message;
  if (!ep::IsValidUtf8(result.Value())) {
    return "Base64 decode produced invalid UTF-8 -- document left unchanged";
  }
  ReplaceActiveText(editor, has_sel, result.Value());
  return "";
}

}  // namespace ep::win32::tools
