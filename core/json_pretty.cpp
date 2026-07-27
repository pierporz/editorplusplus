#include "core/json_pretty.h"

#include <vector>

namespace ep {

namespace {

enum class TokType {
  LBrace,
  RBrace,
  LBracket,
  RBracket,
  Colon,
  Comma,
  String,
  Number,
  True,
  False,
  Null,
  End,
  Error,
};

struct Token {
  TokType type = TokType::Error;
  std::string text;  // raw source slice for String/Number; message for Error
  size_t line = 1;
  size_t col = 1;
};

// Iterative, single-pass, on-demand tokenizer. Never recurses, so document
// nesting depth cannot overflow the call stack here -- see also the parser
// below, which walks an explicit heap stack instead of recursing.
class Lexer {
 public:
  explicit Lexer(const std::string& src) : m_src(src) {}

  Token Next() {
    SkipWhitespace();
    if (m_pos >= m_src.size()) return Make(TokType::End, "");

    size_t start_line = m_line;
    size_t start_col = m_col;
    char c = m_src[m_pos];

    switch (c) {
      case '{':
        Advance();
        return MakeAt(TokType::LBrace, "{", start_line, start_col);
      case '}':
        Advance();
        return MakeAt(TokType::RBrace, "}", start_line, start_col);
      case '[':
        Advance();
        return MakeAt(TokType::LBracket, "[", start_line, start_col);
      case ']':
        Advance();
        return MakeAt(TokType::RBracket, "]", start_line, start_col);
      case ':':
        Advance();
        return MakeAt(TokType::Colon, ":", start_line, start_col);
      case ',':
        Advance();
        return MakeAt(TokType::Comma, ",", start_line, start_col);
      case '"':
        return ScanString(start_line, start_col);
      case 't':
        return ScanLiteral("true", TokType::True, start_line, start_col);
      case 'f':
        return ScanLiteral("false", TokType::False, start_line, start_col);
      case 'n':
        return ScanLiteral("null", TokType::Null, start_line, start_col);
      default:
        if (c == '-' || (c >= '0' && c <= '9')) {
          return ScanNumber(start_line, start_col);
        }
        Advance();
        return MakeAt(TokType::Error, "unexpected character", start_line,
                       start_col);
    }
  }

 private:
  const std::string& m_src;
  size_t m_pos = 0;
  size_t m_line = 1;
  size_t m_col = 1;

  void Advance() {
    if (m_pos >= m_src.size()) return;
    if (m_src[m_pos] == '\n') {
      m_line++;
      m_col = 1;
    } else {
      m_col++;
    }
    m_pos++;
  }

  void SkipWhitespace() {
    while (m_pos < m_src.size()) {
      char c = m_src[m_pos];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        Advance();
      } else {
        break;
      }
    }
  }

  static Token MakeAt(TokType type, std::string text, size_t line,
                       size_t col) {
    Token t;
    t.type = type;
    t.text = std::move(text);
    t.line = line;
    t.col = col;
    return t;
  }

  Token Make(TokType type, std::string text) {
    return MakeAt(type, std::move(text), m_line, m_col);
  }

  Token ScanLiteral(const char* literal, TokType type, size_t start_line,
                     size_t start_col) {
    size_t len = 0;
    while (literal[len] != '\0') len++;
    if (m_src.compare(m_pos, len, literal) != 0) {
      Advance();
      return MakeAt(TokType::Error, "invalid literal", start_line, start_col);
    }
    for (size_t i = 0; i < len; i++) Advance();
    return MakeAt(type, literal, start_line, start_col);
  }

