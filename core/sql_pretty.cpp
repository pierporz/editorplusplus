#include "core/sql_pretty.h"

#include <array>
#include <vector>

namespace ep {

namespace {

enum class TokType { Word, Number, String, Symbol, Comment, End };

struct Token {
  TokType type;
  std::string text;   // raw source text, verbatim
  std::string upper;  // upper-cased text, only meaningful for Word
};

std::string ToUpper(const std::string& s) {
  std::string out = s;
  for (char& c : out) {
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
  }
  return out;
}

bool IsIdentStart(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}
bool IsIdentChar(char c) { return IsIdentStart(c) || (c >= '0' && c <= '9'); }
bool IsDigit(char c) { return c >= '0' && c <= '9'; }

// Iterative lexer: a single left-to-right scan, no recursion, so it never
// overflows the call stack regardless of input size or nesting.
class Lexer {
 public:
  explicit Lexer(const std::string& src) : m_src(src) {}

  Token Next() {
    SkipWhitespace();
    if (m_pos >= m_src.size()) return {TokType::End, "", ""};

    char c = m_src[m_pos];

    if (c == '-' && Peek(1) == '-') return ScanLineComment();
    if (c == '/' && Peek(1) == '*') return ScanBlockComment();
    if (c == '\'' || c == '"' || c == '`') return ScanQuoted(c);
    if (IsIdentStart(c)) return ScanWord();
    if (IsDigit(c)) return ScanNumber();
    return ScanSymbol();
  }

 private:
  const std::string& m_src;
  size_t m_pos = 0;

  char Peek(size_t ahead) const {
    size_t i = m_pos + ahead;
    return i < m_src.size() ? m_src[i] : '\0';
  }

  void SkipWhitespace() {
    while (m_pos < m_src.size()) {
      char c = m_src[m_pos];
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        m_pos++;
      } else {
        break;
      }
    }
  }

  Token ScanLineComment() {
    size_t begin = m_pos;
    while (m_pos < m_src.size() && m_src[m_pos] != '\n') m_pos++;
    return {TokType::Comment, m_src.substr(begin, m_pos - begin), ""};
  }

  Token ScanBlockComment() {
    size_t begin = m_pos;
    m_pos += 2;
    while (m_pos < m_src.size() && !(m_src[m_pos] == '*' && Peek(1) == '/')) m_pos++;
    m_pos = (m_pos < m_src.size()) ? m_pos + 2 : m_src.size();
    return {TokType::Comment, m_src.substr(begin, m_pos - begin), ""};
  }

  Token ScanQuoted(char quote) {
    size_t begin = m_pos;
    m_pos++;
    while (m_pos < m_src.size()) {
      if (m_src[m_pos] == quote) {
        if (Peek(1) == quote) {  // doubled-quote escape, e.g. 'it''s'
          m_pos += 2;
          continue;
        }
        m_pos++;
        break;
      }
      m_pos++;
    }
    return {TokType::String, m_src.substr(begin, m_pos - begin), ""};
  }

  Token ScanWord() {
    size_t begin = m_pos;
    while (m_pos < m_src.size() && IsIdentChar(m_src[m_pos])) m_pos++;
    std::string text = m_src.substr(begin, m_pos - begin);
    return {TokType::Word, text, ToUpper(text)};
  }

  Token ScanNumber() {
    size_t begin = m_pos;
    while (m_pos < m_src.size() && IsDigit(m_src[m_pos])) m_pos++;
    if (m_pos < m_src.size() && m_src[m_pos] == '.' && IsDigit(Peek(1))) {
      m_pos++;
      while (m_pos < m_src.size() && IsDigit(m_src[m_pos])) m_pos++;
    }
    return {TokType::Number, m_src.substr(begin, m_pos - begin), ""};
  }

