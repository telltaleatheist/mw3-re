// Probe for tests/differential/test_pm_ally_hit.py: Pirate's Moon's ally hit 0x004bb6a0 with its stand-down
// 0x004ba8c0 (the port's pm_hit_an_ally on a firing record), on the branch the exe takes (the +0x94 byte set).
#include <cstdint>
#include <cstring>

#include "mech3/zai_wire_firing_pm.h"

extern "C" {
// in: the victim, rest (+0x10), wait (+0x30), busy (+0x34), until (+0x4c), time (+0x04), phase (+0x00) as words.
// out: the obstacle, wait, busy, until, time, phase, rest.
int probe_pm_ally_hit(const uint32_t* in, uint32_t* out) {
  mech3::ai::FiringRecord r;
  auto f = [](uint32_t b) { float x; std::memcpy(&x, &b, 4); return x; };
  auto w = [](float x) { uint32_t b; std::memcpy(&b, &x, 4); return b; };
  r.fire.rest = f(in[1]), r.fire.wait = f(in[2]), r.fire.busy = in[3] != 0, r.pick.until = f(in[4]);
  r.fire.time = f(in[5]), r.fire.phase = static_cast<int32_t>(in[6]);
  mech3::ai::pm_hit_an_ally(r, static_cast<zgame::Handle>(in[0]));
  int n = 0;
  out[n++] = static_cast<uint32_t>(r.pick.aimed), out[n++] = w(r.fire.wait), out[n++] = r.fire.busy;
  out[n++] = w(r.pick.until), out[n++] = w(r.fire.time), out[n++] = static_cast<uint32_t>(r.fire.phase);
  out[n++] = w(r.fire.rest);
  return n;
}
}  // extern "C"