  Token ScanString(size_t start_line, size_t start_col) {
    size_t begin = m_pos;
    Advance();  // opening quote
    while (true) {
      if (m_pos >= m_src.size()) {
        return MakeAt(TokType::Error, "unterminated string", start_line,
                       start_col);
      }
      unsigned char c = static_cast<unsigned char>(m_src[m_pos]);
      if (c == '"') {
        Advance();
        break;
      }
      if (c < 0x20) {
        return MakeAt(TokType::Error, "control character in string", m_line,
                       m_col);
      }
      if (c == '\\') {
        size_t esc_line = m_line, esc_col = m_col;
        Advance();
        if (m_pos >= m_src.size()) {
          return MakeAt(TokType::Error, "unterminated escape", esc_line,
                         esc_col);
        }
        char e = m_src[m_pos];
        switch (e) {
          case '"':
          case '\\':
          case '/':
          case 'b':
          case 'f':
          case 'n':
          case 'r':
          case 't':
            Advance();
            break;
          case 'u': {
            Advance();
            for (int i = 0; i < 4; i++) {
              if (m_pos >= m_src.size() || !IsHexDigit(m_src[m_pos])) {
                return MakeAt(TokType::Error, "invalid unicode escape",
                               esc_line, esc_col);
              }
              Advance();
            }
            break;
          }
          default:
            return MakeAt(TokType::Error, "invalid escape sequence", esc_line,
                           esc_col);
        }
      } else {
        Advance();
      }
    }
    return MakeAt(TokType::String, m_src.substr(begin, m_pos - begin),
                   start_line, start_col);
  }

  static bool IsHexDigit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
  }

  Token ScanNumber(size_t start_line, size_t start_col) {
    size_t begin = m_pos;
    if (m_src[m_pos] == '-') Advance();
    if (m_pos >= m_src.size() || !IsDigit(m_src[m_pos])) {
      return MakeAt(TokType::Error, "invalid number", start_line, start_col);
    }
    if (m_src[m_pos] == '0') {
      Advance();
    } else {
      while (m_pos < m_src.size() && IsDigit(m_src[m_pos])) Advance();
    }
    if (m_pos < m_src.size() && m_src[m_pos] == '.') {
      Advance();
      if (m_pos >= m_src.size() || !IsDigit(m_src[m_pos])) {
        return MakeAt(TokType::Error, "invalid number: expected digit after '.'",
                       start_line, start_col);
      }
      while (m_pos < m_src.size() && IsDigit(m_src[m_pos])) Advance();
    }
    if (m_pos < m_src.size() && (m_src[m_pos] == 'e' || m_src[m_pos] == 'E')) {
      Advance();
      if (m_pos < m_src.size() && (m_src[m_pos] == '+' || m_src[m_pos] == '-')) {
        Advance();
      }
      if (m_pos >= m_src.size() || !IsDigit(m_src[m_pos])) {
        return MakeAt(TokType::Error,
                       "invalid number: expected digit in exponent",
                       start_line, start_col);
      }
      while (m_pos < m_src.size() && IsDigit(m_src[m_pos])) Advance();
    }
    return MakeAt(TokType::Number, m_src.substr(begin, m_pos - begin),
                   start_line, start_col);
  }

  static bool IsDigit(char c) { return c >= '0' && c <= '9'; }
};

// Container kind for the explicit (heap) nesting stack used by the printer.
enum class Container { Object, Array };

struct Frame {
  Container kind;
  bool began = false;  // true once at least one member/element was emitted
};

// What kind of token is syntactically valid next. Driven purely by table
// lookups on top-of-stack, so arbitrarily deep nesting only grows a
// std::vector, never the C++ call stack.
enum class Expect {
  Value,               // top-level, after ':', after ',' in an array
  ValueOrCloseArray,    // right after '['
  KeyOrCloseObject,     // right after '{'
  KeyOnly,              // right after ',' in an object
  Colon,
  CommaOrCloseObject,
  CommaOrCloseArray,
  End,
};

bool IsValueStart(TokType t) {
  return t == TokType::String || t == TokType::Number || t == TokType::True ||
         t == TokType::False || t == TokType::Null || t == TokType::LBrace ||
         t == TokType::LBracket;
}

JsonFormatResult ErrorAt(const Token& t, const std::string& message) {
  JsonFormatResult r;
  r.ok = false;
  r.error.line = t.line;
  r.error.column = t.col;
  r.error.message = message.empty() ? t.text : message;
  return r;
}

