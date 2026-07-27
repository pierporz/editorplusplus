#include "core/encoding.h"

#include <cstdint>
#include <initializer_list>

namespace ep {

namespace {

bool StartsWith(const std::string& bytes, std::initializer_list<unsigned char> prefix) {
  if (bytes.size() < prefix.size()) return false;
  size_t i = 0;
  for (unsigned char b : prefix) {
    if (static_cast<unsigned char>(bytes[i]) != b) return false;
    i++;
  }
  return true;
}

}  // namespace

bool IsValidUtf8(const std::string& bytes) {
  size_t i = 0;
  size_t n = bytes.size();
  while (i < n) {
    unsigned char b0 = static_cast<unsigned char>(bytes[i]);
    size_t extra;
    uint32_t min_codepoint;
    uint32_t codepoint;

    if (b0 <= 0x7F) {
      i++;
      continue;
    } else if ((b0 & 0xE0) == 0xC0) {
      extra = 1;
      min_codepoint = 0x80;
      codepoint = b0 & 0x1F;
    } else if ((b0 & 0xF0) == 0xE0) {
      extra = 2;
      min_codepoint = 0x800;
      codepoint = b0 & 0x0F;
    } else if ((b0 & 0xF8) == 0xF0) {
      extra = 3;
      min_codepoint = 0x10000;
      codepoint = b0 & 0x07;
    } else {
      return false;  // stray continuation byte or invalid leading byte
    }

    if (i + extra >= n) return false;

    for (size_t k = 1; k <= extra; k++) {
      unsigned char bk = static_cast<unsigned char>(bytes[i + k]);
      if ((bk & 0xC0) != 0x80) return false;
      codepoint = (codepoint << 6) | (bk & 0x3F);
    }

    if (codepoint < min_codepoint) return false;  // overlong encoding
    if (codepoint > 0x10FFFF) return false;
    if (codepoint >= 0xD800 && codepoint <= 0xDFFF) return false;  // surrogate

    i += extra + 1;
  }
  return true;
}

EncodingDetection DetectEncoding(const std::string& bytes) {
  EncodingDetection result;

  if (StartsWith(bytes, {0xEF, 0xBB, 0xBF})) {
    result.encoding = Encoding::Utf8Bom;
    result.bom_length = 3;
    return result;
  }
  if (StartsWith(bytes, {0xFF, 0xFE})) {
    result.encoding = Encoding::Utf16LE;
    result.bom_length = 2;
    return result;
  }
  if (StartsWith(bytes, {0xFE, 0xFF})) {
    result.encoding = Encoding::Utf16BE;
    result.bom_length = 2;
    return result;
  }

  if (IsValidUtf8(bytes)) {
    result.encoding = Encoding::Utf8;
    result.bom_length = 0;
    return result;
  }

  result.encoding = Encoding::Ansi;
  result.bom_length = 0;
  return result;
}

const char* EncodingName(Encoding encoding) {
  switch (encoding) {
    case Encoding::Utf8:
      return "UTF-8";
    case Encoding::Utf8Bom:
      return "UTF-8 BOM";
    case Encoding::Utf16LE:
      return "UTF-16LE";
    case Encoding::Utf16BE:
      return "UTF-16BE";
    case Encoding::Ansi:
      return "ANSI";
  }
  return "UTF-8";
}

const char* EolName(Eol eol) {
  switch (eol) {
    case Eol::CRLF:
      return "CRLF";
    case Eol::LF:
      return "LF";
    case Eol::CR:
      return "CR";
  }
  return "CRLF";
}

Eol DetectEol(const std::string& text) {
  size_t crlf = 0, lf_only = 0, cr_only = 0;
  for (size_t i = 0; i < text.size(); i++) {
    if (text[i] == '\r') {
      if (i + 1 < text.size() && text[i + 1] == '\n') {
        crlf++;
        i++;
      } else {
        cr_only++;
      }
    } else if (text[i] == '\n') {
      lf_only++;
    }
  }

  if (crlf == 0 && lf_only == 0 && cr_only == 0) return Eol::CRLF;
  if (crlf >= lf_only && crlf >= cr_only) return Eol::CRLF;
  if (lf_only >= cr_only) return Eol::LF;
  return Eol::CR;
}

std::string ConvertEol(const std::string& text, Eol target) {
  const char* target_str = target == Eol::CRLF   ? "\r\n"
                            : target == Eol::LF ? "\n"
                                                 : "\r";
  std::string out;
  out.reserve(text.size());

  size_t i = 0;
  while (i < text.size()) {
    char c = text[i];
    if (c == '\r') {
      out.append(target_str);
      i += (i + 1 < text.size() && text[i + 1] == '\n') ? 2 : 1;
    } else if (c == '\n') {
      out.append(target_str);
      i += 1;
    } else {
      out.push_back(c);
      i += 1;
    }
  }
  return out;
}

}  // namespace ep
