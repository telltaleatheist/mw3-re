// test_pm_ai_defaults.py: PM's AI structs as their constructors and start-up leave them, word by word at the exe's
// offsets (an unnamed or unset word 0).
#include <cstdint>
#include <cstring>

#include "mech3/zai_attackers_pm.h"
#include "mech3/zai_firing_pm.h"
#include "mech3/zai_steer_pm.h"
#include "mech3/zai_walk_pm.h"

using namespace mech3::ai;

namespace {
uint32_t w(float f) {
  uint32_t b;
  std::memcpy(&b, &f, 4);
  return b;
}
}  // namespace

extern "C" {
// 0x0060b688's 0x6c bytes (0x004ba660), as FiringPm::seed copies them.
void probe_pm_firing_defaults(uint32_t* o) {
  FiringPm f;
  f.seed();
  std::memset(o, 0, 0x6c);
  o[0] = static_cast<uint32_t>(f.phase), o[1] = w(f.time), o[2] = w(f.quick), o[3] = w(f.delay), o[4] = w(f.rest);
  o[5] = f.plan, o[6] = w(f.reach_arg), o[7] = w(f.track_arg), o[8] = w(f.fire_arg), o[9] = w(f.x24);
  o[11] = w(f.range), o[12] = w(f.wait), o[13] = f.busy, o[14] = w(f.too_hot), o[15] = w(f.x3c), o[16] = w(f.x40);
  o[17] = w(f.x44), o[18] = w(f.x48), o[20] = f.x50, o[21] = w(f.x54), o[22] = w(f.cool_for), o[23] = w(f.cool_until);
  o[24] = w(f.cool_below), o[25] = w(f.shot_every), o[26] = w(f.shot_next);
}

// 0x004bad70: the aim's 0x24 bytes.
void probe_pm_aim_defaults(uint32_t* o) {
  AimPm a;
  std::memset(o, 0, 0x24);
  for (int i = 0; i < 3; ++i) o[i] = w(a.offset[i]), o[3 + i] = w(a.drift[i]);
  o[6] = a.active, o[7] = w(a.until), o[8] = w(a.limit2);
}

// 0x004b2950 (0x004b2580): the rate, limit and stamp.
void probe_pm_attackers_defaults(float clock, uint32_t* o) {
  AttackersPm a = AttackersPm::for_pilot(clock);
  o[0] = w(a.rate()), o[1] = w(a.limit()), o[2] = w(a.stamp()), o[3] = static_cast<uint32_t>(a.list.size());
}

// 0x004d9b10: +0x08..+0x34.
void probe_pm_fight_defaults(uint32_t* o) {
  FightStatePm s;
  std::memset(o, 0, 0x38);
  o[2] = s.flags, o[3] = w(s.stop_dist), o[4] = w(s.range_far), o[5] = w(s.range_near), o[6] = w(s.range_mid);
  o[7] = w(s.x1c), o[8] = w(s.speed), o[9] = w(s.wake), o[10] = w(s.until);
  o[11] = s.pulled | static_cast<uint32_t>(s.x2d) << 8, o[12] = s.owned ? 1u : 0u, o[13] = s.x34;
}

// 0x004dc0f0: +0x04..+0x9c.
void probe_pm_steer_defaults(uint32_t* o) {
  SteerPm s;
  std::memset(o, 0, 0xa0);
  for (int i = 0; i < 3; ++i) o[1 + i] = w(s.start[i]);
  o[4] = w(s.dir[0]), o[5] = w(s.dir[1]);
  const float f[] = {s.along_min, s.length, s.along_at, s.across, s.across_to, s.rate, s.ease, s.along, s.side,
                     s.near_dist, s.x40, s.keep_near};
  for (int i = 0; i < 12; ++i) o[6 + i] = w(f[i]);
  o[18] = s.keep;
  const float g[] = {s.x4c, s.keep_slope, s.keep_scale};
  for (int i = 0; i < 3; ++i) o[19 + i] = w(g[i]);
  o[22] = s.push;
  const float h[] = {s.push_reach, s.push_band, s.far_along, s.far_side, s.cos_on, s.cos_turnabout, s.turnabout_room,
                     s.gain_along, s.gain_side, s.least, s.brake, s.cos_dead, s.sin_dead, s.gain, s.least_turn,
                     s.cos_behind};
  for (int i = 0; i < 16; ++i) o[23 + i] = w(h[i]);
  o[39] = static_cast<uint32_t>(s.sets);
}
}
