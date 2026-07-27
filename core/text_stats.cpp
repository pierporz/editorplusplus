#include "core/text_stats.h"

namespace ep {

TextStats ComputeTextStats(const std::string& utf8_text) {
  TextStats stats;
  stats.byte_count = utf8_text.size();

  size_t eol_boundaries = 0;
  for (size_t i = 0; i < utf8_text.size(); i++) {
    unsigned char c = static_cast<unsigned char>(utf8_text[i]);
    if ((c & 0xC0) != 0x80) stats.char_count++;  // not a UTF-8 continuation byte

    if (c == '\r') {
      eol_boundaries++;
      if (i + 1 < utf8_text.size() && utf8_text[i + 1] == '\n') i++;
    } else if (c == '\n') {
      eol_boundaries++;
    }
  }
  stats.line_count = eol_boundaries + 1;

  return stats;
}

}  // namespace ep
