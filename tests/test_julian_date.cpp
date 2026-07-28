#include "core/julian_date.h"
#include "tests/test_framework.h"

using ep::DayOfYear;
using ep::IsLeapYear;
using ep::JulianDate;

TEST(JulianDate, LeapYearRules) {
  EXPECT_TRUE(IsLeapYear(2024));
  EXPECT_TRUE(IsLeapYear(2000));   // divisible by 400
  EXPECT_FALSE(IsLeapYear(1900));  // divisible by 100, not 400
  EXPECT_FALSE(IsLeapYear(2023));
}

TEST(JulianDate, DayOfYearBoundaries) {
  EXPECT_EQ(DayOfYear(2026, 1, 1), 1);
  EXPECT_EQ(DayOfYear(2023, 12, 31), 365);
  EXPECT_EQ(DayOfYear(2024, 12, 31), 366);  // leap year
  EXPECT_EQ(DayOfYear(2023, 3, 1), 60);     // Jan(31)+Feb(28)+1
  EXPECT_EQ(DayOfYear(2024, 3, 1), 61);     // leap Feb has 29
}

TEST(JulianDate, FormatIsCYYDDD) {
  EXPECT_EQ(JulianDate(1990, 1, 1), std::string("090001"));
  EXPECT_EQ(JulianDate(2026, 7, 28), std::string("126209"));
  EXPECT_EQ(JulianDate(1999, 12, 31), std::string("099365"));
}

TEST(JulianDate, CenturyBoundary) {
  EXPECT_EQ(JulianDate(1999, 12, 31), std::string("099365"));
  EXPECT_EQ(JulianDate(2000, 1, 1), std::string("100001"));
}
