#include "core/xml_pretty.h"

#include <vector>

namespace ep {

namespace {

enum class NodeType { Text, OpenTag, CloseTag, SelfCloseTag, Comment, CData, Decl, Doctype };

struct Node {
  NodeType type;
  std::string name;  // tag name for Open/Close/SelfClose, lowercased copy for matching
  std::string full;  // raw source slice, verbatim
  size_t start_pos;
  size_t end_pos;  // one-past-the-end offset in the original source
};

std::string ToLower(const std::string& s) {
  std::string out = s;
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
  }
  return out;
}

std::string Trim(const std::string& s) {
  size_t begin = 0;
  while (begin < s.size() &&
         (s[begin] == ' ' || s[begin] == '\t' || s[begin] == '\r' || s[begin] == '\n')) {
    begin++;
  }
  size_t end = s.size();
  while (end > begin &&
         (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n')) {
    end--;
  }
  return s.substr(begin, end - begin);
}

bool IsWhitespaceOnly(const std::string& s) {
  for (char c : s) {
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') return false;
  }
  return true;
}

// Finds the offset of `marker` at or after `from`. Returns text.size() if not found.
size_t FindMarker(const std::string& text, const std::string& marker, size_t from) {
  size_t pos = text.find(marker, from);
  return pos == std::string::npos ? text.size() : pos;
}

// Scans a tag's raw text starting at `start` (which points at '<'), honoring
// quoted attribute values so a '>' inside "..." or '...' does not end the
// tag early. Returns the offset one-past the terminating '>' (or end of text
// if unterminated).
size_t ScanTagEnd(const std::string& text, size_t start) {
  size_t i = start + 1;
  char quote = '\0';
  while (i < text.size()) {
    char c = text[i];
    if (quote != '\0') {
      if (c == quote) quote = '\0';
    } else if (c == '"' || c == '\'') {
      quote = c;
    } else if (c == '>') {
      return i + 1;
    }
    i++;
  }
  return text.size();
}

std::string ExtractTagName(const std::string& tag_text, bool is_close) {
  size_t i = is_close ? 2 : 1;  // skip "</" or "<"
  size_t begin = i;
  while (i < tag_text.size()) {
    char c = tag_text[i];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '/' || c == '>') break;
    i++;
  }
  return tag_text.substr(begin, i - begin);
}

// Iterative tokenizer: a single left-to-right scan, no recursion.
std::vector<Node> Tokenize(const std::string& xml) {
  std::vector<Node> nodes;
  size_t pos = 0;
  size_t n = xml.size();

  while (pos < n) {
    if (xml[pos] != '<') {
      size_t next = xml.find('<', pos);
      if (next == std::string::npos) next = n;
      nodes.push_back({NodeType::Text, "", xml.substr(pos, next - pos), pos, next});
      pos = next;
      continue;
    }

    if (xml.compare(pos, 2, "<?") == 0) {
      size_t end = FindMarker(xml, "?>", pos + 2);
      end = (end == n) ? n : end + 2;
      nodes.push_back({NodeType::Decl, "", xml.substr(pos, end - pos), pos, end});
      pos = end;
    } else if (xml.compare(pos, 4, "<!--") == 0) {
      size_t end = FindMarker(xml, "-->", pos + 4);
      end = (end == n) ? n : end + 3;
      nodes.push_back({NodeType::Comment, "", xml.substr(pos, end - pos), pos, end});
      pos = end;
    } else if (xml.compare(pos, 9, "<![CDATA[") == 0) {
      size_t end = FindMarker(xml, "]]>", pos + 9);
      end = (end == n) ? n : end + 3;
      nodes.push_back({NodeType::CData, "", xml.substr(pos, end - pos), pos, end});
      pos = end;
    } else if (xml.compare(pos, 2, "<!") == 0) {
      size_t end = ScanTagEnd(xml, pos);
      nodes.push_back({NodeType::Doctype, "", xml.substr(pos, end - pos), pos, end});
      pos = end;
    } else if (xml.compare(pos, 2, "</") == 0) {
      size_t end = ScanTagEnd(xml, pos);
      std::string raw = xml.substr(pos, end - pos);
      nodes.push_back({NodeType::CloseTag, ToLower(ExtractTagName(raw, true)), raw, pos, end});
      pos = end;
    } else {
      size_t end = ScanTagEnd(xml, pos);
      std::string raw = xml.substr(pos, end - pos);
      bool self_closing = false;
      for (size_t i = raw.size(); i-- > 0;) {
        if (raw[i] == ' ' || raw[i] == '\t' || raw[i] == '\r' || raw[i] == '\n') continue;
        if (raw[i] == '>' && i > 0 && raw[i - 1] == '/') self_closing = true;
        break;
      }
      NodeType type = self_closing ? NodeType::SelfCloseTag : NodeType::OpenTag;
      nodes.push_back({type, ToLower(ExtractTagName(raw, false)), raw, pos, end});
      pos = end;
    }
  }

  return nodes;
}

class Formatter {
 public:
  Formatter(const std::string& source, int indent_width)
      : m_source(source), m_indent_width(indent_width) {}

