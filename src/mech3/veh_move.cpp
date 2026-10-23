#include "mech3/veh_move.h"

#include <cmath>

#include "gamez/zmath/zmth_matrix.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {
VehBasicHooks& hk() { return veh_basic_hooks(); }
float sign1(float x) { return x >= 0.0f ? 1.0f : -1.0f; }  // the exe's: 1 at 0, -1 for NaN
}  // namespace

// Mech3.exe 1.2 @ 0x004a2a00: the turn's own (turns[0]), damped to 0 without one.
void VehicleControls::turn_step(const VehicleMotion& m, float dt) {
  const float t = turn;  // IControls vt+0x24
  if (t == 0.0f || std::isnan(t)) {
    x50.y = zmth::exp_neg(m.turn_damping * dt) * x50.y;
    return;
  }
  if ((t > 0.0f && !(x50.y >= 0.0f)) || (t < 0.0f && x50.y > 0.0f)) x50.y = 0.0f;
  const float r = (m.turns[0] * t) * dt + x50.y;
  x50.y = r;
  if (r > top_turn) x50.y = top_turn;
  else if (!(r >= -top_turn)) x50.y = -top_turn;
}

// Mech3.exe 1.2 @ 0x004a28c0
void vehicle_turn_to_heading(BasicVehicle& v) {
  VehicleControls& c = *v.controls;
  if (!c.x760) return;
  if (c.turn != 0.0f && !std::isnan(c.turn)) c.x760 = 0;  // IControls vt+0x24 (a NaN counts as none)
  if (!c.x760) return;
  const float dt = hk().frame_dt();
  const float step = static_cast<float>(std::cos(double{dt * c.motion.turns[1]}));
  const float dot = v.forward.z * c.heading.z + v.forward.x * c.heading.x;
  const float cross = c.heading.x * v.forward.z - v.forward.x * c.heading.z;
  if (step > dot) {  // further than a frame's turn
    const float s = cross < 0.0f || std::isnan(cross) ? -1.0f : 1.0f;
    c.turn = s;  // IControls vt+0x28
    c.x50.y = s * c.motion.turns[1];
    return;
  }
  c.x760 = 0;
  c.turn = 0.0f;
  c.angles.y = static_cast<float>(std::atan2(-double{c.heading.x}, -double{c.heading.z}));
  c.x50.y = 0.0f;
}

// Mech3.exe 1.2 @ 0x004a2890
void vehicle_up_refresh(BasicVehicle& v) {
  hk().refresh(v);
  const float* m = v.transform;
  v.controls->up = {m[3], m[4], m[5]};
}

// Mech3.exe 1.2 @ 0x004a2b10
void vehicle_speed_step(BasicVehicle& v) {
  VehicleControls& c = *v.controls;
  if (c.sliding) return vehicle_slide(v);
  const VehicleMotion& m = c.motion;
  const float dt = hk().frame_dt();
  const float t = c.throttle;  // IControls vt+0x1c, asked each time
  if (t == 0.0f || std::isnan(t)) {
    c.x70.z = zmth::exp_neg(m.rate_damping[1] * dt) * c.x70.z;
  } else {
    // "Below" tests pass unordered values (the x87's C0), "above" ones do not.
    if ((t > 0.0f && c.x70.z > 0.0f) || (t < 0.0f && !(c.x70.z >= 0.0f)))
      c.x70.z = zmth::exp_neg(m.rate_damping[1] * dt) * c.x70.z;
    c.x70.z = c.x70.z - (t * m.rates[0]) * dt;
    const float cap = std::fabs(t) * c.top_speed;
    if (c.x70.z > cap) c.x70.z = cap;
    else if (!(c.x70.z >= -cap)) c.x70.z = -cap;
  }
  if (c.x70.x != 0.0f && !std::isnan(c.x70.x)) c.x70.x = zmth::exp_neg(m.rate_damping[0] * dt) * c.x70.x;
  if (!(std::fabs(c.x70.x) >= 0.01f)) c.x70.x = 0.0f;
}

// Mech3.exe 1.2 @ 0x004a2dd0
void vehicle_slide(BasicVehicle& v) {
  VehicleControls& c = *v.controls;
  const float dt = hk().frame_dt();
  const float fx = v.forward.x, fz = v.forward.z;
  c.x70.x = -fz * c.velocity.x - -fx * c.velocity.z;
  c.x70.z = -fz * c.velocity.z + -fx * c.velocity.x;
  const float top = c.top_speed;
  const float z = c.x70.z - (c.throttle * c.motion.rates[0]) * dt;
  c.x70.z = z;
  if (z > top) c.x70.z = top;
  else if (!(z >= -top)) c.x70.z = -top;
  float s = vehicle_side_force(v) * dt + c.x70.x;
  if (c.x70.x != 0.0f && !std::isnan(c.x70.x) && sign1(c.x70.x) != sign1(s)) {
    s = 0.0f;
    c.sliding = 0;
  }
  c.x70.x = s;
}

// Mech3.exe 1.2 @ 0x004a2c90
float vehicle_side_force(BasicVehicle& v) {
  const float dt = hk().frame_dt();
  if (!(double{dt} >= 1e-07)) return 0.0f;  // a NaN too
  VehicleControls& c = *v.controls;
  const float f = v.transform[1] * -60.0f +
                  -c.x70.z * (v.forward.x * c.x314.z - v.forward.z * c.x314.x) * hk().frame_inv_dt();
  if (c.x70.x == 0.0f || std::isnan(c.x70.x)) {  // the exe's equal test passes a NaN
    if (c.motion.friction[0] < std::fabs(f)) {
      c.sliding = 1;
      return f - sign1(f) * c.motion.friction[0];
    }
    return 0.0f;
  }
  return f - sign1(c.x70.x) * c.motion.friction[1];
}

}  // namespace mech3
