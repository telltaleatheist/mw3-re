// PM's power on/off 0x0041dab0 and power-down completion 0x0041d920 (test_pm_power.py).
#include <cstdint>
#include <vector>

#include "mech3/mech.h"
#include "mech3/mech_collide.h"

using namespace mech3;

namespace {
std::vector<int32_t> g_log;
Mech g_m, g_player;
int32_t g_next, g_screen, g_byte;
void log3(int32_t a, int32_t b, int32_t c) { g_log.push_back(a), g_log.push_back(b), g_log.push_back(c); }
Sound snd(int32_t v) { return reinterpret_cast<Sound>(static_cast<intptr_t>(v)); }
int32_t snd(Sound s) { return static_cast<int32_t>(reinterpret_cast<intptr_t>(s)); }
}  // namespace

extern "C" {
// i: pending, shutdown, powered down, state, no power down, the HUD's saved byte, 3 loops, up and down sounds,
// detail, the view's mech, a screen, the screen's byte, the network class's, the mission's player targets it.
int probe_power(int op, float* f, int32_t* i, int32_t* log, int cap) {
  Mech& m = g_m;
  m = Mech{};
  m.vec_3c = {f[0], f[1], f[2]};
  m.power_down_pending = i[0], m.heat_shutdown = i[1], m.powered_down = i[2];
  m.anim_state = i[3], m.no_power_down = i[4], m.hud_saved = static_cast<uint8_t>(i[5]);
  for (int k = 0; k < 3; ++k) m.loops()[k] = snd(i[6 + k]);
  m.power_up_sound() = snd(i[9]), m.power_down_sound() = snd(i[10]);
  m.detail = static_cast<uint32_t>(i[11]);
  m.remote = i[15] != 0;
  world().player = i[12] ? &m : nullptr;
  g_screen = i[13], g_byte = i[14], g_next = 100;
  g_player.target = i[16] ? reinterpret_cast<decltype(g_player.target)>(uintptr_t{0x10}) : nullptr;
  world().mission_player = &g_player;
  g_log.clear();
  HeatCalls& c = heat_calls();
  const HeatCalls c0 = c;
  c.stop_sound = [](Sound s) { log3(3, snd(s), 0); };
  c.screen = [](Mech&) -> void* { return g_screen ? &g_screen : nullptr; };
  c.stop_controls = [](Mech&, int a) { log3(9, a, 0); };
  c.screen_power_down = [](void*) { log3(10, g_byte, 0); return static_cast<uint8_t>(g_byte); };
  c.screen_power_up = [](void*, uint8_t b) { log3(11, b, 0); };
  c.play_sound_at = [](Mech&, int id) {
    log3(2, id, g_next);
    return snd(g_next++);
  };
  c.headlight_off = [](Mech&) { log3(12, 0, 0); };
  c.clear_target = [](Mech&) { log3(13, 0, 0); };
  auto keep_contact = collide_calls().contact_mech;
  collide_calls().contact_mech = [](Handle) -> Mech* { return &g_m; };
  if (op == 2) m.finish_power_down();
  else m.power(op);
  f[0] = m.vec_3c.x, f[1] = m.vec_3c.y, f[2] = m.vec_3c.z;
  i[0] = m.power_down_pending, i[1] = m.heat_shutdown, i[2] = m.powered_down;
  i[3] = m.anim_state, i[4] = m.no_power_down, i[5] = m.hud_saved;
  for (int k = 0; k < 3; ++k) i[6 + k] = snd(m.loops()[k]);
  i[9] = snd(m.power_up_sound()), i[10] = snd(m.power_down_sound());
  c = c0;
  collide_calls().contact_mech = keep_contact;
  world().player = world().mission_player = nullptr;
  g_player.target = nullptr;
  const int n = static_cast<int>(g_log.size());
  for (int k = 0; k < n && k < cap; ++k) log[k] = g_log[k];
  return n;
}
}  // extern "C"
