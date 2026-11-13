// Probe for tests/differential/test_pm_lance_orders.py: PM's lance orders (zai_brain_pm's lance_order_pm) with
// every call out answered from a script of bits and logged, as the test's stand-ins answer the exe's.
#include <cstdint>
#include <vector>

#include "mech3/zai_brain_pm.h"

namespace {
using mech3::ai::LancePmHooks;
using Handle = uint32_t;

// in: own vehicle, contact vehicle, net, behaviour, first order, formation, team, allies, enemies, radio on
struct Lance : LancePmHooks {
  const uint32_t* in;
  const uint8_t* bits;
  int n = 0;
  std::vector<uint32_t> log;
  uint32_t kept;
  void put(uint32_t k, uint32_t v = 0) { log.push_back(k), log.push_back(v); }
  bool bit() { return bits[n++] != 0; }
  Handle own_vehicle() override { return in[0]; }
  Handle contact_vehicle(Handle) override { put(1); return bit() ? in[1] : 0; }
  bool leavable() override { put(2); return bit(); }
  Handle vehicle_net(Handle v) override { put(3, v); return bit() ? in[2] : 0; }
  Handle follow_behaviour(Handle net) override { put(4, net); return in[3]; }
  void give(Handle net, Handle beh) override { put(5, net), put(5, beh); }
  Handle first_order() override { put(6); return bit() ? in[4] : 0; }
  void pop_to(Handle first) override { put(7, first); }
  int32_t give_net(Handle net) override { put(8, net); return 1; }
  Handle formation() override { return in[5]; }
  uint32_t team(Handle v) override { put(10, v); return in[6]; }
  uint32_t allies(Handle) override { return in[7]; }
  uint32_t enemies(Handle) override { return in[8]; }
  void keep(Handle t) override { kept = t; }
  int32_t refit() override { put(11); return bit() ? 1 : 0; }
  bool has_radio() override { return in[9] != 0; }
  void radio_line(int32_t line) override { put(12, static_cast<uint32_t>(line)); }
};
}  // namespace

// out: the answer, the kept target (starting at `kept`), the bits used, then the log. Returns the words written.
extern "C" int32_t probe_pm_lance_order(int32_t order, const uint32_t* in, const uint8_t* bits, uint32_t kept,
                                        uint32_t* out) {
  Lance h;
  h.in = in, h.bits = bits, h.kept = kept;
  const int32_t r = mech3::ai::lance_order_pm(h, order, 0x5150);
  int32_t w = 0;
  out[w++] = static_cast<uint32_t>(r), out[w++] = h.kept, out[w++] = static_cast<uint32_t>(h.n);
  for (uint32_t x : h.log) out[w++] = x;
  return w;
}
