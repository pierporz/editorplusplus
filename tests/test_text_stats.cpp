#include "core/text_stats.h"
#include "tests/test_framework.h"

using ep::ComputeTextStats;

TEST(TextStats, EmptyTextIsOneLine) {
  auto s = ComputeTextStats("");
  EXPECT_EQ(s.char_count, static_cast<size_t>(0));
  EXPECT_EQ(s.line_count, static_cast<size_t>(1));
  EXPECT_EQ(s.byte_count, static_cast<size_t>(0));
}

TEST(TextStats, AsciiCountsMatchByteCounts) {
  auto s = ComputeTextStats("hello");
  EXPECT_EQ(s.char_count, static_cast<size_t>(5));
  EXPECT_EQ(s.byte_count, static_cast<size_t>(5));
  EXPECT_EQ(s.line_count, static_cast<size_t>(1));
}

TEST(TextStats, MultiByteUtf8CharCountLessThanByteCount) {
  // "café" = c,a,f + 'é' (2 UTF-8 bytes) = 4 characters, 5 bytes.
  auto s = ComputeTextStats("caf\xC3\xA9");
  EXPECT_EQ(s.char_count, static_cast<size_t>(4));
  EXPECT_EQ(s.byte_count, static_cast<size_t>(5));
}

TEST(TextStats, LineCountCountsBoundariesPlusOne) {
  EXPECT_EQ(ComputeTextStats("a\nb\nc").line_count, static_cast<size_t>(3));
  EXPECT_EQ(ComputeTextStats("a\nb\nc\n").line_count, static_cast<size_t>(4));
  EXPECT_EQ(ComputeTextStats("a\r\nb\r\nc").line_count, static_cast<size_t>(3));
  EXPECT_EQ(ComputeTextStats("a\rb\rc").line_count, static_cast<size_t>(3));
}

TEST(TextStats, MixedEolCountsEachBoundaryOnce) {
  EXPECT_EQ(ComputeTextStats("a\r\nb\nc\rd").line_count, static_cast<size_t>(4));
}
