// PM's firing's aim (zai_firing_pm), for test_pm_ai_aim.py: the world's answers come in, the calls
// out and the aim's words go back.
#include <cstdint>
#include <cstring>
#include <vector>

#include "mech3/zai_firing_pm.h"
#include "mech3/zai_target.h"

namespace {
using namespace mech3::ai;
struct W : AimPmHooks {
  const float* f;
  const int32_t* in;
  const int32_t* rands;
  int nrand = 0;
  Contact c;
  float pos[3];
  std::vector<uint32_t> log;
  Contact* plan_contact(zgame::Handle) override { return &c; }
  const float* plan_position(zgame::Handle) override { return pos; }
  zgame::Handle contact_entity(const Contact*) override { return in[0] ? 1 : 0; }
  bool entity_is_vehicle(zgame::Handle) override { return in[1] != 0; }
  uint32_t entity_pilot_kind(zgame::Handle) override { return static_cast<uint32_t>(in[2]); }
  float difficulty_error() override { return f[0]; }
  float pilot_error(Brain*) override { return f[1]; }
  float pilot_aid(Brain*) override { return f[2]; }
  bool targeting_aid(Brain*) override { return in[3] != 0; }
  void vehicle_position(Brain*, float out[3]) override { std::memcpy(out, f + 3, 12); }
  void weapon_line(zgame::Handle, const float at[3], float muzzle[3], float dir[3]) override {
    for (int i = 0; i < 3; ++i) log.push_back(bits(at[i]));
    std::memcpy(muzzle, f + 6, 12), std::memcpy(dir, f + 9, 12);
  }
  int32_t rand() override { return rands[nrand++]; }
  float now() override { return f[12]; }
  void wm_aim_at(zgame::Handle, const float* at) override {
    log.push_back(0x41);
    for (int i = 0; i < 3; ++i) log.push_back(bits(at[i]));
  }
  void wm_has_target(zgame::Handle, bool on) override { log.push_back(0x42), log.push_back(on); }
  static uint32_t bits(float x) {
    uint32_t u;
    std::memcpy(&u, &x, 4);
    return u;
  }
};
}  // namespace

// in: entity, vehicle, its pilot kind, the aid, a weapon, the op (0 track, 1 lead); rands; f: the
// difficulty, +0xb8, +0xc0, the vehicle's place, the weapon's muzzle and line, now, dt, the
// plan's place, the contact's delta and its dist, the aim's words (active by in[6]).
extern "C" int32_t probe_pm_ai_aim(const int32_t* in, const int32_t* rands, const float* f, uint32_t* out, int32_t cap) {
  W w;
  w.in = in, w.f = f, w.rands = rands;
  std::memcpy(w.pos, f + 14, 12);
  std::memcpy(w.c.delta, f + 17, 12);
  w.c.dist = f[20];
  AimPmHooks* saved = &aim_pm_hooks();
  set_aim_pm_hooks(&w);
  AimPm a;
  std::memcpy(a.offset, f + 21, 12), std::memcpy(a.drift, f + 24, 12);
  a.active = in[6] != 0, a.until = f[27], a.limit2 = f[28];
  const zgame::Handle weapon = in[4] ? 7 : 0;
  if (in[5] == 0) a.track(1, weapon, nullptr);
  else a.lead(f[13], 1, weapon, 2, nullptr);
  set_aim_pm_hooks(saved);
  std::vector<uint32_t>& l = w.log;
  l.push_back(0x2b);
  for (int i = 0; i < 3; ++i) l.push_back(W::bits(a.offset[i]));
  for (int i = 0; i < 3; ++i) l.push_back(W::bits(a.drift[i]));
  l.push_back(a.active), l.push_back(W::bits(a.until)), l.push_back(W::bits(a.limit2));
  l.push_back(static_cast<uint32_t>(w.nrand));
  const int32_t n = static_cast<int32_t>(l.size());
  if (n > cap) return -1;
  std::memcpy(out, l.data(), 4 * l.size());
  return n;
}
