#pragma once

#include <cstddef>
#include <string>

namespace ep {

struct JsonFormatError {
  size_t line = 1;    // 1-based
  size_t column = 1;  // 1-based, counted in bytes
  std::string message;
};

struct JsonFormatResult {
  bool ok = false;
  std::string output;  // valid only if ok
  JsonFormatError error;  // valid only if !ok
};

// Both functions parse the whole input as a single JSON document and, on
// success, re-emit it either indented or minified. On any grammar error the
// input is left untouched by the caller: JsonFormatResult.ok is false and
// .output is empty, with .error pointing at the offending byte.
JsonFormatResult JsonPrettyPrint(const std::string& json, int indent_width = 2);
JsonFormatResult JsonMinify(const std::string& json);

}  // namespace ep
