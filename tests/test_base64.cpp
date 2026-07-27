#include "core/base64.h"
#include "tests/test_framework.h"

using ep::Base64Decode;
using ep::Base64Encode;

TEST(Base64, EncodeKnownVectors) {
  EXPECT_EQ(Base64Encode(""), "");
  EXPECT_EQ(Base64Encode("f"), "Zg==");
  EXPECT_EQ(Base64Encode("fo"), "Zm8=");
  EXPECT_EQ(Base64Encode("foo"), "Zm9v");
  EXPECT_EQ(Base64Encode("foob"), "Zm9vYg==");
  EXPECT_EQ(Base64Encode("fooba"), "Zm9vYmE=");
  EXPECT_EQ(Base64Encode("foobar"), "Zm9vYmFy");
}

TEST(Base64, EncodeUtf8Bytes) {
  EXPECT_EQ(Base64Encode("caf\xC3\xA9"), "Y2Fmw6k=");
}

TEST(Base64, DecodeKnownVectors) {
  auto d = [](const std::string& s) { return Base64Decode(s); };
  EXPECT_TRUE(d("").IsOk());
  EXPECT_EQ(d("").Value(), "");
  EXPECT_EQ(d("Zg==").Value(), "f");
  EXPECT_EQ(d("Zm8=").Value(), "fo");
  EXPECT_EQ(d("Zm9v").Value(), "foo");
  EXPECT_EQ(d("Zm9vYg==").Value(), "foob");
  EXPECT_EQ(d("Zm9vYmE=").Value(), "fooba");
  EXPECT_EQ(d("Zm9vYmFy").Value(), "foobar");
}

TEST(Base64, RoundTripRandomish) {
  std::string data;
  for (int i = 0; i < 300; i++) data.push_back(static_cast<char>(i % 251));
  auto encoded = Base64Encode(data);
  auto decoded = Base64Decode(encoded);
  EXPECT_TRUE(decoded.IsOk());
  EXPECT_EQ(decoded.Value(), data);
}

TEST(Base64, DecodeIgnoresWhitespace) {
  auto d = Base64Decode("Zm9v\nYmFy\r\n");
  EXPECT_TRUE(d.IsOk());
  EXPECT_EQ(d.Value(), "foobar");
}

TEST(Base64, DecodeRejectsInvalidCharacter) {
  EXPECT_TRUE(Base64Decode("Zm9v!===").IsError());
}

TEST(Base64, DecodeRejectsTruncatedInput) {
  EXPECT_TRUE(Base64Decode("Zg").IsError());
}

TEST(Base64, DecodeRejectsDataAfterPadding) {
  EXPECT_TRUE(Base64Decode("Zg==Zg==").IsError());
}

TEST(Base64, DecodeRejectsPaddingInWrongPosition) {
  EXPECT_TRUE(Base64Decode("Z=g=").IsError());
}
