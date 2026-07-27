#include "core/xml_pretty.h"
#include "tests/test_framework.h"

using ep::XmlPrettyPrint;

TEST(XmlPretty, IndentsNestedElements) {
  auto out = XmlPrettyPrint("<a><b><c/></b></a>");
  EXPECT_EQ(out, "<a>\n  <b>\n    <c/>\n  </b>\n</a>");
}

TEST(XmlPretty, CollapsesTextLeafOnOneLine) {
  auto out = XmlPrettyPrint("<root><name>John</name><age>42</age></root>");
  EXPECT_EQ(out, "<root>\n  <name>John</name>\n  <age>42</age>\n</root>");
}

TEST(XmlPretty, ReformatsAlreadyIndentedInput) {
  std::string input =
      "<root>\n"
      "        <a>1</a>\n"
      "   <b>2</b>\n"
      "</root>";
  auto out = XmlPrettyPrint(input);
  EXPECT_EQ(out, "<root>\n  <a>1</a>\n  <b>2</b>\n</root>");
}

TEST(XmlPretty, KeepsXmlDeclarationAndComments) {
  auto out = XmlPrettyPrint("<?xml version=\"1.0\"?><!-- hi --><root/>");
  EXPECT_EQ(out, "<?xml version=\"1.0\"?>\n<!-- hi -->\n<root/>");
}

TEST(XmlPretty, PreservesCDataVerbatim) {
  std::string input = "<root><![CDATA[  keep   this\nexactly  ]]></root>";
  auto out = XmlPrettyPrint(input);
  EXPECT_EQ(out, "<root>\n  <![CDATA[  keep   this\nexactly  ]]>\n</root>");
}

TEST(XmlPretty, PreservesPreContentVerbatim) {
  std::string input = "<root><pre>  line1\n    line2  </pre></root>";
  auto out = XmlPrettyPrint(input);
  EXPECT_EQ(out, "<root>\n  <pre>  line1\n    line2  </pre>\n</root>");
}

TEST(XmlPretty, HandlesMixedQuoteAttributesWithAngleBrackets) {
  std::string input = "<a title='1 > 0' note=\"it's fine\"><b/></a>";
  auto out = XmlPrettyPrint(input);
  EXPECT_EQ(out, "<a title='1 > 0' note=\"it's fine\">\n  <b/>\n</a>");
}

TEST(XmlPretty, HandlesNamespacedTags) {
  auto out = XmlPrettyPrint("<ns:root xmlns:ns=\"urn:x\"><ns:child/></ns:root>");
  EXPECT_EQ(out, "<ns:root xmlns:ns=\"urn:x\">\n  <ns:child/>\n</ns:root>");
}

TEST(XmlPretty, SelfClosingTagWithSpaceBeforeSlash) {
  auto out = XmlPrettyPrint("<root><br /></root>");
  EXPECT_EQ(out, "<root>\n  <br />\n</root>");
}

TEST(XmlPretty, DoesNotCrashOnUnterminatedTag) {
  auto out = XmlPrettyPrint("<root><a");
  EXPECT_TRUE(out.find("<root>") == 0);
}

TEST(XmlPretty, DoesNotCrashOnUnterminatedComment) {
  auto out = XmlPrettyPrint("<root><!-- never closed");
  EXPECT_TRUE(out.find("<root>") == 0);
}

TEST(XmlPretty, EmptyInputProducesEmptyOutput) {
  EXPECT_EQ(XmlPrettyPrint(""), "");
}

TEST(XmlPretty, CustomIndentWidth) {
  auto out = XmlPrettyPrint("<a><b/></a>", 4);
  EXPECT_EQ(out, "<a>\n    <b/>\n</a>");
}
