#include "core/result.h"
#include "tests/test_framework.h"

using ep::Fail;
using ep::Ok;
using ep::Result;

TEST(Result, OkHoldsValue) {
  Result<int> r = Ok(42);
  EXPECT_TRUE(r.IsOk());
  EXPECT_FALSE(r.IsError());
  EXPECT_EQ(r.Value(), 42);
}

TEST(Result, FailHoldsError) {
  Result<int> r = Fail<int>("boom");
  EXPECT_TRUE(r.IsError());
  EXPECT_FALSE(r.IsOk());
  EXPECT_EQ(r.Err().message, "boom");
}

TEST(Result, VoidOkIsOk) {
  Result<void> r = Ok();
  EXPECT_TRUE(r.IsOk());
  EXPECT_TRUE(static_cast<bool>(r));
}

TEST(Result, VoidFailIsError) {
  Result<void> r = Fail("nope");
  EXPECT_TRUE(r.IsError());
  EXPECT_FALSE(static_cast<bool>(r));
  EXPECT_EQ(r.Err().message, "nope");
}
