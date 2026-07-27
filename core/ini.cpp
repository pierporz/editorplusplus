#include "core/ini.h"

#include <cstdlib>

namespace ep {

namespace {

std::string Trim(const std::string& s) {
  size_t begin = 0;
  while (begin < s.size() && (s[begin] == ' ' || s[begin] == '\t' || s[begin] == '\r')) {
    begin++;
  }
  size_t end = s.size();
  while (end > begin && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r')) {
    end--;
  }
  return s.substr(begin, end - begin);
}

bool EqualsIgnoreCase(const std::string& a, const char* b) {
  size_t i = 0;
  for (; a[i] != '\0' && b[i] != '\0'; i++) {
    char ca = a[i] >= 'A' && a[i] <= 'Z' ? static_cast<char>(a[i] + 32) : a[i];
    if (ca != b[i]) return false;
  }
  return a[i] == '\0' && b[i] == '\0';
}

}  // namespace

IniDocument IniDocument::Parse(const std::string& text) {
  IniDocument doc;
  Section* current = nullptr;

  size_t pos = 0;
  while (pos <= text.size()) {
    size_t eol = text.find('\n', pos);
    std::string raw_line =
        eol == std::string::npos ? text.substr(pos) : text.substr(pos, eol - pos);
    pos = eol == std::string::npos ? text.size() + 1 : eol + 1;

    std::string line = Trim(raw_line);
    if (line.empty() || line[0] == ';' || line[0] == '#') continue;

    if (line.front() == '[' && line.back() == ']' && line.size() >= 2) {
      std::string name = Trim(line.substr(1, line.size() - 2));
      current = &doc.GetOrCreateSection(name);
      continue;
    }

    size_t eq = line.find('=');
    if (eq == std::string::npos) continue;  // malformed line, skip

    std::string key = Trim(line.substr(0, eq));
    std::string value = Trim(line.substr(eq + 1));
    if (key.empty()) continue;

    if (!current) current = &doc.GetOrCreateSection("");
    current->entries.push_back({key, value});
  }

  return doc;
}

std::string IniDocument::Serialize() const {
  std::string out;
  for (const auto& section : m_sections) {
    if (!section.name.empty()) {
      out += "[" + section.name + "]\n";
    }
    for (const auto& entry : section.entries) {
      out += entry.key + "=" + entry.value + "\n";
    }
    if (!section.name.empty()) out += "\n";
  }
  return out;
}

IniDocument::Section* IniDocument::FindSection(const std::string& name) {
  for (auto& s : m_sections) {
    if (s.name == name) return &s;
  }
  return nullptr;
}

const IniDocument::Section* IniDocument::FindSection(const std::string& name) const {
  for (const auto& s : m_sections) {
    if (s.name == name) return &s;
  }
  return nullptr;
}

IniDocument::Section& IniDocument::GetOrCreateSection(const std::string& name) {
  if (Section* existing = FindSection(name)) return *existing;
  m_sections.push_back({name, {}});
  return m_sections.back();
}

std::string IniDocument::Get(const std::string& section, const std::string& key,
                              const std::string& default_value) const {
  const Section* s = FindSection(section);
  if (!s) return default_value;
  for (const auto& entry : s->entries) {
    if (entry.key == key) return entry.value;
  }
  return default_value;
}

void IniDocument::Set(const std::string& section, const std::string& key,
                       const std::string& value) {
  Section& s = GetOrCreateSection(section);
  for (auto& entry : s.entries) {
    if (entry.key == key) {
      entry.value = value;
      return;
    }
  }
  s.entries.push_back({key, value});
}

int IniDocument::GetInt(const std::string& section, const std::string& key,
                         int default_value) const {
  std::string v = Get(section, key, "");
  if (v.empty()) return default_value;
  char* end = nullptr;
  long parsed = std::strtol(v.c_str(), &end, 10);
  if (end == v.c_str() || *end != '\0') return default_value;
  return static_cast<int>(parsed);
}

void IniDocument::SetInt(const std::string& section, const std::string& key, int value) {
  Set(section, key, std::to_string(value));
}

bool IniDocument::GetBool(const std::string& section, const std::string& key,
                           bool default_value) const {
  std::string v = Get(section, key, "");
  if (v.empty()) return default_value;
  if (v == "1" || EqualsIgnoreCase(v, "true") || EqualsIgnoreCase(v, "yes")) return true;
  if (v == "0" || EqualsIgnoreCase(v, "false") || EqualsIgnoreCase(v, "no")) return false;
  return default_value;
}

void IniDocument::SetBool(const std::string& section, const std::string& key, bool value) {
  Set(section, key, value ? "1" : "0");
}

}  // namespace ep
