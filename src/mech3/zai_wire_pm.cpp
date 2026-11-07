#include "mech3/zai_wire_pm.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mech3::ai {

namespace {
std::map<std::string, int>& missing_counts() {
  static std::map<std::string, int> m;
  return m;
}
}  // namespace

bool pm_ai_on() {
  static const bool on = [] {
    const char* v = std::getenv("PM_AI");
    return v && std::strcmp(v, "1") == 0;
  }();
  return on;
}

void pm_hook_missing(const char* key) {
  if (++missing_counts()[key] == 1) std::printf("mw3: pm ai: no answer yet for %s\n", key);
}

const std::map<std::string, int>& pm_hooks_missing() { return missing_counts(); }
void pm_hooks_missing_clear() { missing_counts().clear(); }

}  // namespace mech3::ai
