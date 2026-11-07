// test_pm_window.py: a mech's two slots of PM's frame window, 0x0041b900 and 0x0041b930, their calls logged.
#include <cstdint>

#include "mech3/app_mech.h"
#include "mech3/mech.h"
#include "mech3/mech_lod.h"

using mech3::CloudBand;

namespace {
uint32_t* g_log;
int g_n, g_pilot, g_head;
void* g_mech;
void note(uint32_t c) { g_log[g_n++] = c; }
}  // namespace

static CloudBand g_band;
static float g_y;
static int g_viewed;
extern "C" {
// 0x004b2b40: slot 0 the mech's. Log: 1 camera, 2 prep, 3 kill; out the +0x658 bit.
int probe_pm_cloud(int on, int death_on, float low, float high, float y, int active, int state, int viewed, uint32_t* log, int* flag) {
  static mech3::Mech m;
  g_log = log, g_n = 0, g_band = CloudBand{on != 0, death_on != 0, low, high}, g_y = y, g_viewed = viewed;
  m.active = active != 0, m.anim_state = state, m.x634 = 0;
  mech3::CloudCalls& c = mech3::cloud_calls();
  const mech3::CloudCalls was = c;
  c.band = []() { return g_band; };
  c.height = [](mech3::Mech&) { return g_y; };
  c.viewed = [](mech3::Mech&) { return g_viewed != 0; };
  c.camera = [](int32_t id) { note(1), g_log[g_n++] = static_cast<uint32_t>(id); };
  c.prep = [](mech3::Mech&) { note(2); };
  c.kill = [](mech3::Mech&) { note(3); };
  mech3::cloud_kill(m);
  c = was;
  *flag = static_cast<int>(m.x634 & 1);
  return g_n;
}
int probe_pm_cloud_band(int on, int death_on, float low, float high, float y) { return CloudBand{on != 0, death_on != 0, low, high}.holds(y) ? 1 : 0; }
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