  std::string Run(const std::vector<Node>& nodes) {
    size_t i = 0;
    while (i < nodes.size()) {
      i = EmitNode(nodes, i);
    }
    return std::move(m_out);
  }

 private:
  const std::string& m_source;
  int m_indent_width;
  std::string m_out;
  std::vector<std::string> m_open_stack;
  bool m_first = true;

  void StartLine() {
    if (!m_first) m_out.push_back('\n');
    m_first = false;
    m_out.append(static_cast<size_t>(m_indent_width) * m_open_stack.size(), ' ');
  }

  // Returns the index of the next node to process.
  size_t EmitNode(const std::vector<Node>& nodes, size_t i) {
    const Node& node = nodes[i];

    switch (node.type) {
      case NodeType::Text: {
        if (IsWhitespaceOnly(node.full)) return i + 1;
        StartLine();
        m_out.append(Trim(node.full));
        return i + 1;
      }
      case NodeType::Comment:
      case NodeType::CData:
      case NodeType::Decl:
      case NodeType::Doctype: {
        StartLine();
        m_out.append(node.full);
        return i + 1;
      }
      case NodeType::SelfCloseTag: {
        StartLine();
        m_out.append(node.full);
        return i + 1;
      }
      case NodeType::CloseTag: {
        if (!m_open_stack.empty()) m_open_stack.pop_back();
        StartLine();
        m_out.append(node.full);
        return i + 1;
      }
      case NodeType::OpenTag: {
        if (node.name == "pre") return EmitPreVerbatim(nodes, i);

        // Leaf-inline heuristic: <tag>text</tag> with nothing else inside
        // stays on one line, matching what most XML/HTML pretty-printers do.
        if (i + 2 < nodes.size() && nodes[i + 1].type == NodeType::Text &&
            nodes[i + 2].type == NodeType::CloseTag &&
            nodes[i + 2].name == node.name && !IsWhitespaceOnly(nodes[i + 1].full)) {
          StartLine();
          m_out.append(node.full);
          m_out.append(Trim(nodes[i + 1].full));
          m_out.append(nodes[i + 2].full);
          return i + 3;
        }
        // Also collapse genuinely empty elements split as Open+Close with
        // only whitespace between them: <tag>\n</tag> -> keep as-is but on
        // its own two lines (handled by falling through to normal push).
        StartLine();
        m_out.append(node.full);
        m_open_stack.push_back(node.name);
        return i + 1;
      }
    }
    return i + 1;
  }

  size_t EmitPreVerbatim(const std::vector<Node>& nodes, size_t open_index) {
    const Node& open_node = nodes[open_index];
    StartLine();
    m_out.append(open_node.full);

    size_t depth = 1;
    size_t j = open_index + 1;
    for (; j < nodes.size(); j++) {
      if (nodes[j].type == NodeType::OpenTag && nodes[j].name == "pre") depth++;
      if (nodes[j].type == NodeType::CloseTag && nodes[j].name == "pre") {
        depth--;
        if (depth == 0) break;
      }
    }

    size_t content_start = open_node.end_pos;
    size_t content_end = (j < nodes.size()) ? nodes[j].start_pos : m_source.size();
    m_out.append(m_source.substr(content_start, content_end - content_start));

    if (j < nodes.size()) {
      m_out.append(nodes[j].full);
      return j + 1;
    }
    return nodes.size();
  }
};

}  // namespace

std::string XmlPrettyPrint(const std::string& xml, int indent_width) {
  std::vector<Node> nodes = Tokenize(xml);
  Formatter formatter(xml, indent_width);
  return formatter.Run(nodes);
}

}  // namespace ep
