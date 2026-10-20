#include <cstdio>

#include "check.h"

namespace check {
std::vector<Test>& registry() {
  static std::vector<Test> tests;
  return tests;
}
int failures = 0;
}  // namespace check

int main() {
  std::setvbuf(stdout, nullptr, _IOLBF, 0);  // a test that crashes still shows where
  for (const check::Test& t : check::registry()) {
    const int before = check::failures;
    t.fn();
    std::printf("%s %s\n", check::failures == before ? "ok  " : "FAIL", t.name);
  }
  std::printf("%zu tests, %d failed checks\n", check::registry().size(), check::failures);
  return check::failures == 0 ? 0 : 1;
}
