#include "platform/win32/syntax_highlight.h"

#include "ILexer.h"
#include "Lexilla.h"
#include "SciLexer.h"
#include "Scintilla.h"

namespace ep::win32 {

namespace {

std::string Extension(const std::string& path) {
  size_t slash = path.find_last_of("/\\");
  size_t dot = path.find_last_of('.');
  if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return "";
  std::string ext = path.substr(dot + 1);
  for (char& c : ext) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
  }
  return ext;
}

void SetFore(Editor& editor, int style, COLORREF color) {
  editor.Send(SCI_STYLESETFORE, static_cast<WPARAM>(style), static_cast<LPARAM>(color));
}

void SetBold(Editor& editor, int style) {
  editor.Send(SCI_STYLESETBOLD, static_cast<WPARAM>(style), 1);
}

constexpr COLORREF kComment = RGB(0, 128, 0);
constexpr COLORREF kString = RGB(163, 21, 21);
constexpr COLORREF kNumber = RGB(9, 106, 133);
constexpr COLORREF kKeyword = RGB(0, 0, 200);
constexpr COLORREF kTag = RGB(0, 0, 160);
constexpr COLORREF kAttribute = RGB(153, 69, 0);
constexpr COLORREF kPreprocessor = RGB(128, 0, 128);

void ApplyCpp(Editor& editor) {
  editor.Send(SCI_STYLESETFORE, SCE_C_COMMENT, kComment);
  editor.Send(SCI_STYLESETFORE, SCE_C_COMMENTLINE, kComment);
  editor.Send(SCI_STYLESETFORE, SCE_C_COMMENTDOC, kComment);
  SetFore(editor, SCE_C_STRING, kString);
  SetFore(editor, SCE_C_CHARACTER, kString);
  SetFore(editor, SCE_C_NUMBER, kNumber);
  SetFore(editor, SCE_C_WORD, kKeyword);
  SetBold(editor, SCE_C_WORD);
  SetFore(editor, SCE_C_WORD2, kKeyword);
  SetFore(editor, SCE_C_PREPROCESSOR, kPreprocessor);
  const char* keywords =
      "if else for while do switch case default break continue return goto "
      "class struct union enum namespace using public private protected "
      "virtual override static const constexpr volatile mutable friend "
      "template typename this new delete try catch throw sizeof typedef "
      "void int char short long float double bool auto unsigned signed "
      "true false nullptr null NULL "
      "function var let const in of typeof instanceof new delete export "
      "import from async await yield undefined";
  editor.Send(SCI_SETKEYWORDS, 0, reinterpret_cast<LPARAM>(keywords));
}

void ApplyPython(Editor& editor) {
  SetFore(editor, SCE_P_COMMENTLINE, kComment);
  SetFore(editor, SCE_P_COMMENTBLOCK, kComment);
  SetFore(editor, SCE_P_STRING, kString);
  SetFore(editor, SCE_P_CHARACTER, kString);
  SetFore(editor, SCE_P_TRIPLE, kString);
  SetFore(editor, SCE_P_TRIPLEDOUBLE, kString);
  SetFore(editor, SCE_P_NUMBER, kNumber);
  SetFore(editor, SCE_P_WORD, kKeyword);
  SetBold(editor, SCE_P_WORD);
  SetFore(editor, SCE_P_CLASSNAME, kKeyword);
  SetFore(editor, SCE_P_DEFNAME, kKeyword);
  const char* keywords =
      "False None True and as assert async await break class continue def "
      "del elif else except finally for from global if import in is lambda "
      "nonlocal not or pass raise return try while with yield self";
  editor.Send(SCI_SETKEYWORDS, 0, reinterpret_cast<LPARAM>(keywords));
}

void ApplyBash(Editor& editor) {
  SetFore(editor, SCE_SH_COMMENTLINE, kComment);
  SetFore(editor, SCE_SH_STRING, kString);
  SetFore(editor, SCE_SH_CHARACTER, kString);
  SetFore(editor, SCE_SH_NUMBER, kNumber);
  SetFore(editor, SCE_SH_WORD, kKeyword);
  SetBold(editor, SCE_SH_WORD);
  SetFore(editor, SCE_SH_SCALAR, kPreprocessor);
  SetFore(editor, SCE_SH_PARAM, kPreprocessor);
  const char* keywords =
      "if then else elif fi for while do done case esac function in "
      "select until time break continue return exit export local readonly "
      "shift trap unset echo";
  editor.Send(SCI_SETKEYWORDS, 0, reinterpret_cast<LPARAM>(keywords));
}

void ApplySql(Editor& editor) {
  SetFore(editor, SCE_SQL_COMMENT, kComment);
  SetFore(editor, SCE_SQL_COMMENTLINE, kComment);
  SetFore(editor, SCE_SQL_COMMENTDOC, kComment);
  SetFore(editor, SCE_SQL_STRING, kString);
  SetFore(editor, SCE_SQL_CHARACTER, kString);
  SetFore(editor, SCE_SQL_NUMBER, kNumber);
  SetFore(editor, SCE_SQL_WORD, kKeyword);
  SetBold(editor, SCE_SQL_WORD);
  const char* keywords =
      "select insert update delete from where join inner outer left right "
      "on group by order having limit offset as into values set create "
      "table alter drop index view trigger primary key foreign references "
      "not null default unique and or in like between is distinct union "
      "all case when then else end begin commit rollback transaction";
  editor.Send(SCI_SETKEYWORDS, 0, reinterpret_cast<LPARAM>(keywords));
}

void ApplyJson(Editor& editor) {
  SetFore(editor, SCE_JSON_STRING, kString);
  SetFore(editor, SCE_JSON_NUMBER, kNumber);
  SetFore(editor, SCE_JSON_PROPERTYNAME, kAttribute);
  SetFore(editor, SCE_JSON_KEYWORD, kKeyword);
  SetBold(editor, SCE_JSON_KEYWORD);
  SetFore(editor, SCE_JSON_LINECOMMENT, kComment);
  SetFore(editor, SCE_JSON_BLOCKCOMMENT, kComment);
  SetFore(editor, SCE_JSON_ERROR, RGB(255, 0, 0));
}

void ApplyMarkup(Editor& editor) {
  SetFore(editor, SCE_H_TAG, kTag);
  SetFore(editor, SCE_H_TAGEND, kTag);
  SetFore(editor, SCE_H_TAGUNKNOWN, kTag);
  SetFore(editor, SCE_H_ATTRIBUTE, kAttribute);
  SetFore(editor, SCE_H_ATTRIBUTEUNKNOWN, kAttribute);
  SetFore(editor, SCE_H_DOUBLESTRING, kString);
  SetFore(editor, SCE_H_SINGLESTRING, kString);
  SetFore(editor, SCE_H_NUMBER, kNumber);
  SetFore(editor, SCE_H_COMMENT, kComment);
  SetFore(editor, SCE_H_CDATA, kPreprocessor);
}

void ApplyMarkdown(Editor& editor) {
  SetFore(editor, SCE_MARKDOWN_HEADER1, kKeyword);
  SetFore(editor, SCE_MARKDOWN_HEADER2, kKeyword);
  SetFore(editor, SCE_MARKDOWN_HEADER3, kKeyword);
  SetFore(editor, SCE_MARKDOWN_HEADER4, kKeyword);
  SetFore(editor, SCE_MARKDOWN_HEADER5, kKeyword);
  SetFore(editor, SCE_MARKDOWN_HEADER6, kKeyword);
  SetBold(editor, SCE_MARKDOWN_STRONG1);
  SetBold(editor, SCE_MARKDOWN_STRONG2);
  SetFore(editor, SCE_MARKDOWN_LINK, kAttribute);
  SetFore(editor, SCE_MARKDOWN_CODE, kString);
  SetFore(editor, SCE_MARKDOWN_CODE2, kString);
  SetFore(editor, SCE_MARKDOWN_CODEBK, kString);
  SetFore(editor, SCE_MARKDOWN_BLOCKQUOTE, kComment);
}

void ApplyProps(Editor& editor) {
  SetFore(editor, SCE_PROPS_COMMENT, kComment);
  SetFore(editor, SCE_PROPS_SECTION, kKeyword);
  SetBold(editor, SCE_PROPS_SECTION);
  SetFore(editor, SCE_PROPS_KEY, kAttribute);
  SetFore(editor, SCE_PROPS_ASSIGNMENT, kPreprocessor);
  SetFore(editor, SCE_PROPS_DEFVAL, kString);
}

}  // namespace

