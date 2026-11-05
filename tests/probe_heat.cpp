// The heat update (mech3::update_heat) for tests/differential/test_pm_heat.py: FIELDS there is the order here.
#include <cstdint>
#include <cstring>
#include <vector>

#include "crt/msvc_rand.h"
#include "mech3/mech.h"
#include "mech3/mech_skis.h"

namespace {
mech3::Mech g_mech;
mech3::Skis g_skis;
mech3::Foot g_feet[2];
mech3::Vec3 g_pos;
struct {
  float controls = 0, health = 1;
  int mission = 0, option = 1, screen = 0, ammo = 0;
  uintptr_t next_sound = 1;
} g_script;
std::vector<int32_t> g_log;

void log3(int32_t a, int32_t b, int32_t c) { g_log.insert(g_log.end(), {a, b, c}); }
int32_t fbits(float f) {
  int32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}
float bitsf(int32_t i) {
  float f;
  std::memcpy(&f, &i, 4);
  return f;
}
int32_t snd(mech3::Sound s) { return static_cast<int32_t>(reinterpret_cast<uintptr_t>(s)); }
mech3::Sound snd(int32_t i) { return reinterpret_cast<mech3::Sound>(static_cast<uintptr_t>(static_cast<uint32_t>(i))); }

// Part codes: 1 torso, 2 head, 3 4 legs, 5 6 arms. Kinds 9 and 10: internal and armor set.
struct LTorso : mech3::Torso {
  void set_internal(float v, int s) override { log3(9, fbits(v), 16 + s); }
  void set_armor(float v, int s) override { log3(10, fbits(v), 16 + s); }
};
struct LHead : mech3::Head {
  void set_internal(float v, int s) override { log3(9, fbits(v), 32 + s); }
  void set_armor(float v, int s) override { log3(10, fbits(v), 32 + s); }
};
struct LLeg : mech3::Leg {
  int code = 3;
  void set_internal(float v, int s) override { log3(9, fbits(v), 16 * code + s); }
  void set_armor(float v, int s) override { log3(10, fbits(v), 16 * code + s); }
};
struct LArm : mech3::Arm {
  int code = 5;
  void set_internal(float v, int s) override { log3(9, fbits(v), 16 * code + s); }
  void set_armor(float v, int s) override { log3(10, fbits(v), 16 * code + s); }
};
LTorso g_torso;
LHead g_head;
LLeg g_legs[2];
LArm g_arms[2];
}  // namespace

