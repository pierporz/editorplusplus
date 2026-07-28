#include "core/julian_date.h"

#include <array>
#include <cstdio>

namespace ep {

bool IsLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

namespace {
constexpr std::array<int, 12> kDaysInMonth = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
}  // namespace

int DayOfYear(int year, int month, int day) {
  int total = day;
  for (int m = 1; m < month; m++) {
    total += kDaysInMonth[m - 1];
    if (m == 2 && IsLeapYear(year)) total += 1;
  }
  return total;
}

std::string JulianDate(int year, int month, int day) {
  int century = year / 100 - 19;
  int yy = year % 100;
  int ddd = DayOfYear(year, month, day);

  char buf[16];
  std::snprintf(buf, sizeof(buf), "%d%02d%03d", century, yy, ddd);
  return std::string(buf);
}

}  // namespace ep
