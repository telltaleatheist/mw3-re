// The port's side of test_pm_ai_brain.py: PM's frame, every call logged, its answers from a script of bits.
#include <cstdint>
#include <cstring>
#include <vector>

#include "mech3/zai_brain_pm.h"

using namespace mech3::ai;
using zgame::Handle;

namespace {
constexpr Handle kSelf = 1, kPick = 0x51, kPlayer = 0x52, kTarget = 0x53, kPv = 0x61, kV = 0x62, kNet = 0x71, kBeh = 0x72;
constexpr Handle kCurNet = 0x73, kCurState = 0x74, kWorst = 0x75;

uint32_t ubits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}

struct Tick : TickPmHooks {
  const int32_t* in;
  const uint8_t* bits;
  const float* f;
  int n = 0, count = 0;
  std::vector<uint32_t> log;
  void put(uint32_t k, uint32_t v = 0) { log.push_back(k), log.push_back(v); }
  bool bit() { return bits[n++] != 0; }
  bool ai_on() override { return in[0] != 0; }
  float now() override { return f[1]; }
  void stop() override { put(3); }
  void smooth(float dt) override { put(4, ubits(dt)); }
  int32_t power_state(Handle) override { put(1); return bit() ? 1 : 0; }
  void steer_update(float dt, Handle) override { put(2, ubits(dt)); }
  Handle pick(Handle) override { put(5); return bit() ? kPick : 0; }
  Handle player_contact(Handle) override { return in[1] ? kPlayer : 0; }
  Handle target_contact(Handle) override { return in[2] ? kTarget : 0; }
  float dist2(Handle c) override { return c == kPick ? f[10] : c == kPlayer ? f[11] : f[12]; }
  Handle contact_vehicle(Handle) override { put(6); return bit() ? kPv : 0; }
  float range2(Handle) override { return f[13]; }
  void count_awake() override { ++count; }
  bool has_manager(Handle) override { return in[3] != 0; }
  double ammo(Handle) override {
    double a;
    std::memcpy(&a, in + 14, 8);
    return a;
  }
  void radio_line(int32_t slot) override { put(7, 0x1000 + slot); }
  void vlist(Handle, bool* picked, Handle* enemy) override {
    put(8);
    if (bit()) *picked = true, *enemy = bit() ? kPick : 0;
  }
  bool hit_lately() override { put(9); return bit(); }
  bool may_fire() override { put(10); return bit(); }
  void firing_reset() override { put(11); }
  Handle worst_attacker() override { put(12); return kWorst; }
  void take_attacker(Handle a) override { put(13, static_cast<uint32_t>(a)); }
  void manoeuvre() override { put(14); }
  void current(Handle* net, Handle* state) override { put(15), *net = kCurNet, *state = kCurState; }
  bool may_engage(Handle, Handle net, Handle state) override {
    put(17, static_cast<uint32_t>(net)), put(17, static_cast<uint32_t>(state));
    return bit();
  }
  Handle target_vehicle(Handle c) override { put(18, static_cast<uint32_t>(c)); return bit() ? kV : 0; }
  Handle vehicle_net(Handle) override { put(20); return bit() ? kNet : 0; }
  Handle attack_behaviour(Handle) override { put(21); return bit() ? kBeh : 0; }
  void attack(Handle net, Handle beh) override { put(22, static_cast<uint32_t>(net)), put(22, static_cast<uint32_t>(beh)); }
  int32_t rand_int() override { put(23); return in[4]; }
  void run_orders(float dt) override { put(24, ubits(dt)); }
  void firing(float dt) override { put(25, ubits(dt)); }
};
}  // namespace