std::string DetectLanguageForPath(const std::string& utf8_path) {
  std::string ext = Extension(utf8_path);
  if (ext == "c" || ext == "h" || ext == "cpp" || ext == "cc" || ext == "cxx" ||
      ext == "hpp" || ext == "hh" || ext == "js" || ext == "mjs" || ext == "ts") {
    return "cpp";
  }
  if (ext == "py" || ext == "pyw") return "python";
  if (ext == "sh" || ext == "bash") return "bash";
  if (ext == "sql") return "sql";
  if (ext == "json") return "json";
  if (ext == "xml") return "xml";
  if (ext == "html" || ext == "htm") return "hypertext";
  if (ext == "md" || ext == "markdown") return "markdown";
  if (ext == "ini" || ext == "cfg" || ext == "conf") return "props";
  return "";
}

void ApplyLanguage(Editor& editor, const std::string& language) {
  editor.Send(SCI_STYLESETFONT, STYLE_DEFAULT, reinterpret_cast<LPARAM>("Consolas"));
  editor.Send(SCI_STYLESETSIZE, STYLE_DEFAULT, 11);
  editor.Send(SCI_STYLECLEARALL);

  if (language.empty()) {
    editor.Send(SCI_SETILEXER, 0, 0);
    return;
  }

  Scintilla::ILexer5* lexer = CreateLexer(language.c_str());
  if (!lexer) return;
  editor.Send(SCI_SETILEXER, 0, reinterpret_cast<LPARAM>(lexer));

  if (language == "cpp") {
    ApplyCpp(editor);
  } else if (language == "python") {
    ApplyPython(editor);
  } else if (language == "bash") {
    ApplyBash(editor);
  } else if (language == "sql") {
    ApplySql(editor);
  } else if (language == "json") {
    ApplyJson(editor);
  } else if (language == "xml" || language == "hypertext") {
    ApplyMarkup(editor);
  } else if (language == "markdown") {
    ApplyMarkdown(editor);
  } else if (language == "props") {
    ApplyProps(editor);
  }

  editor.Send(SCI_COLOURISE, 0, -1);
}

}  // namespace ep::win32
