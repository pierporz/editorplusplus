#include "core/sql_pretty.h"
#include "tests/test_framework.h"

using ep::SqlPretty;

TEST(SqlPretty, SimpleSelect) {
  auto out = SqlPretty("select a, b from t");
  EXPECT_EQ(out, "SELECT\n  a,\n  b\nFROM t");
}

TEST(SqlPretty, WhereWithAndOr) {
  auto out = SqlPretty("select a from t where x = 1 and y = 2 or z = 3");
  EXPECT_EQ(out,
            "SELECT\n  a\nFROM t\nWHERE x = 1\n  AND y = 2\n  OR z = 3");
}

TEST(SqlPretty, JoinOnSameLine) {
  auto out = SqlPretty("select a from t left join u on t.id = u.id");
  EXPECT_EQ(out, "SELECT\n  a\nFROM t\nLEFT JOIN u ON t.id = u.id");
}

TEST(SqlPretty, InnerJoinAndGroupByOrderBy) {
  auto out = SqlPretty(
      "select a, count(*) from t inner join u on t.id = u.id "
      "group by a order by a");
  EXPECT_EQ(out,
            "SELECT\n  a,\n  count(*)\nFROM t\nINNER JOIN u ON t.id = u.id\n"
            "GROUP BY a\nORDER BY a");
}

TEST(SqlPretty, FunctionCallStaysTight) {
  // Function/identifier names are kept as written (not upper-cased), unlike
  // clause and other reserved keywords.
  auto out = SqlPretty("select count(*) from t");
  EXPECT_EQ(out, "SELECT\n  count(*)\nFROM t");
}

TEST(SqlPretty, InListGetsSpaceBeforeParen) {
  auto out = SqlPretty("select a from t where a in (1, 2, 3)");
  EXPECT_EQ(out, "SELECT\n  a\nFROM t\nWHERE a IN (1, 2, 3)");
}

TEST(SqlPretty, Subquery) {
  auto out = SqlPretty("select a from (select a from t) as sub");
  EXPECT_EQ(out, "SELECT\n  a\nFROM (\n  SELECT\n    a\n  FROM t\n) AS sub");
}

TEST(SqlPretty, InsertInto) {
  // The column-list paren right after the table name renders tight, same as
  // a function call would (see FunctionCallStaysTight) -- telling the two
  // apart would need real grammar awareness, not just token spacing rules.
  auto out = SqlPretty("insert into t (a, b) values (1, 2)");
  EXPECT_EQ(out, "INSERT INTO t(a, b)\nVALUES (1, 2)");
}

TEST(SqlPretty, UpdateSet) {
  auto out = SqlPretty("update t set a = 1 where id = 2");
  EXPECT_EQ(out, "UPDATE t\nSET a = 1\nWHERE id = 2");
}

TEST(SqlPretty, DeleteFrom) {
  auto out = SqlPretty("delete from t where id = 1");
  EXPECT_EQ(out, "DELETE FROM t\nWHERE id = 1");
}

TEST(SqlPretty, UnionAll) {
  auto out = SqlPretty("select a from t union all select a from u");
  EXPECT_EQ(out, "SELECT\n  a\nFROM t\nUNION ALL\nSELECT\n  a\nFROM u");
}

TEST(SqlPretty, LineCommentPreserved) {
  // Comments render inline with whatever precedes them, keeping them
  // visually attached to what they're commenting on.
  auto out = SqlPretty("select a -- pick a\nfrom t");
  EXPECT_EQ(out, "SELECT\n  a -- pick a\nFROM t");
}

TEST(SqlPretty, BlockCommentPreserved) {
  auto out = SqlPretty("select /* all */ a from t");
  EXPECT_EQ(out, "SELECT\n  /* all */ a\nFROM t");
}

TEST(SqlPretty, StringLiteralPreservedVerbatim) {
  auto out = SqlPretty("select a from t where s = 'it''s a test'");
  EXPECT_EQ(out, "SELECT\n  a\nFROM t\nWHERE s = 'it''s a test'");
}

TEST(SqlPretty, DotNoSpace) {
  auto out = SqlPretty("select t.a from t");
  EXPECT_EQ(out, "SELECT\n  t.a\nFROM t");
}

TEST(SqlPretty, EmptyInputProducesEmptyOutput) { EXPECT_EQ(SqlPretty(""), ""); }

TEST(SqlPretty, CustomIndentWidth) {
  auto out = SqlPretty("select a from t", 4);
  EXPECT_EQ(out, "SELECT\n    a\nFROM t");
}

TEST(SqlPretty, DoesNotCrashOnUnterminatedString) {
  auto out = SqlPretty("select a from t where s = 'oops");
  EXPECT_TRUE(out.find("SELECT") == 0);
}

TEST(SqlPretty, DoesNotCrashOnUnterminatedBlockComment) {
  auto out = SqlPretty("select a /* never closed");
  EXPECT_TRUE(out.find("SELECT") == 0);
}
