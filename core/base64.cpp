#include "core/base64.h"

#include <array>
#include <cstdint>

namespace ep {

namespace {

constexpr char kAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::array<int8_t, 256> BuildDecodeTable() {
  std::array<int8_t, 256> table{};
  table.fill(-1);
  for (int i = 0; i < 64; i++) {
    table[static_cast<unsigned char>(kAlphabet[i])] = static_cast<int8_t>(i);
  }
  return table;
}

bool IsSkippable(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

}  // namespace

std::string Base64Encode(const std::string& bytes) {
  std::string out;
  out.reserve(((bytes.size() + 2) / 3) * 4);

  size_t i = 0;
  while (i + 3 <= bytes.size()) {
    uint32_t n = (static_cast<unsigned char>(bytes[i]) << 16) |
                 (static_cast<unsigned char>(bytes[i + 1]) << 8) |
                 static_cast<unsigned char>(bytes[i + 2]);
    out.push_back(kAlphabet[(n >> 18) & 0x3F]);
    out.push_back(kAlphabet[(n >> 12) & 0x3F]);
    out.push_back(kAlphabet[(n >> 6) & 0x3F]);
    out.push_back(kAlphabet[n & 0x3F]);
    i += 3;
  }

  size_t remaining = bytes.size() - i;
  if (remaining == 1) {
    uint32_t n = static_cast<unsigned char>(bytes[i]) << 16;
    out.push_back(kAlphabet[(n >> 18) & 0x3F]);
    out.push_back(kAlphabet[(n >> 12) & 0x3F]);
    out.push_back('=');
    out.push_back('=');
  } else if (remaining == 2) {
    uint32_t n = (static_cast<unsigned char>(bytes[i]) << 16) |
                 (static_cast<unsigned char>(bytes[i + 1]) << 8);
    out.push_back(kAlphabet[(n >> 18) & 0x3F]);
    out.push_back(kAlphabet[(n >> 12) & 0x3F]);
    out.push_back(kAlphabet[(n >> 6) & 0x3F]);
    out.push_back('=');
  }
  return out;
}

Result<std::string> Base64Decode(const std::string& encoded) {
  static const std::array<int8_t, 256> kDecodeTable = BuildDecodeTable();

  std::string quad;
  quad.reserve(4);
  std::string out;
  out.reserve(encoded.size() / 4 * 3);

  size_t pad_count = 0;
  bool stream_finished = false;
  for (size_t pos = 0; pos < encoded.size(); pos++) {
    char c = encoded[pos];
    if (IsSkippable(c)) continue;
    if (stream_finished) {
      return Fail<std::string>("unexpected data after padding");
    }
    if (c == '=') {
      pad_count++;
      quad.push_back(c);
    } else {
      if (pad_count > 0) {
        return Fail<std::string>("unexpected data after padding");
      }
      if (kDecodeTable[static_cast<unsigned char>(c)] < 0) {
        return Fail<std::string>("invalid base64 character");
      }
      quad.push_back(c);
    }

    if (quad.size() == 4) {
      int real_pad = static_cast<int>(pad_count);
      if (real_pad > 2) return Fail<std::string>("invalid padding");

      int8_t v0 = quad[0] == '=' ? 0 : kDecodeTable[static_cast<unsigned char>(quad[0])];
      int8_t v1 = quad[1] == '=' ? 0 : kDecodeTable[static_cast<unsigned char>(quad[1])];
      int8_t v2 = quad[2] == '=' ? 0 : kDecodeTable[static_cast<unsigned char>(quad[2])];
      int8_t v3 = quad[3] == '=' ? 0 : kDecodeTable[static_cast<unsigned char>(quad[3])];
      if (quad[0] == '=' || quad[1] == '=') {
        return Fail<std::string>("invalid padding position");
      }

      uint32_t n = (static_cast<uint32_t>(v0) << 18) |
                   (static_cast<uint32_t>(v1) << 12) |
                   (static_cast<uint32_t>(v2) << 6) | static_cast<uint32_t>(v3);
      out.push_back(static_cast<char>((n >> 16) & 0xFF));
      if (real_pad < 2) out.push_back(static_cast<char>((n >> 8) & 0xFF));
      if (real_pad < 1) out.push_back(static_cast<char>(n & 0xFF));

      if (real_pad > 0) stream_finished = true;
      quad.clear();
      pad_count = 0;
    }
  }

  if (!quad.empty()) {
    return Fail<std::string>("truncated base64 input");
  }
  return Ok(std::move(out));
}

}  // namespace ep