  Token ScanSymbol() {
    static constexpr std::array<const char*, 6> kTwoChar = {"<=", ">=", "<>", "!=", "||", "::"};
    for (const char* op : kTwoChar) {
      if (m_src.compare(m_pos, 2, op) == 0) {
        m_pos += 2;
        return {TokType::Symbol, op, ""};
      }
    }
    std::string text(1, m_src[m_pos]);
    m_pos++;
    return {TokType::Symbol, text, ""};
  }
};

// Multi-word clause keywords the formatter treats as a single unit. Longest
// phrases must be listed so a greedy longest-match wins (e.g. "LEFT OUTER
// JOIN" before "LEFT JOIN").
struct Phrase {
  std::array<const char*, 3> words;
  int count;
};
constexpr Phrase kPhrases[] = {
    {{"LEFT", "OUTER", "JOIN"}, 3},  {{"RIGHT", "OUTER", "JOIN"}, 3},
    {{"FULL", "OUTER", "JOIN"}, 3},  {{"GROUP", "BY", nullptr}, 2},
    {{"ORDER", "BY", nullptr}, 2},   {{"INSERT", "INTO", nullptr}, 2},
    {{"DELETE", "FROM", nullptr}, 2}, {{"UNION", "ALL", nullptr}, 2},
    {{"INNER", "JOIN", nullptr}, 2}, {{"LEFT", "JOIN", nullptr}, 2},
    {{"RIGHT", "JOIN", nullptr}, 2}, {{"FULL", "JOIN", nullptr}, 2},
    {{"CROSS", "JOIN", nullptr}, 2},
};

// Merges runs of Word tokens matching a known multi-word keyword phrase
// into a single token (e.g. ["GROUP","BY"] -> "GROUP BY"). Iterative:
// bounded lookahead per position, one pass over the token list.
std::vector<Token> MergePhrases(const std::vector<Token>& tokens) {
  std::vector<Token> out;
  size_t i = 0;
  while (i < tokens.size()) {
    bool matched = false;
    if (tokens[i].type == TokType::Word) {
      for (const Phrase& phrase : kPhrases) {
        if (i + static_cast<size_t>(phrase.count) > tokens.size()) continue;
        bool ok = true;
        std::string merged_text;
        std::string merged_upper;
        for (int k = 0; k < phrase.count; k++) {
          const Token& t = tokens[i + static_cast<size_t>(k)];
          if (t.type != TokType::Word || t.upper != phrase.words[static_cast<size_t>(k)]) {
            ok = false;
            break;
          }
          if (k > 0) {
            merged_text += ' ';
            merged_upper += ' ';
          }
          merged_text += t.text;
          merged_upper += t.upper;
        }
        if (ok) {
          out.push_back({TokType::Word, merged_text, merged_upper});
          i += static_cast<size_t>(phrase.count);
          matched = true;
          break;
        }
      }
    }
    if (!matched) {
      out.push_back(tokens[i]);
      i++;
    }
  }
  return out;
}

bool IsClauseKeyword(const std::string& upper) {
  static const std::array<const char*, 25> kClauses = {
      "SELECT",     "FROM",       "WHERE",        "HAVING",      "LIMIT",
      "OFFSET",     "SET",        "VALUES",       "UPDATE",      "WITH",
      "UNION",      "UNION ALL",  "JOIN",         "INNER JOIN",  "LEFT JOIN",
      "RIGHT JOIN", "FULL JOIN",  "CROSS JOIN",   "LEFT OUTER JOIN",
      "RIGHT OUTER JOIN", "FULL OUTER JOIN", "GROUP BY", "ORDER BY",
      "INSERT INTO", "DELETE FROM"};
  for (const char* kw : kClauses) {
    if (upper == kw) return true;
  }
  return false;
}

// Other reserved words that get upper-cased for readability but don't start
// a new line -- unlike IsClauseKeyword, these stay wherever they naturally
// fall in the token stream.
bool IsReservedWord(const std::string& upper) {
  static const std::array<const char*, 21> kWords = {
      "AS",   "ON",     "IN",    "IS",    "NOT",  "NULL", "LIKE",  "BETWEEN",
      "EXISTS", "ASC",  "DESC",  "DISTINCT", "CASE", "WHEN", "THEN", "ELSE",
      "END",  "TOP",    "ALL",   "TRUE",  "FALSE"};
  for (const char* kw : kWords) {
    if (upper == kw) return true;
  }
  return false;
}

// Function-call-style keywords: when one of these precedes '(' the paren
// stays tight (no space, no forced multi-line) instead of getting the
// "operator paren" treatment (VALUES (..), IN (..)).
bool WantsSpaceBeforeParen(const std::string& upper) {
  return upper == "IN" || upper == "VALUES" || upper == "AND" || upper == "OR" ||
         upper == "NOT" || upper == "EXISTS" || IsClauseKeyword(upper);
}

class Formatter {
 public:
  Formatter(int indent_width) : m_indent_width(indent_width) {}

  std::string Run(const std::vector<Token>& tokens) {
    m_select_list_active.push_back(false);

    for (size_t i = 0; i < tokens.size(); i++) {
      const Token& t = tokens[i];
      switch (t.type) {
        case TokType::End:
          break;
        case TokType::Comment:
        case TokType::String:
        case TokType::Number:
          EmitInline(t.text);
          break;
        case TokType::Word:
          EmitWord(t);
          break;
        case TokType::Symbol:
          EmitSymbol(t, tokens, i);
          break;
      }
    }
    return std::move(m_out);
  }

 private:
  int m_indent_width;
  std::string m_out;
  bool m_first = true;
  bool m_at_line_start = true;
  int m_paren_depth = 0;
  std::vector<bool> m_select_list_active;  // indexed by paren depth
  std::vector<bool> m_is_subquery_paren;   // one entry per currently-open paren