namespace {
struct Side : BrainSidePmHooks {
  const int32_t* in;
  const uint8_t* bits;
  const float* f;
  int n = 0;
  std::vector<uint32_t> log;
  void put(uint32_t k, uint32_t v = 0) { log.push_back(k), log.push_back(v); }
  bool bit() { return bits[n++] != 0; }
  float now() override { return f[0]; }
  float clock() override { return f[1]; }
  bool net_allows(Handle) override { put(30); return bit(); }
  bool state_allows(Handle) override { put(31); return bit(); }
  void position(Handle, float o[3]) override { o[0] = f[2], o[1] = f[3], o[2] = f[4]; }
  double health(Handle) override { put(32); return f[5]; }
  void push_behaviour(int32_t w) override { put(33, w); }
  uint32_t allies(Handle) override { return in[0]; }
  uint32_t team(Handle) override { put(34); return in[1]; }
  Handle attacker_vehicle(Handle) override { put(35); return in[2] ? 0x90 : 0x91; }
  Handle player_vehicle() override { return 0x90; }
  Handle player_contact(Handle) override { return in[3] ? 0x92 : 0; }
  float dist2(Handle) override { return f[6]; }
  void radio_line(int32_t slot) override { put(7, 0x1000 + slot); }
  const float* hit_record(Handle v) override {
    put(36, static_cast<uint32_t>(v));
    return bit() ? f + 7 : nullptr;
  }
  void allies_turn() override { put(37); }
  void hit_base(int32_t a, int32_t b, int32_t c, Handle t) override {
    put(38, a), put(38, b), put(38, c), put(38, static_cast<uint32_t>(t));
  }
  void died_base() override { put(39); }
  double speed_at(Handle, float t) override { put(40, ubits(t)); return f[10]; }
  bool in_world(Handle) override { return in[4] != 0; }
  void set_way(const float* a, const float* b, float s, Handle v) override {
    for (int i = 0; i < 3; ++i) put(41, ubits(a[i]));
    for (int i = 0; i < 3; ++i) put(41, ubits(b[i]));
    put(41, ubits(s)), put(41, static_cast<uint32_t>(v));
  }
};
}  // namespace

namespace {
struct Ord : OrdersPmHooks {
  const uint8_t* bits;
  int n = 0;
  uint32_t next_part = 0x100;
  std::vector<uint32_t> log;
  void put(uint32_t k, uint32_t v = 0) { log.push_back(k), log.push_back(v); }
  bool bit() { return bits[n++] != 0; }
  bool may_fire(Handle s) override { put(50, s); return bit(); }
  bool net_idle(Handle x) override { put(51, x); return bit(); }
  bool state_idle(Handle x) override { put(52, x); return bit(); }
  Handle as_net(Handle x) override { put(53, x); return bit() ? x : 0; }
  Handle make_net(Handle x) override { put(54, x); return next_part++; }
  Handle make_state(Handle x) override { put(55, x); return next_part++; }
  Handle default_net() override { return 0xd1; }
  Handle default_state() override { return 0xd2; }
  Handle stand_state() override { return 0xd3; }
  void free_part(Handle x) override { put(56, x); }
  void resume(Handle x) override { put(62, x); }
  void stop_state(Handle x) override { put(65, x); }
  Handle net_of(Handle x) override { put(66, x); return x; }
  void run_order(OrderPm& o, float dt) override {
    put(57, o.net), put(57, ubits(dt));
    if (bit()) this->o->again = 1;
  }
  void run_pending(Handle x) override { put(58, x), put(59, x); }
  OrdersPm* o = nullptr;
};
}  // namespace

namespace {
struct Scan : ScanPmHooks {
  std::vector<uint32_t> log;
  void scan_test(int32_t i) override { log.push_back(63), log.push_back(i); }
  void set_behaviour(Handle n) override { log.push_back(64), log.push_back(static_cast<uint32_t>(n)); }
};
}  // namespace

extern "C" int32_t probe_pm_ai_scan(int32_t tests, uint32_t* out) {
  Scan t;
  OrdersPm o;
  scan_run_pm(t, o, tests, 0xb7);
  t.log.push_back(o.again);
  std::memcpy(out, t.log.data(), t.log.size() * 4);
  return static_cast<int32_t>(t.log.size());
}

