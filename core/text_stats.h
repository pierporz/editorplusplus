#pragma once

#include <cstddef>
#include <string>

namespace ep {

struct TextStats {
  size_t char_count = 0;  // Unicode codepoints, not bytes
  size_t line_count = 1;
  size_t byte_count = 0;
};

// Works the same for a whole document or for a selection substring -- pass
// whichever UTF-8 slice you want counted.
TextStats ComputeTextStats(const std::string& utf8_text);

}  // namespace ep
