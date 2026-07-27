#pragma once

#include <string>

namespace ep {

// Structural, non-validating XML indenter (à la Notepad++'s XML Tools):
// handles the XML declaration, comments, CDATA, self-closing tags, mixed
// quote styles in attributes, and namespaced tag names. Never rejects
// input -- malformed/partial XML is reformatted on a best-effort basis.
// CDATA sections and the content of <pre>...</pre> elements are copied
// through byte-for-byte, since whitespace is significant there.
std::string XmlPrettyPrint(const std::string& xml, int indent_width = 2);

}  // namespace ep