namespace {
struct Slots : NetSlotsPmHooks {
  const uint8_t* live;
  Handle slot(int32_t i) override { return live[i] ? 0x200 + i : 0; }
  Handle step_of(Handle s, int32_t off) override { return s * 0x1000 + off; }
};
}  // namespace

// which: -1 the follow, -2 the attack, -3 the guard, else a kind.
extern "C" uint32_t probe_pm_ai_kind(int32_t which, const uint8_t* live) {
  Slots t;
  t.live = live;
  return which == -1 ? follow_pm(t) : which == -2 ? attack_pm(t) : which == -3 ? net_slot5_pm(t) : kind_behaviour_pm(t, which - 16);
}
extern "C" uint32_t probe_pm_ai_slot_vt(int32_t i) { return kVehicleSlotsPm[i]; }

namespace {
struct Lead : LeadPmHooks {
  const float* f;
  const int32_t* in;
  const float* velocity(Handle e) override { return e == 1 ? f + 3 : f + 6; }
  const float* smoothed_velocity(Handle) override { return in[0] ? f + 9 : nullptr; }
  void aim_point(Handle, float o[3]) override { o[0] = f[12], o[1] = f[13], o[2] = f[14]; }
  uint32_t weapon_flags(Handle) override { return in[1]; }
  float weapon_speed(Handle) override { return f[15]; }
  const float* muzzle(Handle) override { return in[2] ? f : nullptr; }
};
struct Tg : TargetsPmHooks {
  const int32_t* in;
  uint32_t team_of(Handle) override { return in[5]; }
  uint32_t flags_of(Handle) override { return in[6]; }
};
}  // namespace

// in: pilot, weapon flags, muzzle, a weapon. f: the muzzle, the own, target's and pilot's velocities, the aim point, speed.
extern "C" int32_t probe_pm_ai_lead(const int32_t* in, const float* f, float* out) {
  Lead t;
  t.f = f, t.in = in;
  return lead_point_pm(t, 1, 2, in[3] ? 3 : 0, out);
}

// in: flags, allies, enemies, keep, a contact, its team, its flags; old: target, ally, enemy, marked.
extern "C" void probe_pm_ai_settarget(const int32_t* in, const uint32_t* old, uint32_t* out) {
  Tg h;
  h.in = in;
  TargetsPm t;
  t.target = old[0], t.ally = old[1], t.enemy = old[2], t.marked = old[3];
  t.flags = in[0], t.allies = in[1], t.enemies = in[2];
  set_target_pm(h, t, in[4] ? 0x77 : 0, in[3] != 0);
  out[0] = t.target, out[1] = t.ally, out[2] = t.enemy, out[3] = t.marked;
}

// ops: triples of op, a, b.
extern "C" int32_t probe_pm_ai_orders(const int32_t* ops, int32_t count, const uint8_t* bits, uint32_t* out, int32_t cap) {
  Ord t;
  t.bits = bits;
  OrdersPm o;
  uint32_t dtu = 0x3d000000;
  for (int i = 0; i < count; ++i) {
    const int32_t op = ops[i * 3], a = ops[i * 3 + 1], b = ops[i * 3 + 2];
    switch (op) {
      case 0:
        o.orders.emplace_back();
        order_parts_pm(t, o.orders.back(), a, b);
        break;
      case 1: o.orders.back().ready = static_cast<uint8_t>(a); break;
      case 2: t.put(60, may_fire_pm(t, o)); break;
      case 3: t.put(60, orders_idle_pm(t, o)); break;
      case 4: {
        float dt;
        std::memcpy(&dt, &dtu, 4);
        t.o = &o;
        run_orders_pm(t, o, dt);
        break;
      }
      case 5: t.put(60, replace_orders_pm(t, o, a)); break;
      case 7: o.pending.push_front(a); break;
      case 9: give_order_pm(t, o, a, b); break;
      case 10: push_behaviour_pm(t, o, b); break;
      case 8:
        stop_orders_pm(t, o);
        order_parts_pm(t, o.orders.back(), 0, 0);
        break;
      default: o.pending.push_back(a);
    }
    t.put(61, static_cast<uint32_t>(o.orders.size()));
  }
  if (static_cast<int32_t>(t.log.size()) > cap) return -1;
  std::memcpy(out, t.log.data(), t.log.size() * 4);
  return static_cast<int32_t>(t.log.size());
}

