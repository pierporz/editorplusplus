#pragma once

#include <cstddef>
#include <string>

namespace ep {

enum class Encoding { Utf8, Utf8Bom, Utf16LE, Utf16BE, Ansi };

struct EncodingDetection {
  Encoding encoding = Encoding::Utf8;
  size_t bom_length = 0;  // bytes of the BOM to strip before decoding, 0 if none
};

// Looks at a BOM first; falls back to UTF-8 validation; if that fails too,
// assumes single-byte ANSI (CP1252). Never fails -- there is always a
// reasonable guess for arbitrary bytes.
EncodingDetection DetectEncoding(const std::string& bytes);

bool IsValidUtf8(const std::string& bytes);

const char* EncodingName(Encoding encoding);  // "UTF-8", "UTF-8 BOM", ...

enum class Eol { CRLF, LF, CR };

const char* EolName(Eol eol);  // "CRLF", "LF", "CR"

// Majority vote across the three line-ending styles found in the text.
// Defaults to CRLF when no line ending is present at all.
Eol DetectEol(const std::string& text);

// Normalizes every CRLF/LF/CR line ending (in any mixture) to the target
// style. A trailing partial line with no terminator is left as-is.
std::string ConvertEol(const std::string& text, Eol target);

}  // namespace ep
