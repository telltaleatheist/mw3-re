// Probe for tests/differential/test_pm_controls_zero.py: Pirate's Moon's controls vt+0x1c, 0x00428940 (MechControls::
// zero_held) and the player's 0x004289d0 (PlayerControls::zero_held: the look's too), on controls whose inputs the test
// fills: the 16 floats at PM's +0x10..+0x4c and, for the player, the look at +0x158..+0x164.
#include <cstdint>

#include "mech3/mech_control.h"

extern "C" {
// in/out: 20 floats, PM's +0x10, +0x14, ..., +0x4c (16) then +0x158..+0x164 (4). player: the player's version.
void probe_pm_controls_zero(int player, const float* in, float* out) {
  mech3::PlayerControls p;
  mech3::MechControls& c = p;
  float* base[16] = {&c.throttle, &c.turn, &c.pitch, &c.twist, &c.jump, &c.jj_strafe, &c.jj_forward, &c.axis_2c, &c.axis_30,
                     nullptr, nullptr, nullptr, &c.joy_throttle, &c.throttle_target, &c.turn_target, &c.jump_target};
  float* look[4] = {&p.look_dx, &p.look_dy, &p.look_x, &p.look_y};
  for (int i = 0; i < 16; ++i)
    if (base[i]) *base[i] = in[i];
  for (int i = 0; i < 4; ++i) *look[i] = in[16 + i];
  if (player) p.zero_held();
  else c.MechControls::zero_held();
  for (int i = 0; i < 16; ++i) out[i] = base[i] ? *base[i] : in[i];
  for (int i = 0; i < 4; ++i) out[16 + i] = *look[i];
}
}  // extern "C"
