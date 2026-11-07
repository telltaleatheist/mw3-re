// test_pm_ai_attackers.py: PM's list of who hit a pilot, one call at a time.
#include <cstdint>
#include <memory>

#include "mech3/zai_attackers_pm.h"

using namespace mech3::ai;

namespace {
std::unique_ptr<AttackersPm> g;
}

extern "C" {
void probe_attackers_new(float rate, float limit, float clock) { g = std::make_unique<AttackersPm>(rate, limit, clock); }
int32_t probe_attackers_age(uint32_t who, float amount, float clock) { return g->age(who, amount, clock); }
void probe_attackers_add(uint32_t who, float amount, float clock) { g->add(who, amount, clock); }
int32_t probe_attackers_within(float secs, float clock) { return g->hit_within(secs, clock); }
uint32_t probe_attackers_hardest(float clock) { return g->hardest(clock); }
uint32_t probe_attackers_latest(float clock) { return g->latest(clock); }
int32_t probe_attackers_dump(uint32_t* out, int32_t max) {
  int32_t n = 0;
  for (const AttackerPm& r : g->list) {
    if (n == max) break;
    const float f[3] = {r.sum, r.last, r.first};
    __builtin_memcpy(out + 4 * n, f, 12);
    out[4 * n + 3] = r.attacker;
    ++n;
  }
  return n;
}
}
