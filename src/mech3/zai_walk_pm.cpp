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
  const float len = std::sqrt(out[0] * out[0] + out[2] * out[2]);  // 0x004013d0
  if (len != 0.0f) {
    const float k = 1.0f / len;
    out[0] *= k, out[2] *= k;
  }
  return len;
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
  dot = fwd[1] * dir[2] + fwd[0] * dir[0];
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
  dot = fwd[1] * dir[2] + fwd[0] * dir[0];
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
  cross = fwd[1] * dir[0] - dir[2] * fwd[0];
  turn_out = 0.0f;
  if (dot < 0.0f) {
    turn_out = 0.0f <= cross ? 1.0f : -1.0f;
    return;
  }
  if (-steer.sin_dead <= cross) {
    if (steer.sin_dead < cross) {
      const float v = (cross - steer.sin_dead) * steer.gain;
      turn_out = v;
      if (1.0 < v) turn_out = 1.0f;
      else if (v < steer.least_turn) turn_out = steer.least_turn;
    }
    return;
  }
  const float v = (steer.sin_dead + cross) * steer.gain;
  turn_out = v;
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
  float b = off_side - steer.far_side;
  if (!(a > 0.0f) && !(b > 0.0f)) {
    throttle_out = 0.0f;
    return;
  }
  if (a < 0.0f) a = 0.0f;
  if (b < 0.0f) b = 0.0f;
  const float v = a * steer.gain_along + b * steer.gain_side;
  throttle_out = v;
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
  speed = h.speed_at(vehicle, h.max_throttle());
  wake = 0.0f;
  until = FLT_MAX;
  stop_dist = h.stop_distance(vehicle, speed);
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

// Pirate's Moon 1.0 @ 0x004d99a0
WalkFightPm::WalkFightPm(WalkPmHooks& hooks, SteerPm& s, zgame::Handle v, FightStatePm& st)
    : WalkPm(hooks, s, v), state(st) {}

// Pirate's Moon 1.0 @ 0x004d99e0. The exe also tests its pointer to the vehicle's place, which is
// never null (0x005cb260).
void WalkFightPm::measure_target() {
  no_target = !target_pos;
  if (no_target) return;
  refresh();
  to_target[0] = target_pos[0] - pos[0];
  to_target[1] = 0.0f;
  to_target[2] = target_pos[2] - pos[2];
  target_dist = std::sqrt(to_target[0] * to_target[0] + to_target[2] * to_target[2]);  // 0x004013d0
  if (target_dist != 0.0f) {
    const float k = 1.0f / target_dist;
    to_target[0] *= k, to_target[2] *= k;
  }
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

// Pirate's Moon 1.0 @ 0x004d9b90
void WalkFightPm::decide(const FightStatePm& s) {
  may_go = !no_target && h.now() < s.wake && off_way && !at_stop;
}

}  // namespace mech3::ai
