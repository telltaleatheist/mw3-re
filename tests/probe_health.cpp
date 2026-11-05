// A mech's health, its limp and top speed (mech3/mech_damage, mech_move) for tests/differential/test_pm_health.py.
#include <cstdint>
#include <cstring>
#include <vector>

#include "mech3/mech.h"
#include "mech3/mech_config.h"
#include "mech3/mech_control.h"
#include "mech3/mech_move.h"

using namespace mech3;

namespace {
std::vector<int32_t> g_log;
void log(std::initializer_list<int32_t> v) {
  g_log.push_back(static_cast<int32_t>(v.size()));
  g_log.insert(g_log.end(), v);
}
int32_t bits(float f) {
  int32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}
const float* g_table = nullptr;
struct HLeg : Leg {
  int at = 0;
  float get_internal(int) const override { return g_table[at]; }
  void set_internal(float v, int s) override { log({5, at, bits(v), s}); }
  void set_armor(float v, int s) override { log({6, at, bits(v), s}); }
};
struct HArm : Arm {
  int at = 0;
  float get_internal(int) const override { return g_table[at]; }
  void set_internal(float v, int s) override { log({5, at, bits(v), s}); }
  void set_armor(float v, int s) override { log({6, at, bits(v), s}); }
};
struct HHead : Head {
  float get_internal(int) const override { return g_table[0]; }
  void set_internal(float v, int s) override { log({5, 0, bits(v), s}); }
  void set_armor(float v, int s) override { log({6, 0, bits(v), s}); }
};
struct HTorso : Torso {
  float get_internal(int s) const override { return g_table[5 + s]; }
  void set_internal(float v, int s) override { log({5, 5, bits(v), s}); }
  void set_armor(float v, int s) override { log({6, 5, bits(v), s}); }
};
struct World_ {
  Mech m;
  MechData pilot;
  MechControls c;
  HLeg legs[2];
  HArm arms[2];
  HHead head;
  HTorso torso;
  MechAnim anims[13];
};
World_* g = nullptr;
int anim_of(Handle a) {
  for (int i = 0; i < 13; ++i)
    if (a == &g->anims[i]) return i;
  return -1;
}
}  // namespace

extern "C" {
// op 0 health, 1 start_limp(arg), 2 masc(arg), 3 the kill, 4 the torso's end. in: parts' structure [8] (head, leg, leg, arm, arm, torso 0 1 2),
// cached, walk hi, trot lo, limp speed, limp amount, max speed, walk speed, masc time, throttle; ints: damaged, state,
// invincible, limp, masc fitted, player, roster, engine, mech; anims bit by bit. out: result, the floats, limp, log.
int32_t probe_pm_health(int32_t op, int32_t arg, const float* f, const int32_t* i, uint32_t present, float* result,
                        float* out, int32_t* limp, int32_t* log_out, int32_t cap) {
  static World_ state;
  g = &state;
  static float table[8];
  std::memcpy(table, f, sizeof table);
  g_table = table;
  Mech& m = g->m;
  m = Mech{};
  for (int k = 0; k < 2; ++k) {
    g->legs[k].at = 1 + k, m.legs[k] = &g->legs[k];
    g->arms[k].at = 3 + k, m.arms[k] = &g->arms[k];
  }
  m.head = &g->head, m.torso = &g->torso;
  m.salvage_mech = true;
  g->pilot = MechData{};
  g->pilot.trans[4][1] = f[9], g->pilot.trans[5][0] = f[10], g->pilot.limp_speed = f[11];
  g->pilot.roster = i[6];
  m.pilot = &g->pilot;
  static Config cfg(0, "probe");
  cfg.mech = i[8], cfg.engine = i[7];
  m.config = &cfg;
  g->c = MechControls{};
  g->c.throttle = f[16], g->c.owner = &m;
  m.controls = &g->c;
  m.x40c = f[8], m.limp_amount = f[12], m.max_speed = f[13], m.walk_speed = f[14], m.masc_time = f[15];
  m.damaged = i[0] != 0, m.anim_state = i[1], m.invincible = i[2] != 0, m.limp = i[3], m.equipment[12] = i[4];
  for (int k = 0; k < 13; ++k) m.anims[k] = present >> k & 1 ? &g->anims[k] : nullptr;
  world().mission_player = i[5] ? &m : nullptr;
  g_log.clear();
  DamageCalls& c = damage_calls();
  const DamageCalls saved = c;
  c = DamageCalls{};
  c.anim_speed = [](Handle a, float v) { log({1, anim_of(a), bits(v)}); };
  c.gait_speed = [](Mech&, float v) { log({2, bits(v)}); };
  c.blend_anim = [](Handle a, Handle b) -> Handle {
    log({3, anim_of(a), anim_of(b)});
    return &g->anims[12];
  };
  HeatCalls& hc = heat_calls();
  const HeatCalls saved_h = hc;
  hc = HeatCalls{};
  hc.play_mech_sound = [](int s) { log({4, s}); };
  float r = 0.0f;
  if (op == 0) r = m.health();
  else if (op == 1) r = m.start_limp(arg, false) ? 1.0f : 0.0f;
  else if (op == 2) m.masc(arg != 0);
  else if (op == 3) m.kill();  // what the destroy valve 0x004f5210 calls
  else m.destroy_part(1);
  c = saved, hc = saved_h;
  world().mission_player = nullptr;
  *result = r;
  out[0] = m.x40c, out[1] = m.limp_amount, out[2] = m.max_speed, out[3] = m.walk_speed, out[4] = m.masc_time;
  out[5] = g->c.throttle, out[6] = m.damaged ? 1.0f : 0.0f, out[7] = m.invincible ? 1.0f : 0.0f;
  out[8] = m.salvage_mech ? 1.0f : 0.0f;
  *limp = m.limp;
  const int32_t n = static_cast<int32_t>(g_log.size()) < cap ? static_cast<int32_t>(g_log.size()) : cap;
  std::memcpy(log_out, g_log.data(), 4 * n);
  return n;
}
}  // extern "C"
