// test_pm_window.py: a mech's two slots of PM's frame window, 0x0041b900 and 0x0041b930, their calls logged.
#include <cstdint>

#include "mech3/app_mech.h"
#include "mech3/mech.h"

namespace {
uint32_t* g_log;
int g_n, g_pilot, g_head;
void* g_mech;
void note(uint32_t c) { g_log[g_n++] = c; }
}  // namespace

extern "C" {
// slot 0: 0x0041b900, 1: 0x0041b930. Returns the calls logged.
int probe_pm_window(int slot, int pilot, int head, float dt, uint32_t* log, float* frame_dt, int* attacker_kept) {
  static mech3::Mech m;
  g_log = log, g_n = 0, g_pilot = pilot, g_head = head, g_mech = &m;
  m.frame_dt = -1.0f;
  m.attacker = &m;
  mech3::MechFrameCalls& c = mech3::mech_frame_calls();
  const mech3::MechFrameCalls was = c;
  c.aim = [](void*) { note(1); };
  c.is_pilot = [](void* p) { return p == g_mech && g_pilot != 0; };
  c.gauge = [](void*) { note(2); };
  c.head_out = [](void*) { if (g_head) note(3); };
  c.head_in = [](void*, float) { if (g_head) note(4); };
  if (slot == 0) mech3::mech_before_frame(&m);
  else mech3::mech_after_frame(&m, dt);
  c = was;
  *frame_dt = m.frame_dt;
  *attacker_kept = m.attacker != nullptr;
  return g_n;
}
}
