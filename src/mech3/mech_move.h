#pragma once

#include "mech3/mech.h"

// Movement and physics (docs/movement.md). Original file: mech3\mech.cpp
// (the gait, turning and jump jets sit between its string and mech_anim.cpp).
namespace mech3 {

// The calls movement makes into code not ported yet. Unset ones do nothing.
struct MoveCalls {
  void (*move)(Mech&, float dt, MechAnim* anim, float speed) = nullptr;  // test hook; unset runs Mech::move
  void (*foot_ground)(Foot&) = nullptr;            // 0x00462a10(0): a foot's ground update
  void (*gait_pose)(Mech&, MechAnim&, float blend) = nullptr;  // 0x00417bbc..: the gait's pose on the parts
  void (*crouch)(Mech&, int on) = nullptr;                              // 0x0041bda0
  void (*wading_effect)(int on) = nullptr;                              // 0x00433930 on [0x00608620]
  bool (*skis_update)(Mech&, float dt) = nullptr;  // test hook; unset runs Skis::update
  Sound (*pilot_sound)(Mech&, int id, float volume, float pitch) = nullptr;  // 0x00432990 at the mech
  Sound (*fade_sound)(Sound s) = nullptr;          // 0x00571d20(s, 1, 0), returns the handle to keep (0)
  void (*stop_effect)(Handle effect) = nullptr;    // 0x00506ca0
  Handle (*pilot_effect)(Mech&, int slot, Handle node) = nullptr;  // 0x00432830 on +0x684, at rest
  void (*ff_jets)(int on) = nullptr;               // 0x00433870 on [0x00608620]
  void (*ff_landing)(float jolt) = nullptr;        // 0x00433800 on [0x00608620]
  void (*jet_pose)(Mech&) = nullptr;               // 0x004177e7..0x004179fd: the jump pose on the parts
  void (*landing_dust)(Mech&) = nullptr;           // 0x004164f3..0x0041660b: a ray down, dust or splash
  void (*landing_pose)(Mech&) = nullptr;           // 0x0041660b: landf's Reset and root (+0x494)
  void (*controls_landed)(Mech&) = nullptr;        // controls vt +0xac (0x005ac5c0, nothing, in the base)
  void (*weapons_enable)(Mech&, int on) = nullptr; // 0x004b0bf0 on +0xd4
  void (*fall_camera)(Mech&) = nullptr;            // 0x00413af0
  void (*restore_camera)(Mech&) = nullptr;         // 0x00413b70
  void (*action_dust)(Mech&) = nullptr;            // 0x00416d3a..0x00416e91: landf/landb's ray, dust or splash
  void (*action_pose)(Mech&, MechAction&, const Vec3& hip, const float q[4]) = nullptr;  // 0x00416fb8..0x00417293
};
MoveCalls& move_calls();

}  // namespace mech3
