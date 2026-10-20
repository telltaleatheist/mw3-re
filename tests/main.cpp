#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
  // MW3_TEST=name runs only the tests whose names contain it; a,b,c any of them.
  const char* only = std::getenv("MW3_TEST");
  auto wanted = [only](const char* name) {
    if (!only) return true;
    for (const char* p = only; *p;) {
      const char* e = std::strchr(p, ',');
      const std::string part(p, e ? e - p : std::strlen(p));
      if (!part.empty() && std::strstr(name, part.c_str())) return true;
      if (!e) break;
      p = e + 1;
    }
    return false;
  };
  for (const check::Test& t : check::registry()) {
    if (!wanted(t.name)) continue;
    const int before = check::failures;
    t.fn();
    std::printf("%s %s\n", check::failures == before ? "ok  " : "FAIL", t.name);
  }
  std::printf("%zu tests, %d failed checks\n", check::registry().size(), check::failures);
  return check::failures == 0 ? 0 : 1;
}