// What: 0 engage, 1 manoeuvre, 2 hit, 3 died, 4 way. in: allies, team, attacker is the player, +0x98, in world,
// then the brain's mode, flags, home, kind, radio, owner is attacker, no attacker, vehicle, net, state.
extern "C" int32_t probe_pm_ai_side(int32_t what, const int32_t* in, const uint8_t* bits, const float* f, uint32_t* out, int32_t cap) {
  Side t;
  t.in = in, t.bits = bits, t.f = f;
  BrainTickPm b;
  b.mode = in[5], b.engage_flags = static_cast<uint8_t>(in[6]), b.manoeuvre_kind = in[8], b.radio = in[9] != 0;
  const float home[3] = {f[11], f[12], f[13]};
  if (in[7]) b.home = home;
  b.leash = f[14], b.health_floor = f[15], b.attack_until = f[16], b.give_up = f[17];
  float from[3] = {0, 0, 0};
  uint32_t r = 0;
  const Handle owner = 0x80;
  switch (what) {
    case 0: r = may_engage_pm(t, b, owner, in[13] ? 0x93 : 0, in[14] ? 0x94 : 0); break;
    case 1: manoeuvre_pm(t, b); break;
    case 2: on_hit_pm(t, b, owner, 11, 22, 33, in[10] ? owner : in[11] ? 0 : 0x95); break;
    case 3: died_pm(t, b); break;
    default: {
      const float spot[3] = {f[18], f[19], f[20]};
      way_reset_pm(t, in[12] ? owner : 0, f[21], spot, from);
    }
  }
  std::vector<uint32_t> o = {r, ubits(b.wait_until), ubits(from[0]), ubits(from[1]), ubits(from[2]), static_cast<uint32_t>(t.n)};
  o.insert(o.end(), t.log.begin(), t.log.end());
  if (static_cast<int32_t>(o.size()) > cap) return -1;
  std::memcpy(out, o.data(), o.size() * 4);
  return static_cast<int32_t>(o.size());
}

// in: ai on, +0x98, +0x6c, manager, the roll, then the brain's ints.
extern "C" int32_t probe_pm_ai_tick(const int32_t* in, const uint8_t* bits, const float* f, uint32_t* out, int32_t cap) {
  Tick t;
  t.in = in, t.bits = bits, t.f = f;
  BrainTickPm b;
  b.mode = in[5], b.delay_on = in[6], b.awake = in[7], b.by_player = in[8], b.reacts = in[9], b.wanders = in[10];
  b.busy = in[11], b.runs_down = static_cast<uint8_t>(in[12]), b.radio = in[13] != 0;
  b.sight = f[2], b.engage = f[3], b.wait_low = f[4], b.wait_high = f[5], b.react_gap = f[6], b.delay_until = f[7];
  b.wait_until = f[8], b.react_at = f[9];
  const bool r = tick_pm(t, b, kSelf, f[0]);
  std::vector<uint32_t> o = {r ? 1u : 0u, static_cast<uint32_t>(b.mode), ubits(b.delay_until), ubits(b.wait_until), ubits(b.react_at),
                             static_cast<uint32_t>(t.count), static_cast<uint32_t>(t.n)};
  o.insert(o.end(), t.log.begin(), t.log.end());
  if (static_cast<int32_t>(o.size()) > cap) return -1;
  std::memcpy(out, o.data(), o.size() * 4);
  return static_cast<int32_t>(o.size());
}
