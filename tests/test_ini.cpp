#include "core/ini.h"
#include "tests/test_framework.h"

using ep::IniDocument;

TEST(Ini, ParsesSectionsAndKeys) {
  std::string text =
      "[window]\n"
      "x=100\n"
      "y=200\n"
      "\n"
      "[view]\n"
      "theme=light\n";
  auto doc = IniDocument::Parse(text);
  EXPECT_EQ(doc.Get("window", "x"), "100");
  EXPECT_EQ(doc.Get("window", "y"), "200");
  EXPECT_EQ(doc.Get("view", "theme"), "light");
}

TEST(Ini, MissingKeyReturnsDefault) {
  auto doc = IniDocument::Parse("[a]\nk=v\n");
  EXPECT_EQ(doc.Get("a", "missing", "fallback"), "fallback");
  EXPECT_EQ(doc.Get("missing_section", "k", "fallback"), "fallback");
}

TEST(Ini, IgnoresCommentsAndBlankLines) {
  std::string text =
      "; a comment\n"
      "# another comment\n"
      "\n"
      "[s]\n"
      "k=v\n";
  auto doc = IniDocument::Parse(text);
  EXPECT_EQ(doc.Get("s", "k"), "v");
}

TEST(Ini, SkipsMalformedLinesWithoutCrashing) {
  std::string text = "[s]\nnotakeyvalue\nk=v\n===\n";
  auto doc = IniDocument::Parse(text);
  EXPECT_EQ(doc.Get("s", "k"), "v");
}

TEST(Ini, TrimsWhitespaceAroundKeysAndValues) {
  auto doc = IniDocument::Parse("[s]\n  k  =  v  \n");
  EXPECT_EQ(doc.Get("s", "k"), "v");
}

TEST(Ini, GetIntAndGetBool) {
  auto doc = IniDocument::Parse("[s]\nn=42\nbad=notanumber\nflag1=1\nflag0=0\n");
  EXPECT_EQ(doc.GetInt("s", "n", -1), 42);
  EXPECT_EQ(doc.GetInt("s", "bad", -1), -1);
  EXPECT_EQ(doc.GetInt("s", "missing", 7), 7);
  EXPECT_TRUE(doc.GetBool("s", "flag1", false));
  EXPECT_FALSE(doc.GetBool("s", "flag0", true));
  EXPECT_TRUE(doc.GetBool("s", "missing", true));
}

TEST(Ini, SetCreatesAndUpdatesEntries) {
  IniDocument doc;
  doc.Set("s", "k", "v1");
  EXPECT_EQ(doc.Get("s", "k"), "v1");
  doc.Set("s", "k", "v2");
  EXPECT_EQ(doc.Get("s", "k"), "v2");
  doc.SetInt("s", "n", 5);
  EXPECT_EQ(doc.GetInt("s", "n", 0), 5);
  doc.SetBool("s", "flag", true);
  EXPECT_TRUE(doc.GetBool("s", "flag", false));
}

TEST(Ini, SerializeRoundTrips) {
  IniDocument doc;
  doc.Set("window", "x", "100");
  doc.Set("window", "y", "200");
  doc.Set("view", "theme", "dark");

  std::string serialized = doc.Serialize();
  auto reparsed = IniDocument::Parse(serialized);
  EXPECT_EQ(reparsed.Get("window", "x"), "100");
  EXPECT_EQ(reparsed.Get("window", "y"), "200");
  EXPECT_EQ(reparsed.Get("view", "theme"), "dark");
}