  // depth is passed explicitly rather than always using m_paren_depth
  // because SELECT's column list and AND/OR conditions sit one logical
  // level deeper than their clause keyword without opening a paren.
  void StartLine(int depth) {
    if (!m_first) m_out.push_back('\n');
    m_first = false;
    m_out.append(static_cast<size_t>(m_indent_width) * static_cast<size_t>(depth), ' ');
    m_at_line_start = true;
  }

  void EmitInline(const std::string& text) {
    if (!m_at_line_start && !m_out.empty()) m_out.push_back(' ');
    m_out += text;
    m_at_line_start = false;
    m_first = false;
  }

  void EmitWord(const Token& t) {
    if (t.upper == "AND" || t.upper == "OR") {
      StartLine(m_paren_depth + 1);
      m_out += t.upper;
      m_at_line_start = false;
      return;
    }

    if (IsClauseKeyword(t.upper)) {
      StartLine(m_paren_depth);
      m_out += t.upper;
      bool is_select = (t.upper == "SELECT");
      m_select_list_active[static_cast<size_t>(m_paren_depth)] = is_select;
      if (is_select) {
        StartLine(m_paren_depth + 1);  // first column starts its own indented line too
      } else {
        m_at_line_start = false;  // keyword's argument stays on the same line (FROM t, WHERE x)
      }
      return;
    }

    if (IsReservedWord(t.upper)) {
      EmitInline(t.upper);
      return;
    }

    // Everything else -- identifiers/function/table/column names -- is kept
    // as written, since SQL identifiers are often case-sensitive or
    // case-preserved by convention.
    EmitInline(t.text);
  }

  void EmitSymbol(const Token& t, const std::vector<Token>& tokens, size_t index) {
    const std::string& s = t.text;

    if (s == ",") {
      m_out += ",";
      if (m_select_list_active[static_cast<size_t>(m_paren_depth)]) {
        StartLine(m_paren_depth + 1);
      } else {
        m_at_line_start = false;  // EmitInline() adds the single space before the next token
      }
      return;
    }

    if (s == ";") {
      m_out += ";";
      m_at_line_start = false;
      return;
    }

    if (s == ".") {
      m_out += ".";
      m_at_line_start = true;  // suppress the space EmitInline would add next
      return;
    }

    if (s == "(") {
      bool prev_wants_space = index > 0 && PrevWordWantsSpaceBeforeParen(tokens, index);
      if (prev_wants_space) {
        EmitInline("(");
      } else {
        m_out += "(";
        m_first = false;
      }
      m_at_line_start = true;  // suppress the space EmitInline would add right after "("
      m_paren_depth++;
      bool is_subquery = NextSignificantIsSelectLike(tokens, index + 1);
      m_is_subquery_paren.push_back(is_subquery);
      if (static_cast<int>(m_select_list_active.size()) <= m_paren_depth) {
        m_select_list_active.push_back(false);
      } else {
        m_select_list_active[static_cast<size_t>(m_paren_depth)] = false;
      }
      // No explicit newline here even for a subquery: the SELECT/WITH
      // keyword that comes right after will call StartLine() itself with
      // the now-incremented depth, so doing it here too would double up.
      return;
    }

    if (s == ")") {
      bool was_subquery = !m_is_subquery_paren.empty() && m_is_subquery_paren.back();
      if (!m_is_subquery_paren.empty()) m_is_subquery_paren.pop_back();
      if (m_paren_depth > 0) m_paren_depth--;
      if (was_subquery) StartLine(m_paren_depth);
      m_out += ")";
      m_at_line_start = false;
      m_first = false;
      return;
    }

    // Generic operator: single space on both sides.
    EmitInline(s);
  }

  static bool PrevWordWantsSpaceBeforeParen(const std::vector<Token>& tokens, size_t open_index) {
    if (open_index == 0) return true;
    const Token& prev = tokens[open_index - 1];
    if (prev.type != TokType::Word) return true;
    return WantsSpaceBeforeParen(prev.upper);
  }

  static bool NextSignificantIsSelectLike(const std::vector<Token>& tokens, size_t from) {
    for (size_t i = from; i < tokens.size(); i++) {
      if (tokens[i].type == TokType::Comment) continue;
      return tokens[i].type == TokType::Word &&
             (tokens[i].upper == "SELECT" || tokens[i].upper == "WITH");
    }
    return false;
  }
};

}  // namespace

std::string SqlPretty(const std::string& sql, int indent_width) {
  Lexer lexer(sql);
  std::vector<Token> tokens;
  while (true) {
    Token t = lexer.Next();
    if (t.type == TokType::End) break;
    tokens.push_back(std::move(t));
  }
  tokens = MergePhrases(tokens);

  Formatter formatter(indent_width);
  return formatter.Run(tokens);
}

}  // namespace ep
