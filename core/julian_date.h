#pragma once

#include <string>

namespace ep {

bool IsLeapYear(int year);

// 1-based day of year (1-366) for the given calendar date. `month` is 1-12,
// `day` is 1-31; the caller is expected to pass an already-valid calendar
// date (this only ever originates from the system clock, not user input).
int DayOfYear(int year, int month, int day);

// CYYDDD julian-style date string: C is the century past 1900 (0 for the
// 1900s, 1 for the 2000s, 2 for the 2100s, ...), YY the two-digit year
// within the century, DDD the zero-padded day of year.
std::string JulianDate(int year, int month, int day);

}  // namespace ep
