#include "core/encoding.h"
#include "tests/test_framework.h"

using namespace ep;

TEST(Encoding, DetectsUtf8Bom) {
  std::string bytes = "\xEF\xBB\xBFhello";
  auto d = DetectEncoding(bytes);
  EXPECT_TRUE(d.encoding == Encoding::Utf8Bom);
  EXPECT_EQ(d.bom_length, static_cast<size_t>(3));
}

TEST(Encoding, DetectsUtf16LEBom) {
  std::string bytes = "\xFF\xFE\x68\x00";
  auto d = DetectEncoding(bytes);
  EXPECT_TRUE(d.encoding == Encoding::Utf16LE);
  EXPECT_EQ(d.bom_length, static_cast<size_t>(2));
}

TEST(Encoding, DetectsUtf16BEBom) {
  std::string bytes = "\xFE\xFF\x00\x68";
  auto d = DetectEncoding(bytes);
  EXPECT_TRUE(d.encoding == Encoding::Utf16BE);
  EXPECT_EQ(d.bom_length, static_cast<size_t>(2));
}

TEST(Encoding, DetectsPlainUtf8NoBom) {
  auto d = DetectEncoding("caf\xC3\xA9 - hello");
  EXPECT_TRUE(d.encoding == Encoding::Utf8);
  EXPECT_EQ(d.bom_length, static_cast<size_t>(0));
}

TEST(Encoding, DetectsAnsiWhenInvalidUtf8) {
  std::string bytes = "caf\xE9";  // Latin-1 'é' byte, invalid as UTF-8
  auto d = DetectEncoding(bytes);
  EXPECT_TRUE(d.encoding == Encoding::Ansi);
}

TEST(Encoding, IsValidUtf8Basics) {
  EXPECT_TRUE(IsValidUtf8(""));
  EXPECT_TRUE(IsValidUtf8("hello"));
  EXPECT_TRUE(IsValidUtf8("caf\xC3\xA9"));
  EXPECT_TRUE(IsValidUtf8("\xE4\xB8\xAD"));      // CJK 3-byte
  EXPECT_TRUE(IsValidUtf8("\xF0\x9F\x98\x80"));  // emoji 4-byte
}

TEST(Encoding, RejectsInvalidUtf8) {
  EXPECT_FALSE(IsValidUtf8("\x80"));              // stray continuation byte
  EXPECT_FALSE(IsValidUtf8("\xC0\x80"));           // overlong encoding of NUL
  EXPECT_FALSE(IsValidUtf8("\xC2"));               // truncated 2-byte sequence
  EXPECT_FALSE(IsValidUtf8("\xE0\x80\x80"));       // overlong 3-byte
  EXPECT_FALSE(IsValidUtf8("\xED\xA0\x80"));       // encoded surrogate
  EXPECT_FALSE(IsValidUtf8("\xF4\x90\x80\x80"));   // beyond U+10FFFF
}

TEST(Encoding, DetectEolMajorityVote) {
  EXPECT_TRUE(DetectEol("a\r\nb\r\nc") == Eol::CRLF);
  EXPECT_TRUE(DetectEol("a\nb\nc") == Eol::LF);
  EXPECT_TRUE(DetectEol("a\rb\rc") == Eol::CR);
  EXPECT_TRUE(DetectEol("a\r\nb\nc\nd\n") == Eol::LF);
  EXPECT_TRUE(DetectEol("no newlines here") == Eol::CRLF);
}

TEST(Encoding, ConvertEolNormalizesMixedInput) {
  std::string mixed = "a\r\nb\nc\rd";
  EXPECT_EQ(ConvertEol(mixed, Eol::LF), "a\nb\nc\nd");
  EXPECT_EQ(ConvertEol(mixed, Eol::CRLF), "a\r\nb\r\nc\r\nd");
  EXPECT_EQ(ConvertEol(mixed, Eol::CR), "a\rb\rc\rd");
}

TEST(Encoding, ConvertEolPreservesTrailingTerminatorPresenceOrAbsence) {
  EXPECT_EQ(ConvertEol("a\nb\n", Eol::CRLF), "a\r\nb\r\n");
  EXPECT_EQ(ConvertEol("a\nb", Eol::CRLF), "a\r\nb");
}
