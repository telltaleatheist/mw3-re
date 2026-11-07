#include "mech3/zai_brain_pm.h"

#include <cmath>
#include <cstring>

namespace mech3::ai {

namespace {
constexpr float kFltMax = 3.4028235e38f;
constexpr double kRand = 3.0518509447574615e-05;  // 0x005e28b0
}  // namespace

// Pirate's Moon 1.0 @ 0x004fab90 (MW3's 0x004e9710): MW3's order-run on the brain and its attack bookkeeping went
// to 0x004fb260 and 0x004fa150, and its params' +0xcc flag is gone. The reaction to a hit also takes the worst
// attacker. Net's vt+0x18 after 0x004fbf30 is a getter whose answer is dropped.
bool tick_pm(TickPmHooks& h, BrainTickPm& b, zgame::Handle vehicle, float dt) {
  if (!h.ai_on()) {
    h.stop();
    h.smooth(dt);
    return true;
  }
  const bool down = h.power_state(vehicle) == 1;
  h.steer_update(dt, vehicle);
  if (!b.runs_down && down) {
    h.stop();
    h.smooth(dt);
    return true;
  }
  bool picked = false;
  zgame::Handle enemy = 0;
  if (b.delay_on && h.now() > b.delay_until) {
    b.mode = 1;
    b.delay_until = kFltMax;
  }
  if (b.awake == 0) {
    zgame::Handle seen;
    if (b.by_player) {
      seen = h.player_contact(vehicle);
    } else {
      seen = enemy = h.pick(vehicle);
      picked = true;
    }
    if (seen && h.dist2(seen) >= b.sight) {
      // Out of sight: still steered while its attacker has it in range.
      const zgame::Handle a = h.player_contact(vehicle);
      const zgame::Handle v = a ? h.contact_vehicle(a) : 0;
      if (!v || h.dist2(a) >= h.range2(v)) return false;
      h.stop();
      h.smooth(dt);
      return true;
    }
  }
  h.count_awake();
  if (b.radio && h.has_manager(vehicle)) {
    const double ammo = h.ammo(vehicle);
    if (!(ammo >= 0.10000000149011612)) h.radio_line(0x28);
    else if (!(ammo >= 0.25)) h.radio_line(0x24);
  }
  h.vlist(vehicle, &picked, &enemy);
  if (b.busy == 0 && b.reacts && h.now() > b.react_at && h.hit_lately() && h.may_fire()) {
    b.react_at = static_cast<float>(double{h.now()} + b.react_gap);
    h.firing_reset();
    h.take_attacker(h.worst_attacker());
    h.manoeuvre();
  }
  zgame::Handle net, state;
  h.current(&net, &state);
  bool settled = false;
  if (h.may_engage(vehicle, net, state)) {
    zgame::Handle t = h.target_contact(vehicle);
    if (!t) {
      if (!picked) {
        enemy = h.pick(vehicle);
        picked = true;
      }
      t = enemy;
    }
    const zgame::Handle v = t ? h.target_vehicle(t) : 0;
    const float d = v ? h.dist2(t) : kFltMax;
    // FIX (bug): as MW3's frame, no vehicle and a range past FLT_MAX calls through null in the exe; here no attack.
    if (!(d >= b.engage) && v) {
      settled = true;
      if (const zgame::Handle n = h.vehicle_net(v))
        if (const zgame::Handle beh = h.attack_behaviour(n)) h.attack(n, beh);
    }
  }
  if (!settled && b.busy == 0 && b.wanders && h.may_fire()) {
    if (b.wait_until == kFltMax) {
      const float low = b.wait_low, high = b.wait_high, now = h.now();
      const double r = h.rand_int();
      b.wait_until = static_cast<float>((double{high} - low) * kRand * r + low + now);
    } else if (h.now() > b.wait_until) {
      h.manoeuvre();
      settled = true;
    }
  }
  if (settled) b.wait_until = kFltMax;
  h.run_orders(dt);
  h.smooth(dt);
  if (!down) h.firing(dt);
  return true;
}

// Pirate's Moon 1.0 @ 0x004fa5a0 (MW3's 0x004e8f10): engaging only at mode 1, and the health floor asked each time
// where MW3 decides it once a frame.
bool may_engage_pm(BrainSidePmHooks& h, const BrainTickPm& b, zgame::Handle v, zgame::Handle net, zgame::Handle state) {
  if (b.mode != 1) return false;
  if (net && !h.net_allows(net)) return false;
  if (state && !h.state_allows(state)) return false;
  if ((b.engage_flags & 1) && b.home) {
    float p[3];
    h.position(v, p);
    const float x = p[0] - b.home[0], y = p[1] - b.home[1], z = p[2] - b.home[2];
    const float d2 = static_cast<float>(double{x} * x + double{y} * y + double{z} * z);
    if (d2 > b.leash) return false;
  }
  if ((b.engage_flags & 4) && !(h.health(v) >= b.health_floor)) return false;
  if ((b.engage_flags & 2) && h.now() >= b.attack_until) return false;
  if ((b.engage_flags & 8) && !(h.health(v) >= b.give_up)) return false;
  return true;
}

// Pirate's Moon 1.0 @ 0x004fab30 (MW3's 0x004e96b0)
void manoeuvre_pm(BrainSidePmHooks& h, BrainTickPm& b) {
  if (b.manoeuvre_kind != 0 && b.manoeuvre_kind != 1) return;
  h.push_behaviour(b.manoeuvre_kind);
  b.wait_until = kFltMax;
}

// Pirate's Moon 1.0 @ 0x004fbc20 (MW3's 0x004ea520): the base's record comes last, not first, and allies turn on
// the player for hits lasting over 3 s, the last within 7 s, more than 10 in all, where MW3 asks only the 3 s.
void on_hit_pm(BrainSidePmHooks& h, const BrainTickPm& b, zgame::Handle owner, int32_t x, int32_t y, int32_t z,
               zgame::Handle attacker) {
  if (owner != attacker && attacker) {
    if (!(h.team(attacker) & h.allies(owner))) {
      const zgame::Handle p = h.player_contact(owner);
      if (p && h.dist2(p) > 500.0f && b.radio) h.radio_line(0x00);
    } else {
      const zgame::Handle v = h.attacker_vehicle(attacker);
      if (v == h.player_vehicle()) {
        if (b.radio) h.radio_line(0x20);
        if (const float* r = h.hit_record(v)) {
          if (!(double{h.clock()} - r[1] >= 7.0) && double{h.clock()} - r[2] > 3.0 && r[0] > 10.0f) h.allies_turn();
        }
      }
    }
  }
  h.hit_base(x, y, z, attacker);
}

// Pirate's Moon 1.0 @ 0x004fbdd0 (MW3's 0x004ea6b0)
void died_pm(BrainSidePmHooks& h, const BrainTickPm& b) {
  h.died_base();
  if (b.radio) h.radio_line(0x18);
}

// Pirate's Moon 1.0 @ 0x004d5a10 (MW3's 0x004cbca0)
void scan_run_pm(ScanPmHooks& h, OrdersPm& o, int32_t tests, zgame::Handle next) {
  for (int32_t i = 0; i < tests; ++i) h.scan_test(i);
  o.again = 1;
  h.set_behaviour(next);
}

// Pirate's Moon 1.0 @ 0x004c7800: every kind's behaviour by vt+0x90 of its slot; any other kind slot 0.
zgame::Handle kind_behaviour_pm(NetSlotsPmHooks& h, int32_t kind) {
  static constexpr int8_t kSlot[8] = {0, 2, 3, 4, 6, 7, 8, 9};
  const zgame::Handle s = h.slot(kind >= 1 && kind <= 7 ? kSlot[kind] : 0);
  return s ? h.step_of(s, 0x90) : 0;
}

// Pirate's Moon 1.0 @ 0x004c7920
zgame::Handle follow_pm(NetSlotsPmHooks& h) {
  const zgame::Handle s = h.slot(0);
  return s ? h.step_of(s, 0x90) : 0;
}

// Pirate's Moon 1.0 @ 0x004c7950
zgame::Handle attack_pm(NetSlotsPmHooks& h) {
  const zgame::Handle s = h.slot(1);
  return s ? h.step_of(s, 0x94) : 0;
}

// Pirate's Moon 1.0 @ 0x004c7980
zgame::Handle net_slot5_pm(NetSlotsPmHooks& h) {
  const zgame::Handle s = h.slot(5);
  return s ? h.step_of(s, 0x98) : 0;
}

// Pirate's Moon 1.0 @ 0x004fa6d0 (MW3's 0x004e9070)
bool may_fire_pm(OrdersPmHooks& h, OrdersPm& o) { return h.may_fire(o.orders.back().state); }

// Pirate's Moon 1.0 @ 0x004fa470
bool orders_idle_pm(OrdersPmHooks& h, OrdersPm& o) {
  const OrderPm& top = o.orders.back();
  return (!top.net || h.net_idle(top.net)) && (!top.state || h.state_idle(top.state));
}

// Pirate's Moon 1.0 @ 0x004f9e70
void order_parts_pm(OrdersPmHooks& h, OrderPm& o, zgame::Handle net, zgame::Handle state) {
  const zgame::Handle s = state ? state : h.default_state();
  const zgame::Handle n = net ? net : h.default_net();
  o.net = h.make_net(n);
  o.state = h.make_state(s);
}

// Pirate's Moon 1.0 @ 0x004ea220: the order under it takes up again.
void pop_order_pm(OrdersPmHooks& h, OrdersPm& o) {
  const OrderPm& top = o.orders.back();
  if (top.state) h.free_part(top.state);
  if (top.net) h.free_part(top.net);
  o.orders.pop_back();
  if (!o.orders.empty()) h.resume(o.orders.back().state);
}

// Pirate's Moon 1.0 @ 0x004fa860: every order goes for one new one, on the given net standing still.
bool replace_orders_pm(OrdersPmHooks& h, OrdersPm& o, zgame::Handle obj) {
  const zgame::Handle net = obj ? h.as_net(obj) : 0;
  if (!net) return false;
  while (!o.orders.empty()) pop_order_pm(h, o);
  o.orders.emplace_back();
  o.orders.back().net = h.make_net(net);
  o.orders.back().state = h.make_state(h.stand_state());
  return true;
}

// Pirate's Moon 1.0 @ 0x004fb260 (the loop of MW3's frame): one pending item, once the last order is ready for it.
void run_orders_pm(OrdersPmHooks& h, OrdersPm& o, float dt) {
  do {
    if (!o.pending.empty() && o.orders.back().ready) {
      const zgame::Handle item = o.pending.front();
      o.pending.pop_front();
      h.run_pending(item);
    }
    o.again = 0;
    h.run_order(o.orders.back(), dt);
  } while (o.again);
}

// Pirate's Moon 1.0 @ 0x004ea5f0: a new order on top of the one told to stop.
void stop_orders_pm(OrdersPmHooks& h, OrdersPm& o) {
  h.stop_state(o.orders.back().state);
  o.orders.emplace_back();
}

// Pirate's Moon 1.0 @ 0x004ea740 (also 0x004fa150): `behaviour` on `net`, over what was running.
void give_order_pm(OrdersPmHooks& h, OrdersPm& o, zgame::Handle net, zgame::Handle behaviour) {
  stop_orders_pm(h, o);
  order_parts_pm(h, o.orders.back(), net, behaviour);
}

// Pirate's Moon 1.0 @ 0x004ea7f0 (also 0x004fa170): on the last order's own net.
void push_behaviour_pm(OrdersPmHooks& h, OrdersPm& o, zgame::Handle behaviour) {
  give_order_pm(h, o, h.net_of(o.orders.back().net), behaviour);
}

namespace {
float root_bits(float v) {  // the exe's: half the bits, plus 0x1fc00000
  int32_t i;
  std::memcpy(&i, &v, 4);
  i = (i >> 1) + 0x1fc00000;
  std::memcpy(&v, &i, 4);
  return v;
}
float dot(const float a[3], const float b[3]) {
  return static_cast<float>((double{a[0]} * b[0] + double{a[1]} * b[1]) + double{a[2]} * b[2]);
}
}  // namespace

// Pirate's Moon 1.0 @ 0x004e7210: the same three ways to the time as MW3's, at the register's width. Each float
// below is one the exe stores.
bool lead_point_pm(LeadPmHooks& h, zgame::Handle self, zgame::Handle entity, zgame::Handle weapon, float out[3]) {
  const float* tv = h.velocity(entity);
  const float* smoothed = h.smoothed_velocity(self);
  const float* seen = smoothed ? smoothed : tv;
  h.aim_point(entity, out);
  const uint32_t flags = weapon ? h.weapon_flags(weapon) : 0;
  const float speed = weapon ? h.weapon_speed(weapon) : 0.0f;
  const float* m = !weapon || (flags & 0x800) || (flags & 2) == 2 || speed == 0.0f || std::isnan(speed) ? nullptr : h.muzzle(weapon);
  if (!m) {
    for (int i = 0; i < 3; ++i) out[i] = out[i] - (tv[i] - seen[i]);
    return true;
  }
  const float d[3] = {out[0] - m[0], out[1] - m[1], out[2] - m[2]};
  const float* own = h.velocity(self);
  float v[3] = {seen[0] - own[0], seen[1] - own[1], seen[2] - own[2]};
  const float vv = dot(v, v), dd = dot(d, d), dv = dot(d, v);
  const float a = static_cast<float>(double{speed} * speed - vv);
  const double far = std::fabs(double{vv});
  const bool still = !(far >= 0.001);  // 0x005dfd38
  double t;
  if (a == 0.0f || std::isnan(a)) {
    if (dv == 0.0f || std::isnan(dv)) return false;
    const double two = double{dv} * speed;
    t = dd / (two + two);
  } else if (still) {
    t = root_bits(dd) / double{speed};
  } else {
    const double disc = double{dv} * dv + double{dd} * a;
    if (disc > 0.0) {
      const float root = root_bits(static_cast<float>(disc));
      const float inv = static_cast<float>(1.0 / a);
      t = (double{root} - dv) * inv;
      const float near = static_cast<float>((-double{dv} - root) * inv);
      if (near > 0.0f) t = near;
    } else {
      if (!(disc >= 0.0)) return false;
      t = -(double{dv} / a);
    }
  }
  if (!(t > 0.0)) return false;
  if (still) return true;
  for (int i = 0; i < 3; ++i) out[i] = out[i] + static_cast<float>(double{v[i]} * t);
  return true;
}

// Pirate's Moon 1.0 @ 0x004e6330
void set_target_pm(TargetsPmHooks& h, TargetsPm& t, zgame::Handle contact, bool keep_sides) {
  if (!keep_sides) {
    if (!contact) {
      if (t.ally == t.target) t.ally = 0;
      if (t.enemy == t.target) t.enemy = 0;
    } else {
      const uint32_t team = h.team_of(contact);
      if (team & t.allies) {
        t.ally = contact;
        if (!(t.flags & 0x20) && (h.flags_of(contact) & 0x10)) t.marked = contact;
      } else if (team & t.enemies) {
        t.enemy = contact;
      }
    }
  }
  t.target = contact;
}

// Pirate's Moon 1.0 @ 0x004fbe90. Compared in the register, stored after: a speed that only rounds to 0 stays.
void way_reset_pm(BrainSidePmHooks& h, zgame::Handle v, float throttle, const float spot[3], float from[3]) {
  float speed = 1.0f;
  float here[3] = {spot[0], spot[1], spot[2]};
  if (v) {
    const double s = h.speed_at(v, throttle);
    speed = s == 0.0 ? 1.0f : static_cast<float>(s);
    if (h.in_world(v)) h.position(v, here);
  }
  from[0] = here[0], from[1] = here[1], from[2] = here[2];
  h.set_way(from, here, speed, v);
}

}  // namespace mech3::ai