extern "C" {

void probe_heat_io(int32_t* v, int write) {
  mech3::Mech& m = g_mech;
  mech3::World& w = mech3::world();
  struct F { float* f = nullptr; int32_t* i = nullptr; bool* b = nullptr; };
  int32_t mission = g_script.mission, option = g_script.option, screen = g_script.screen,
          ammo = g_script.ammo, beep = snd(m.heat_beep()), alarm = snd(m.heat_alarm()),
          warn = snd(m.shutdown_warning()), voice = snd(m.shutdown_voice()), state = m.anim_state;
  int32_t player = w.player == &m, attacker = m.attacker != nullptr;
  const F fields[] = {
      {&m.height}, {&m.heat}, {&m.heat_added}, {&m.heat_dissipation}, {&m.masc_time},
      {&m.shutdown_time}, {&m.beep_time}, {&m.voice_time}, {&g_skis.surface[1]}, {&g_skis.surface[4]}, {&g_pos.y},
      {&w.time}, {&w.dt}, {&g_script.controls}, {&g_script.health},
      {nullptr, nullptr, &m.power_down_pending}, {nullptr, nullptr, &m.heat_shutdown},
      {nullptr, nullptr, &m.powered_down}, {nullptr, nullptr, &m.invincible},
      {nullptr, nullptr, &m.heat_sinks.doubles}, {nullptr, nullptr, &m.active}, {nullptr, nullptr, &m.remote},
      {nullptr, nullptr, &m.salvage_mech}, {nullptr, nullptr, &w.full_heat_all_mechs},
      {nullptr, nullptr, &w.no_heat_haze},
      {nullptr, &m.jump_jets_firing}, {nullptr, &m.heat_sinks.count},
      {nullptr, reinterpret_cast<int32_t*>(&m.heat_state)}, {nullptr, &g_feet[0].soil},
      {nullptr, &g_feet[1].soil}, {nullptr, &beep}, {nullptr, &alarm}, {nullptr, &warn},
      {nullptr, &voice}, {nullptr, &player}, {nullptr, &mission}, {nullptr, &option},
      {nullptr, &screen}, {nullptr, &ammo}, {nullptr, &state}, {nullptr, &attacker},
  };
  int k = 0;
  for (const F& f : fields) {
    if (write) {
      if (f.f) *f.f = bitsf(v[k]);
      else if (f.i) *f.i = v[k];
      else *f.b = v[k] != 0;
    } else {
      v[k] = f.f ? fbits(*f.f) : f.i ? *f.i : (*f.b ? 1 : 0);
    }
    ++k;
  }
  if (write) {
    m.heat_beep() = snd(beep), m.heat_alarm() = snd(alarm);
    m.shutdown_warning() = snd(warn), m.shutdown_voice() = snd(voice);
    w.player = player ? &m : nullptr;
    m.anim_state = state;
    m.attacker = attacker ? &m : nullptr;
    g_script.mission = mission, g_script.option = option;
    g_script.screen = screen, g_script.ammo = ammo;
  }
}

void probe_heat_reset(uint32_t seed) {
  g_mech = mech3::Mech{};
  g_skis = mech3::Skis{};
  g_mech.skis = &g_skis;
  for (int i = 0; i < 2; ++i) {
    g_feet[i] = {};
    g_legs[i] = LLeg{};
    g_legs[i].code = 3 + i;
    g_legs[i].foot = &g_feet[i];
    g_mech.legs[i] = &g_legs[i];
    g_arms[i] = LArm{};
    g_arms[i].code = 5 + i;
    g_mech.arms[i] = &g_arms[i];
  }
  g_mech.torso = &g_torso;
  g_mech.head = &g_head;
  g_script = {};
  crt::msvc_srand(seed);
  mech3::skis_calls().position = [](mech3::Mech&) -> const mech3::Vec3* { return &g_pos; };
  mech3::HeatCalls& c = mech3::heat_calls();
  c.heat_option = [] { return g_script.option != 0; };
  c.game_type = [] { return g_script.mission; };
  c.throttle = [](mech3::Mech&) { return g_script.controls; };
  c.health = [](mech3::Mech&) { return g_script.health; };
  c.screen = [](mech3::Mech&) -> void* { return g_script.screen ? &g_script : nullptr; };
  c.heat_haze = [](void*, float a, int k, float dt) { log3(1, fbits(a), k), log3(1, fbits(dt), 0); };
  c.play_sound = [](mech3::Mech&, int id) {
    log3(2, id, static_cast<int32_t>(g_script.next_sound));
    return snd(static_cast<int32_t>(g_script.next_sound++));
  };
  c.stop_sound = [](mech3::Sound s) { log3(3, snd(s), 0); };
  c.play_mech_sound = [](int i) { log3(4, i, 0); };
  c.power = [](mech3::Mech&, int on) { log3(5, on, 0); };
  c.torso_hit = [](mech3::Mech&, int explode) { log3(6, explode, 1); };
  c.ammo_explosion = [](mech3::Mech&) { log3(7, g_script.ammo, 0); return g_script.ammo != 0; };
  c.hud_message = [](float s, int msg) { log3(8, fbits(s), msg); };
}

int probe_heat_step(int32_t* log, int cap) {
  g_log.clear();
  mech3::update_heat(g_mech, mech3::world().dt);
  const int n = static_cast<int>(g_log.size());
  for (int i = 0; i < n && i < cap; ++i) log[i] = g_log[i];
  return n;
}

}  // extern "C"
