#pragma once

#include <string>
#include <utility>
#include <vector>

namespace ep {

// Minimal INI document: [section] headers, key=value lines, ';' or '#'
// comments, blank lines. Parsing is best-effort and never fails -- lines
// that are not comments, blank, a section header, or a key=value pair are
// simply skipped, and unknown keys/sections read back as their default.
// Section/key order is preserved so Serialize() round-trips a hand-edited
// file without reshuffling it.
class IniDocument {
 public:
  static IniDocument Parse(const std::string& text);
  std::string Serialize() const;

  std::string Get(const std::string& section, const std::string& key,
                   const std::string& default_value = "") const;
  void Set(const std::string& section, const std::string& key,
           const std::string& value);

  int GetInt(const std::string& section, const std::string& key,
             int default_value) const;
  void SetInt(const std::string& section, const std::string& key, int value);

  bool GetBool(const std::string& section, const std::string& key,
               bool default_value) const;
  void SetBool(const std::string& section, const std::string& key, bool value);

 private:
  struct Entry {
    std::string key;
    std::string value;
  };
  struct Section {
    std::string name;
    std::vector<Entry> entries;
  };

  Section* FindSection(const std::string& name);
  const Section* FindSection(const std::string& name) const;
  Section& GetOrCreateSection(const std::string& name);

  std::vector<Section> m_sections;
};

}  // namespace ep
