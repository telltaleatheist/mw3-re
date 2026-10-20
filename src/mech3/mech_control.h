#pragma once
#include <cstdint>

// The shared part of a mech's controls (docs/controls.md): the commanded
// throttle, turn and jump, and their smoothing toward the targets
// (docs/movement.md, Throttle and turn). Original file unknown: the code sits
// between mech_config and mech_data (named here mech_control after the
// alphabetical link order). The player, AI and network classes derive from it.
namespace mech3 {

struct Mech;

struct MechControls {
  uint32_t flags = 0;          // +0x08  bit 2 doubles the turn rate
  int32_t view = 0;            // +0x0c  the player's view state 0..3 (0x00426780)
  float throttle = 0;          // +0x10  -1..1, smoothed toward throttle_target
  float turn = 0;              // +0x14  smoothed toward turn_target
  float pitch = 0;             // +0x18
  float twist = 0;             // +0x1c
  float jump = 0;              // +0x20  0..1, smoothed toward jump_target
  float jj_strafe = 0;         // +0x24
  float jj_forward = 0;        // +0x28
  float axis_2c = 0, axis_30 = 0;  // +0x2c, +0x30  free-look pitch and yaw (the torso's look inputs)
  uint8_t fire = 0;            // +0x34  handed to the weapons each frame (IControls +0x10)
  int32_t wrapped = 0;         // +0x38  set: an order wrapper to take off (primary vt +0x04)
  float throttle_target = 0;   // +0x44  -1..1
  float turn_target = 0;       // +0x48
  float jump_target = 0;       // +0x4c
  bool reverse = false;        // +0x50  the last non-zero throttle command was < 0
  bool moves[2] = {true, true};  // +0x58, +0x59  either set: the gait or the actions run
  Mech* owner = nullptr;       // +0x70  (IControls +0x10)

  int update(float dt);          // 0x00426960 (IControls +0x0c, the base and network class)
  void set_throttle(float v);    // 0x00426b60 (IControls +0x20)
  void zero();                   // 0x00426c80
};

}  // namespace mech3
