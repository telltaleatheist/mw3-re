#pragma once
#include <cstdio>
#include <functional>
#include <vector>

// A minimal test runner: no dependencies, so the tests build anywhere the game does.
namespace check {

struct Test {
  const char* name;
  std::function<void()> fn;
};
std::vector<Test>& registry();
extern int failures;

struct Register {
  Register(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

}  // namespace check

#define CHECK_CAT2(a, b) a##b
#define CHECK_CAT(a, b) CHECK_CAT2(a, b)
#define TEST(name)                                                     \
  static void name();                                                  \
  static check::Register CHECK_CAT(reg_, name)(#name, name);           \
  static void name()
#define EXPECT(cond)                                                                   \
  do {                                                                                 \
    if (!(cond)) {                                                                     \
      std::printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);                  \
      ++check::failures;                                                               \
    }                                                                                  \
  } while (0)
