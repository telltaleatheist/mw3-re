// test_pm_ai_attack.py: PM's attack state 0x004dedf0, its calls out logged.
#include <cstdint>
#include <cstring>

#include "mech3/zai_attack_pm.h"

using namespace mech3::ai;

namespace {
uint32_t* g_log;
int g_n;
void note(uint32_t a, uint32_t b = 0, uint32_t c = 0) { g_log[g_n++] = a, g_log[g_n++] = b, g_log[g_n++] = c; }
uint32_t f2u(const float* p) { uint32_t u; std::memcpy(&u, p, 4); return u; }

struct Hooks : AttackPmHooks {
  const int32_t* i;
  const float* f;
  float pos[3], place[3];
  bool context(AttackStatePm&) override { return i[0] != 0; }
  void fight_step() override { note(1); }
  void fight_enter() override { note(2); }
  double d_speed;
  double speed() override { return d_speed; }
  float now() override { return f[1]; }
  float intensity() override { return f[2]; }
  const float* position() override { return pos; }
  uint32_t orders() override { return static_cast<uint32_t>(i[1]); }
  zgame::Handle prior_net() override { return static_cast<zgame::Handle>(i[2]); }
  zgame::Handle nearest(zgame::Handle net, const float* at) override { note(3, net, f2u(at)); return static_cast<zgame::Handle>(i[3]); }
  const float* place_of(zgame::Handle) override { return place; }
  zgame::Handle order_target() override { return static_cast<zgame::Handle>(i[4]); }
  zgame::Handle kind_behaviour(int32_t k) override { note(4, static_cast<uint32_t>(k)); return static_cast<zgame::Handle>(i[5]); }
  void queue_after(zgame::Handle t, zgame::Handle b) override { note(5, t, b); }
  void return_to(zgame::Handle b) override { note(6, b); }
};
}  // namespace

struct Pending : PendingPmHooks {
  void pop_order() override { note(1); }
  void set_behaviour(zgame::Handle b) override { note(3, b); }
  void follow(zgame::Handle t, zgame::Handle b) override { note(2, t, b); }
};

extern "C" {
int probe_pm_ai_pending(int kind, uint32_t target, uint32_t behaviour, uint32_t* log) {
  g_log = log, g_n = 0;
  Pending p;
  PendingPm o;
  o.kind = static_cast<PendingPm::Kind>(kind), o.target = target, o.behaviour = behaviour;
  o.run(p);
  return g_n / 3;
}
// in: no_target, orders, net, nearest, target, behaviour, flags, kind, x5c, x60, mode, hold_net, hold_set
// fin: speed, now, intensity, delay, x58, xb4, xfc, x108, x1f0, pos[3], place[3], r10, r14, r18, hold[3]
// out: flags, kind, x5c, x60, mode, hold_net, hold_set, place (0 none, 1 hold); fout: xfc, x108, x1f0, r10, r14, r18, hold[3]
int probe_pm_ai_attack(const int32_t* in, const float* fin, double speed, int32_t* out, float* fout, uint32_t* log) {
  g_log = log, g_n = 0;
  Hooks h;
  h.i = in, h.f = fin, h.d_speed = speed;
  std::memcpy(h.pos, fin + 9, 12), std::memcpy(h.place, fin + 12, 12);
  AttackStatePm s;
  AttackBrainPm b;
  s.flags = static_cast<uint32_t>(in[6]);
  b.kind = in[7], b.x5c = in[8], b.x60 = in[9], b.mode = in[10];
  s.hold_net = static_cast<zgame::Handle>(in[11]);
  s.hold_at = in[12] ? h.place : nullptr;
  b.delay = fin[3], b.x58 = fin[4], b.xb4 = fin[5], b.xfc = fin[6], b.x108 = fin[7], b.x1f0 = fin[8];
  s.range_far = fin[15], s.range_near = fin[16], s.range_mid = fin[17];
  std::memcpy(s.hold, fin + 18, 12);
  b.place = h.pos;
  s.step(h, b);
  out[0] = static_cast<int32_t>(s.flags), out[1] = b.kind, out[2] = b.x5c, out[3] = b.x60, out[4] = b.mode;
  out[5] = static_cast<int32_t>(s.hold_net), out[6] = s.hold_at != nullptr;
  out[7] = b.place == nullptr ? 0 : b.place == s.hold ? 1 : 2;
  fout[0] = b.xfc, fout[1] = b.x108, fout[2] = b.x1f0, fout[3] = s.range_far, fout[4] = s.range_near, fout[5] = s.range_mid;
  std::memcpy(fout + 6, s.hold, 12);
  return g_n / 3;
}
}