JsonFormatResult FormatImpl(const std::string& json, bool pretty,
                             int indent_width) {
  Lexer lexer(json);
  std::string out;
  std::vector<Frame> stack;
  Expect expect = Expect::Value;

  auto indent = [&](size_t depth) {
    if (pretty) {
      out.push_back('\n');
      out.append(static_cast<size_t>(indent_width) * depth, ' ');
    }
  };

  while (true) {
    Token t = lexer.Next();
    if (t.type == TokType::Error) return ErrorAt(t, t.text);

    switch (expect) {
      case Expect::Value:
      case Expect::ValueOrCloseArray: {
        if (expect == Expect::ValueOrCloseArray && t.type == TokType::RBracket &&
            !stack.empty() && !stack.back().began) {
          out.push_back(']');
          stack.pop_back();
          expect = stack.empty()
                       ? Expect::End
                       : (stack.back().kind == Container::Object
                              ? Expect::CommaOrCloseObject
                              : Expect::CommaOrCloseArray);
          break;
        }
        if (!IsValueStart(t.type)) return ErrorAt(t, "expected a value");
        if (expect == Expect::ValueOrCloseArray) indent(stack.size());
        if (t.type == TokType::LBrace) {
          out.push_back('{');
          stack.push_back({Container::Object, false});
          expect = Expect::KeyOrCloseObject;
        } else if (t.type == TokType::LBracket) {
          out.push_back('[');
          stack.push_back({Container::Array, false});
          expect = Expect::ValueOrCloseArray;
        } else {
          out.append(t.text);
          if (stack.empty()) {
            expect = Expect::End;
          } else {
            stack.back().began = true;
            expect = stack.back().kind == Container::Object
                         ? Expect::CommaOrCloseObject
                         : Expect::CommaOrCloseArray;
          }
        }
        break;
      }
      case Expect::KeyOrCloseObject:
      case Expect::KeyOnly: {
        if (expect == Expect::KeyOrCloseObject && t.type == TokType::RBrace) {
          out.push_back('}');
          stack.pop_back();
          expect = stack.empty()
                       ? Expect::End
                       : (stack.back().kind == Container::Object
                              ? Expect::CommaOrCloseObject
                              : Expect::CommaOrCloseArray);
          break;
        }
        if (t.type != TokType::String) {
          return ErrorAt(t, "expected a string key");
        }
        indent(stack.size());
        out.append(t.text);
        expect = Expect::Colon;
        break;
      }
      case Expect::Colon: {
        if (t.type != TokType::Colon) return ErrorAt(t, "expected ':'");
        out.push_back(':');
        if (pretty) out.push_back(' ');
        expect = Expect::Value;
        break;
      }
      case Expect::CommaOrCloseObject: {
        if (t.type == TokType::RBrace) {
          size_t depth = stack.size() - 1;
          stack.pop_back();
          indent(depth);
          out.push_back('}');
          expect = stack.empty()
                       ? Expect::End
                       : (stack.back().kind == Container::Object
                              ? Expect::CommaOrCloseObject
                              : Expect::CommaOrCloseArray);
          break;
        }
        if (t.type != TokType::Comma) return ErrorAt(t, "expected ',' or '}'");
        out.push_back(',');
        stack.back().began = true;
        expect = Expect::KeyOnly;
        break;
      }
      case Expect::CommaOrCloseArray: {
        if (t.type == TokType::RBracket) {
          size_t depth = stack.size() - 1;
          stack.pop_back();
          indent(depth);
          out.push_back(']');
          expect = stack.empty()
                       ? Expect::End
                       : (stack.back().kind == Container::Object
                              ? Expect::CommaOrCloseObject
                              : Expect::CommaOrCloseArray);
          break;
        }
        if (t.type != TokType::Comma) {
          return ErrorAt(t, "expected ',' or ']'");
        }
        out.push_back(',');
        indent(stack.size());
        expect = Expect::Value;
        break;
      }
      case Expect::End: {
        if (t.type != TokType::End) {
          return ErrorAt(t, "unexpected trailing content after document");
        }
        JsonFormatResult r;
        r.ok = true;
        r.output = std::move(out);
        return r;
      }
    }
  }
}

}  // namespace

JsonFormatResult JsonPrettyPrint(const std::string& json, int indent_width) {
  return FormatImpl(json, /*pretty=*/true, indent_width);
}

JsonFormatResult JsonMinify(const std::string& json) {
  return FormatImpl(json, /*pretty=*/false, 0);
}

}  // namespace ep
