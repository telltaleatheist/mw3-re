#include "mech3/mech_control.h"

#include <cmath>
#include <cstring>

#include "mech3/mech.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {
constexpr float kExpScale = 12102200.0f;  // 0x005bd5e8: 2^23 / ln 2
constexpr float kJumpRise = 7.0f;         // 0x005e2598
constexpr float kJumpFall = 9.0f;         // 0x005e259c, toward a zero target

// MSVCRT _ftol (0x005ac340): truncates.
int32_t ftol(float x) { return static_cast<int32_t>(x); }

}  // namespace

// exp(x) by writing x / ln 2 straight into a float's exponent (Schraudolph):
// within a few percent of the true value, and what the game uses.
float fast_exp(float x) {
  const int32_t bits = ftol(x * kExpScale) + 0x3f800000;
  float f;
  std::memcpy(&f, &bits, 4);
  return f;
}

// Mech3.exe 1.2 @ 0x00426960
int MechControls::update(float dt) {
  if (owner->powered_down) {
    zero();
    return 1;
  }
  if (throttle_target - throttle != 0.0f) {
    if (std::fabs(throttle_target - throttle) < 0.009) {
      throttle = throttle_target;
    } else {
      const float rate = std::fabs(throttle) <= std::fabs(throttle_target) ? owner->throttle_up
                                                                           : owner->throttle_down;
      const float k = fast_exp(-(rate * dt));
      throttle = (1.0f - k) * throttle_target + k * throttle;
    }
  }
  if (turn != turn_target) {
    float rate = owner->turn_accel;
    if (flags & 4) rate = rate + rate;
    const float target = turn_target;
    turn = (target - turn) * rate * dt + turn;
    if (std::fabs(target - turn) < 0.05f) turn = target;
  }
  if (jump != jump_target) {
    const float rate = jump_target == 0.0f ? kJumpFall : kJumpRise;
    const float target = jump_target;
    jump = (target - jump) * rate * dt + jump;
    if (std::fabs(target - jump) < 0.05f) jump = target;
  }
  return 1;
}

// Mech3.exe 1.2 @ 0x00426b60
void MechControls::set_throttle(float v) {
  if (v > 0.0f) reverse = false;
  else if (v < 0.0f) reverse = true;
  throttle_target = v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v;
}

// Mech3.exe 1.2 @ 0x00426c80
void MechControls::zero() {
  jj_strafe = jj_forward = twist = pitch = axis_30 = axis_2c = 0;
  throttle_target = throttle = turn_target = turn = jump_target = jump = 0;
}

}  // namespace mech3
