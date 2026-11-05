// Pirate's Moon's AI, the port's side of tests/differential/test_pm_ai_*.py.
#include <cstdint>

#include "mech3/zai_valve.h"

namespace ai = mech3::ai;

// test_pm_ai_start.py: the mission-start listeners (0x004944e0, 0x004e4b00).
// In: which valves the data named and their values; out: their values after, and
// whether "bapon" exists then.
extern "C" void probe_pm_ai_start(int32_t named_start, int32_t start, int32_t named_bap, int32_t bap, int32_t player_bap,
                                  int32_t* out) {
  ai::valves_clear();
  if (named_start) ai::valve_get("on_mission_start")->set(start);
  if (named_bap) ai::valve_get("bapon")->set(bap);
  ai::valves_mission_started(player_bap);
  const ai::Valve* s = ai::valve_find("on_mission_start");
  const ai::Valve* b = ai::valve_find("bapon");
  out[0] = s ? s->value->get() : -1;
  out[1] = b ? b->value->get() : -1;
  ai::valves_clear();
}
