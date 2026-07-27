#include "core/json_pretty.h"
#include "tests/test_framework.h"

using ep::JsonMinify;
using ep::JsonPrettyPrint;

TEST(JsonPretty, EmptyObjectAndArray) {
  auto r = JsonPrettyPrint("{}");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.output, "{}");

  auto r2 = JsonPrettyPrint("[]");
  EXPECT_TRUE(r2.ok);
  EXPECT_EQ(r2.output, "[]");
}

TEST(JsonPretty, SimpleObject) {
  auto r = JsonPrettyPrint(R"({"a":1,"b":true})");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.output, "{\n  \"a\": 1,\n  \"b\": true\n}");
}

TEST(JsonPretty, NestedArrayAndObject) {
  auto r = JsonPrettyPrint(R"({"list":[1,2,{"x":null}]})");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.output,
            "{\n  \"list\": [\n    1,\n    2,\n    {\n      \"x\": null\n    "
            "}\n  ]\n}");
}

TEST(JsonPretty, TopLevelScalarsAndCustomIndent) {
  EXPECT_TRUE(JsonPrettyPrint("42").ok);
  EXPECT_TRUE(JsonPrettyPrint("\"hi\"").ok);
  EXPECT_TRUE(JsonPrettyPrint("true").ok);
  EXPECT_TRUE(JsonPrettyPrint("null").ok);

  auto r = JsonPrettyPrint(R"({"a":1})", 4);
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.output, "{\n    \"a\": 1\n}");
}

TEST(JsonPretty, ExponentialAndNegativeNumbers) {
  auto r = JsonPrettyPrint(R"([1.5e10,-3,-4.2E-3,0])");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.output, "[\n  1.5e10,\n  -3,\n  -4.2E-3,\n  0\n]");
}

TEST(JsonPretty, UnicodeEscapeAndRawUtf8PassThroughVerbatim) {
  // \u-escapes and raw multi-byte UTF-8 are both copied through unchanged --
  // the formatter re-emits source string literals verbatim rather than
  // decoding/re-encoding them.
  auto r = JsonPrettyPrint(R"({"s":"aé\nb"})");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.output, "{\n  \"s\": \"aé\\nb\"\n}");

  auto r2 = JsonPrettyPrint("{\"s\":\"caf\xC3\xA9\"}");
  EXPECT_TRUE(r2.ok);
  EXPECT_EQ(r2.output, "{\n  \"s\": \"caf\xC3\xA9\"\n}");
}

TEST(JsonPretty, DeepNestingDoesNotCrash) {
  std::string input;
  const int depth = 20000;
  for (int i = 0; i < depth; i++) input += "[";
  input += "1";
  for (int i = 0; i < depth; i++) input += "]";
  auto r = JsonPrettyPrint(input);
  EXPECT_TRUE(r.ok);
}

TEST(JsonPretty, InvalidJsonReportsLineAndColumnAndLeavesOutputEmpty) {
  auto r = JsonPrettyPrint("{\n  \"a\": ,\n}");
  EXPECT_FALSE(r.ok);
  EXPECT_TRUE(r.output.empty());
  EXPECT_EQ(r.error.line, static_cast<size_t>(2));
}

TEST(JsonPretty, TrailingCommaIsInvalid) {
  EXPECT_FALSE(JsonPrettyPrint(R"({"a":1,})").ok);
  EXPECT_FALSE(JsonPrettyPrint(R"([1,2,])").ok);
}

TEST(JsonPretty, TrailingContentIsInvalid) {
  EXPECT_FALSE(JsonPrettyPrint("{} garbage").ok);
}

TEST(JsonPretty, UnescapedControlCharInStringIsInvalid) {
  std::string input = "{\"a\":\"x\ty\"}";
  EXPECT_FALSE(JsonPrettyPrint(input).ok);
}

TEST(JsonPretty, LeadingZeroIsInvalid) {
  EXPECT_FALSE(JsonPrettyPrint("01").ok);
}

TEST(JsonMinifyTest, RemovesAllInsignificantWhitespace) {
  auto r = JsonMinify(R"({
    "a": 1,
    "b": [1, 2, 3]
  })");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(r.output, R"({"a":1,"b":[1,2,3]})");
}

TEST(JsonMinifyTest, EmptyInputIsInvalid) {
  EXPECT_FALSE(JsonMinify("").ok);
  EXPECT_FALSE(JsonMinify("   \n  ").ok);
}
