#include "core/session.h"
#include "tests/test_framework.h"

using ep::DeserializeSession;
using ep::SerializeSession;
using ep::SessionState;
using ep::TabState;

TEST(Session, RoundTripsMultipleTabs) {
  SessionState state;
  TabState a;
  a.path = "C:\\Users\\foo\\file.txt";
  a.cursor_pos = 120;
  a.scroll_top_line = 5;
  a.sel_start = 10;
  a.sel_end = 20;
  state.tabs.push_back(a);

  TabState b;
  b.backup_path = "backup\\untitled_1.tmp";
  state.tabs.push_back(b);

  state.active_tab_index = 1;

  std::string text = SerializeSession(state);
  SessionState back = DeserializeSession(text);

  EXPECT_EQ(back.tabs.size(), static_cast<size_t>(2));
  EXPECT_EQ(back.active_tab_index, 1);
  EXPECT_EQ(back.tabs[0].path, "C:\\Users\\foo\\file.txt");
  EXPECT_EQ(back.tabs[0].cursor_pos, 120);
  EXPECT_EQ(back.tabs[0].scroll_top_line, 5);
  EXPECT_EQ(back.tabs[0].sel_start, 10);
  EXPECT_EQ(back.tabs[0].sel_end, 20);
  EXPECT_EQ(back.tabs[1].backup_path, "backup\\untitled_1.tmp");
  EXPECT_TRUE(back.tabs[1].path.empty());
}

TEST(Session, EmptyTextDeserializesToEmptySession) {
  SessionState back = DeserializeSession("");
  EXPECT_TRUE(back.tabs.empty());
  EXPECT_EQ(back.active_tab_index, 0);
}

TEST(Session, CorruptTabCountDoesNotCrash) {
  SessionState back = DeserializeSession("[session]\ntab_count=notanumber\n");
  EXPECT_TRUE(back.tabs.empty());
}

TEST(Session, OutOfRangeActiveTabClampsToZero) {
  SessionState back = DeserializeSession(
      "[session]\ntab_count=1\nactive_tab=99\n[tab0]\npath=a.txt\n");
  EXPECT_EQ(back.active_tab_index, 0);
}

TEST(Session, NegativeTabCountYieldsEmptyTabs) {
  SessionState back = DeserializeSession("[session]\ntab_count=-5\n");
  EXPECT_TRUE(back.tabs.empty());
}
