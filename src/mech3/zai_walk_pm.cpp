#include "mech3/zai_walk_pm.h"

#include <cfloat>
#include <cmath>
#include <cstring>

namespace mech3::ai {

// 0x00537e60: to - from, flat, normalized.
float flat_dir_pm(const float from[3], const float to[3], float out[3]) {
  out[0] = to[0] - from[0];
  out[1] = 0.0f;
  out[2] = to[2] - from[2];
  return unit_flat(&out[0], &out[2], false);  // 0x004013d0
}

// Pirate's Moon 1.0 @ 0x004d9870. The brake's share is from the pilot's +0x544: 2^n in the float's
// exponent, as the exe builds it.
WalkPm::WalkPm(WalkPmHooks& hooks, SteerPm& s, zgame::Handle v) : h(hooks), steer(s), vehicle(v) {
  refresh();
  float b = 0.1f;  // [0x0060bbd0]
  if (h.has_pilot(v)) {
    const float e = -(h.pilot_x544(v) * 0.125f) * 12102200.0f;  // 0x0060bb50, 0x005e00c8
    const int32_t n = static_cast<int32_t>(e);  // 0x005cef9a, truncating
    const int32_t raw = n + 0x3f800000;
    float f;
    std::memcpy(&f, &raw, 4);
    b = (1.0f - f) / f;
  }
  steer.brake = b;
}

void WalkPm::refresh() {
  h.position(vehicle, pos);
  h.forward(vehicle, &fwd[0], &fwd[1]);
}

// Pirate's Moon 1.0 @ 0x004d92a0
void WalkPm::measure() {
  off_along = std::fabs(steer.along);
  off_sum = off_along;
  off_side = std::fabs(steer.side);
  off_sum = off_side + off_sum;
}

// Pirate's Moon 1.0 @ 0x004d92c0: toward the carrot; turning about at the end of the way when
// it's behind.
void WalkPm::aim_turnabout() {
  refresh();
  flat_dir_pm(pos, to, dir);
  dot = wide_sum(fwd[1], dir[2], fwd[0], dir[0]);
  const float left = steer.along_at - steer.along_min;
  if (-steer.turnabout_room < left && left < 0.0f && dot < steer.cos_turnabout) {
    dot = -dot;
    dir[0] = -dir[0];
    dir[2] = -dir[2];
    backing = 1;
  }
}

// Pirate's Moon 1.0 @ 0x004d9370
void WalkPm::aim() {
  refresh();
  flat_dir_pm(pos, to, dir);
}

// Pirate's Moon 1.0 @ 0x004d9390 (MW3's 0x004cdd40): 0 within the dead zone, past it the side times
// the gain, at least `least_turn`; full when behind, the way it was turning.
void WalkPm::turn() {
  refresh();
  dot = wide_sum(fwd[1], dir[2], fwd[0], dir[0]);
  const float keep = h.turn_goal();
  turn_out = keep;
  cross = 0.0f;
  if (!(steer.cos_behind <= dot)) {
    turn_out = 0.0f <= keep ? 1.0f : -1.0f;
    return;
  }
  if (steer.cos_dead <= dot) {
    turn_out = 0.0f;
    return;
  }
  cross = wide_sum(fwd[1], dir[0], -dir[2], fwd[0]);
  turn_out = 0.0f;
  if (dot < 0.0f) {
    turn_out = 0.0f <= cross ? 1.0f : -1.0f;
    return;
  }
  if (-steer.sin_dead <= cross) {
    if (steer.sin_dead < cross) {
      const double v = (double{cross} - steer.sin_dead) * steer.gain;
      turn_out = static_cast<float>(v);
      if (1.0 < v) turn_out = 1.0f;
      else if (v < steer.least_turn) turn_out = steer.least_turn;
    }
    return;
  }
  const double v = (double{steer.sin_dead} + cross) * steer.gain;
  turn_out = static_cast<float>(v);
  if (v < -1.0) turn_out = -1.0f;
  else if (-steer.least_turn < v) turn_out = -steer.least_turn;
}

// Pirate's Moon 1.0 @ 0x004d9550: not facing the way, only braking off; facing it, by how far the
// carrot is, within the brain's most.
void WalkPm::throttle() {
  if (!(steer.cos_on < dot)) {
    const float t = h.throttle_goal();
    throttle_out = t;
    if (steer.brake < std::fabs(t)) throttle_out = static_cast<float>(std::copysign(1.0, -static_cast<double>(t)));
    else throttle_out = 0.0f;
    return;
  }
  float a = off_along - steer.far_along;
  double b = double{off_side} - steer.far_side;
  if (!(a > 0.0f) && !(b > 0.0)) {
    throttle_out = 0.0f;
    return;
  }
  if (a < 0.0f) a = 0.0f;
  if (b < 0.0) b = 0.0;
  const double v = double{a} * steer.gain_along + b * steer.gain_side;
  throttle_out = static_cast<float>(v);
  const float most = h.max_throttle();
  if (v > most) throttle_out = most;
  else if (v < steer.least) throttle_out = steer.least;
  if (backing) throttle_out = -throttle_out;
}

void WalkPm::go(bool by_near) {
  steer.carrot(to);
  if (by_near && steer.near(h)) {
    aim_turnabout();
  } else {
    refresh();
    if (h.pull(vehicle, pos, to, to)) aim();
    else aim_turnabout();
  }
  turn();
  throttle();
}

// Pirate's Moon 1.0 @ 0x004d9680
void WalkPm::walk() { go(true); }

// Pirate's Moon 1.0 @ 0x004d9740
void WalkPm::walk_on() { go(false); }

// Pirate's Moon 1.0 @ 0x004d9710
void WalkPm::walk_straight() {
  steer.carrot(to);
  aim_turnabout();
  turn();
  throttle();
}

// Pirate's Moon 1.0 @ 0x004d97a0: braking off first; then toward the target, or the way's start.
void WalkPm::hold() {
  const float t = h.throttle_goal();
  throttle_out = t;
  if (steer.brake < std::fabs(t)) {
    throttle_out = static_cast<float>(std::copysign(1.0, -static_cast<double>(t)));
    turn_out = 0.0f;
    return;
  }
  throttle_out = 0.0f;
  if (h.has_target_contact(vehicle)) {
    h.position(h.target_entity(vehicle), to);
  } else {
    to[0] = steer.start[0], to[1] = steer.start[1], to[2] = steer.start[2];
  }
  aim();
  turn();
}

// Pirate's Moon 1.0 @ 0x004d9920: where the way begins, the start itself when none of it is cut off.
const float* SteerPm::way_begin(float out[3]) const {
  if (along_min == 0.0f) return start;
  out[0] = start[0] + along_min * dir[0];
  out[1] = start[1];
  out[2] = start[2] + along_min * dir[1];
  return out;
}

// Pirate's Moon 1.0 @ 0x004d9b10
FightStatePm::FightStatePm() : stop_dist(FLT_MAX), range_far(FLT_MAX), until(FLT_MAX) {}

// Pirate's Moon 1.0 @ 0x004d9be0. The exe also makes the vehicle's boundary record at +0xd8 if it
// has none; here that's kept on the first query (inout_pull). `owned` is dropped unfreed, as written.
void FightStatePm::enter(WalkPmHooks& h, zgame::Handle vehicle) {
  flags &= ~0xfu;
  speed = static_cast<float>(h.speed_at(vehicle, h.max_throttle()));
  wake = 0.0f;
  until = FLT_MAX;
  stop_dist = static_cast<float>(h.stop_distance(vehicle, speed));
  range_far = h.range_far();
  range_near = h.range_near();
  x1c = 200.0f;
  owned = nullptr;
  x2d = 0;
  pulled = 0;
  range_mid = (range_near + range_far) * 0.5f;
}

// Pirate's Moon 1.0 @ 0x004d9ce0
void FightStatePm::reset() {
  until = FLT_MAX;
  flags &= ~0xfu;
  wake = 0.0f;
  pulled = 0;
}

// Pirate's Moon 1.0 @ 0x004d9cc0
void FightStatePm::release() {
  delete owned;
  owned = nullptr;
}

// Pirate's Moon 1.0 @ 0x004da1c0: lays the way, measured back from the place to stand, and how far
// short of it to stop. Three blocks can't run and aren't here (docs/pm-map.md): slots vt+0x28 and the
// context's vt+0x24 are null in every table, and the lane check's byte 0x0062fb20 is never set.
void FightStatePm::plan(WalkPmHooks& h, WalkFightPm& c) {
  c.refresh();
  bool keep = true;
  float dist = c.target_dist;
  const float* from = c.pos;
  const float* target = c.target_pos;
  const float* way = c.to_target;
  float start[3], back[3], out[3];
  if (!c.at_stop && c.steer.length < c.steer.along_at) {
    const float* w = h.way_from();
    start[0] = w[0], start[1] = w[1], start[2] = w[2];
    from = start;
    dist = flat_dir_pm(start, target, back);
    way = back;
  } else if (pulled && c.at_stop && c.way_dist >= range_mid) {
    from = c.way_point;
    dist = c.way_dist;
    way = c.way_dir;
  }
  if (dist < range_mid) {
    const float neg[3] = {-way[0], -way[1], -way[2]};
    back[0] = neg[0], back[1] = neg[1], back[2] = neg[2];
    for (int i = 0; i < 3; ++i) out[i] = target[i] + back[i] * range_far;
    target = out;
    dist = range_far;
    way = back;
    keep = false;
  }
  pulled = h.pull(c.vehicle, from, target, out);
  if (pulled) {
    target = out;
    dist = flat_dir_pm(from, out, back);
    way = back;
    keep = false;
    if (dist == 0.0f) {
      constexpr float kApart = 0.01f;  // 0x0060bb60
      for (int i = 0; i < 3; ++i) start[i] = out[i] - kApart * back[i];
      from = start;
      dist = kApart;
    }
  }
  if (dist > range_mid + x1c) {
    for (int i = 0; i < 3; ++i) out[i] = from[i] + x1c * way[i];
    target = out;
    keep = false;
    pulled = 0;
  }
  float* w = h.way_from();
  w[0] = from[0], w[1] = from[1], w[2] = from[2];
  c.steer.set(h, w, target, speed, c.vehicle);
  if (keep) {
    c.steer.along_min = range_mid;
  } else if (pulled && range_near > 0.0f) {
    float l[2];
    c.steer.on_line(c.target_pos, l);
    if (range_near > std::fabs(l[1])) {
      const float half = std::sqrt(range_near * range_near - l[1] * l[1]);
      const float end = l[0] + half;
      if (end < 0.0f || l[0] - half > 0.0f) c.steer.along_min = 0.0f;
      else c.steer.along_min = end < c.steer.length ? end : c.steer.length;
    }
  }
  wake = h.now() + 10.0f;  // 0x0060bb58
  until = h.now() + 1.0f;  // 0x0060bb5c
}

// Pirate's Moon 1.0 @ 0x004d9d00: one frame of walking to where it can fight from. The steps' numbers
// are the exe's, in the low four bits of `flags`: 1 and 3..6 holding, 2 and 7 walking, 8 and 9 a via node
// queued, 10 the target gone.
void FightStatePm::step(WalkPmHooks& h, WalkFightPm& c) {
  bool second = false;  // the local at [esp+0x28]: the last try before holding
  auto set = [&](uint32_t n) { flags = (flags & ~0xfu) | n; };
  auto drive = [&](uint32_t n) {
    h.set_turn(c.turn_out);
    h.set_throttle(c.throttle_out);
    set(n);
  };
  auto hold = [&](uint32_t n) {
    c.hold();
    drive(n);
  };
  auto queued = [&](uint32_t n) {
    if (!pulled) return false;
    const InOutVia* via = h.at_via(c.vehicle);
    if (!via) return false;
    h.queue_via(via);
    set(n);
    return true;
  };
  switch (flags & 0xf) {
    case 10:
      wake = 0.0f;
      until = FLT_MAX;
      pulled = 0;
      [[fallthrough]];
    case 0:
      c.measure();
      plan(h, c);
      goto after_first_plan;
    case 8:
      plan(h, c);
    after_first_plan:
      if (queued(8)) return;
      [[fallthrough]];
    case 1:
      if (c.no_target) goto gone;
      if (c.in_range) return hold(1);
      goto try_walk;
    case 2:
      c.measure();
      c.decide(*this);
      goto walk_or_on;
    case 3:
      c.measure();
      c.decide(*this);
      goto wait_at_stop;
    case 4:
      c.measure();
      goto until_replan;
    case 5:
      goto range_after_plan;
    case 6:
      c.measure();
      second = true;
      goto try_walk;
    case 7:
      c.measure();
      goto decide_walk;
    case 9:
      plan(h, c);
      goto after_second_plan;
    default:
      return;
  }
try_walk:
  if (c.no_target) goto gone;
  c.measure();
decide_walk:
  c.decide(*this);
  if (c.may_go) {
    if (c.steer.near(h)) goto walk_or_on;
    c.walk_straight();
    return drive(7);
  }
  goto wait_at_stop;
walk_or_on:
  if (c.may_go) {
    c.walk();
    return drive(2);
  }
wait_at_stop:
  if (!c.no_target && c.at_stop && c.in_range) return hold(3);
until_replan:
  if (h.now() < until) return hold(4);
  plan(h, c);
after_second_plan:
  if (queued(9)) return;
range_after_plan:
  if (!c.no_target && c.in_range) return hold(5);
  if (second) return hold(6);
  second = true;
  goto try_walk;
gone:
  c.steer.carrot(c.to);
  {
    float* w = h.way_from();
    w[0] = c.to[0], w[1] = c.to[1], w[2] = c.to[2];
  }
  set(10);
}

// Pirate's Moon 1.0 @ 0x004d99a0
WalkFightPm::WalkFightPm(WalkPmHooks& hooks, SteerPm& s, zgame::Handle v, FightStatePm& st)
    : WalkPm(hooks, s, v), state(st) {}

// Pirate's Moon 1.0 @ 0x004d99e0. The exe also tests its pointer to the vehicle's place, which is
// never null (0x005cb260).
void WalkFightPm::measure_target() {
  no_target = !target_pos;
  if (no_target) return;
  refresh();
  target_dist = flat_dir_pm(pos, target_pos, to_target);
  in_range = state.range_near < target_dist && target_dist < state.range_far;
}

// Pirate's Moon 1.0 @ 0x004d9a90: the target's distance from where the way begins, and whether the
// vehicle has come to its end.
void WalkFightPm::measure() {
  WalkPm::measure();
  way_point = steer.way_begin(way_own);
  way_dist = flat_dir_pm(target_pos, way_point, way_dir);
  off_way = way_dist >= state.range_near;
  at_stop = steer.at_end() && off_sum < state.stop_dist;
}

// Pirate's Moon 1.0 @ 0x004daa70: the target's place, and none unless it's live.
void WalkFightPm::take_order_target() {
  const zgame::Handle t = h.order_target();
  target_pos = h.target_place(t);
  measure_target();
  no_target = no_target || !h.target_live(t);
}

// Pirate's Moon 1.0 @ 0x004debe0: `mode` the brain's +0x3c.
WalkAttackPm::WalkAttackPm(WalkPmHooks& hooks, SteerPm& s, zgame::Handle v, FightStatePm& st, const float* held, int32_t mode)
    : WalkFightPm(hooks, s, v, st) {
  if (!held) {
    const zgame::Handle t = h.order_target();
    target_pos = h.target_place(t);
    measure_target();
    if (!no_target && h.target_live(t) && !h.must_leave(v)) return;
  } else {
    target_pos = held;
    measure_target();
    if (!no_target && mode != 1) return;
  }
  no_target = 1;
}

// Pirate's Moon 1.0 @ 0x004dfb50. The exe reads the place without testing it after the test.
WalkGuardPm::WalkGuardPm(WalkPmHooks& hooks, SteerPm& s, zgame::Handle v) : WalkPm(hooks, s, v) {
  target = h.order_target();
  target_pos = h.target_place(target);
  gone = !target_pos || !h.target_live(target) || h.must_leave(v);
  target_dist = flat_dir_pm(pos, target_pos, to_target);
}

// Pirate's Moon 1.0 @ 0x004dfec0: a point at a random distance and bearing round the target, the boundary's pull on
// it from the vehicle, and the way there laid from the steering's start.
void WalkGuardPm::pick(GuardPickPm& g) {
  constexpr double kRand = 3.0518509447574615e-05;  // 0x005dfcb4
  constexpr double kTurn = 6.2831854820251465;      // 0x005e14d0
  const int32_t r1 = h.rand_int();
  const float r = static_cast<float>((static_cast<double>(h.reach()) - g.reach) * static_cast<float>(r1) * kRand + g.reach);
  const int32_t r2 = h.rand_int();
  const double a = r2 * kRand * kTurn;
  const float x = static_cast<float>(std::cos(a) * r), z = static_cast<float>(std::sin(a) * r);
  g.at[0] = target_pos[0] + x;
  g.at[1] = target_pos[1] + 0.0f;
  g.at[2] = target_pos[2] + z;
  float here[3];
  h.position(vehicle, here);  // 0x004ccac0 takes it afresh, not the measure's
  h.pull(vehicle, here, g.at, g.at);
  float* from = h.way_from();
  from[0] = steer.start[0], from[1] = steer.start[1], from[2] = steer.start[2];
  steer.set(h, from, g.at, g.rate, vehicle);
}

// Pirate's Moon 1.0 @ 0x004dfc30
void WalkGuardPm::measure() {
  WalkPm::measure();
  float d[3];
  from_way = flat_dir_pm(steer.start, target_pos, d);
}

// Pirate's Moon 1.0 @ 0x004d9b90
void WalkFightPm::decide(const FightStatePm& s) {
  may_go = !no_target && h.now() < s.wake && off_way && !at_stop;
}

}  // namespace mech3::ai
