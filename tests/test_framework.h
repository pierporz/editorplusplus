#pragma once

// Minimal header-only test runner. No gtest, no dependencies.
//
// Usage:
//   TEST(SuiteName, CaseName) { EXPECT_EQ(1 + 1, 2); }
//   int main() { return ep_test::RunAll(); }

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace ep_test {

struct Case {
  std::string name;
  std::function<void()> fn;
};

inline std::vector<Case>& Registry() {
  static std::vector<Case> cases;
  return cases;
}

inline int& FailureCount() {
  static int count = 0;
  return count;
}

inline bool& CurrentCaseFailed() {
  static bool failed = false;
  return failed;
}

struct Registrar {
  Registrar(const std::string& name, std::function<void()> fn) {
    Registry().push_back({name, std::move(fn)});
  }
};

inline void ReportFailure(const char* file, int line, const std::string& msg) {
  std::fprintf(stderr, "  FAILED %s:%d: %s\n", file, line, msg.c_str());
  CurrentCaseFailed() = true;
  FailureCount()++;
}

inline int RunAll() {
  int passed = 0;
  for (auto& c : Registry()) {
    CurrentCaseFailed() = false;
    c.fn();
    if (CurrentCaseFailed()) {
      std::fprintf(stderr, "[FAIL] %s\n", c.name.c_str());
    } else {
      passed++;
    }
  }
  int total = static_cast<int>(Registry().size());
  std::fprintf(stdout, "%d/%d tests passed\n", passed, total);
  return FailureCount() == 0 ? 0 : 1;
}

}  // namespace ep_test

#define EP_TEST_CONCAT_INNER(a, b) a##b
#define EP_TEST_CONCAT(a, b) EP_TEST_CONCAT_INNER(a, b)

#define TEST(suite, name)                                                \
  static void EP_TEST_CONCAT(test_fn_, __LINE__)();                      \
  static ep_test::Registrar EP_TEST_CONCAT(test_reg_, __LINE__)(         \
      #suite "." #name, EP_TEST_CONCAT(test_fn_, __LINE__));             \
  static void EP_TEST_CONCAT(test_fn_, __LINE__)()

#define EXPECT_TRUE(cond)                                                \
  do {                                                                   \
    if (!(cond)) {                                                      \
      ep_test::ReportFailure(__FILE__, __LINE__, "EXPECT_TRUE(" #cond ")"); \
    }                                                                    \
  } while (0)

#define EXPECT_FALSE(cond)                                               \
  do {                                                                   \
    if (cond) {                                                         \
      ep_test::ReportFailure(__FILE__, __LINE__, "EXPECT_FALSE(" #cond ")"); \
    }                                                                    \
  } while (0)

#define EXPECT_EQ(a, b)                                                  \
  do {                                                                   \
    if (!((a) == (b))) {                                                \
      ep_test::ReportFailure(__FILE__, __LINE__, #a " == " #b);         \
    }                                                                    \
  } while (0)

#define EXPECT_NE(a, b)                                                  \
  do {                                                                   \
    if ((a) == (b)) {                                                   \
      ep_test::ReportFailure(__FILE__, __LINE__, #a " != " #b);         \
    }                                                                    \
  } while (0)
