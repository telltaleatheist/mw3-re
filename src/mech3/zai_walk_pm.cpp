#include "mech3/zai_walk_pm.h"

#include <cmath>
#include <cstring>

namespace mech3::ai {

namespace {
// 0x00537e60: to - from, flat, normalized.
void flat_dir(const float from[3], const float to[3], float out[3]) {
  out[0] = to[0] - from[0];
  out[1] = 0.0f;
  out[2] = to[2] - from[2];
  const float len = std::sqrt(out[0] * out[0] + out[2] * out[2]);  // 0x004013d0
  if (len != 0.0f) {
    const float k = 1.0f / len;
    out[0] *= k, out[2] *= k;
  }
}
}  // namespace

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
  flat_dir(pos, to, dir);
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
  flat_dir(pos, to, dir);
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
    if (h.way_blocked(vehicle, pos, to)) aim();
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

}  // namespace mech3::ai
