#pragma once

#include <string>

namespace ep {

// Structural, non-validating SQL formatter (à la a basic dialect-agnostic
// SQL Tools plugin): breaks major clauses (SELECT/FROM/WHERE/JOIN/GROUP
// BY/ORDER BY/HAVING/UNION/INSERT/UPDATE/DELETE/...) onto their own lines,
// indents subqueries, breaks the SELECT column list one column per line,
// and breaks AND/OR onto their own lines. Comments and string literals are
// passed through verbatim. Never rejects input -- malformed/partial SQL is
// reformatted on a best-effort basis, like xml_pretty.
std::string SqlPretty(const std::string& sql, int indent_width = 2);

}  // namespace ep
