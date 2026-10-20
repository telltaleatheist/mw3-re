#pragma once

#include "mech3/mech.h"

// Movement and physics (docs/movement.md). Original file: mech3\mech.cpp
// (the gait, turning and jump jets sit between its string and mech_anim.cpp).
namespace mech3 {

// The calls movement makes into code not ported yet. Unset ones do nothing.
struct MoveCalls {
  void (*move)(Mech&, float dt, MechAnim* anim, float speed) = nullptr;  // test hook; unset runs Mech::move
  void (*foot_ground)(Foot&) = nullptr;            // 0x00462a10(0): a foot's ground update
  void (*gait_pose)(Mech&, MechAnim&, float blend) = nullptr;  // test hook (0x00417bbc..): unset runs Mech::pose_gait
  void (*crouch)(Mech&, int on) = nullptr;                              // 0x0041bda0
  void (*wading_effect)(int on) = nullptr;                              // 0x00433930 on [0x00608620]
  bool (*skis_update)(Mech&, float dt) = nullptr;  // test hook; unset runs Skis::update
  Sound (*pilot_sound)(Mech&, int id, float volume, float pitch) = nullptr;  // 0x00432990 at the mech
  Sound (*fade_sound)(Sound s) = nullptr;          // 0x00571d20(s, 1, 0), returns the handle to keep (0)
  void (*stop_effect)(Handle effect) = nullptr;    // 0x00506ca0
  Handle (*pilot_effect)(Mech&, int slot, Handle node) = nullptr;  // 0x00432830 on +0x684, at rest
  void (*ff_jets)(int on) = nullptr;               // 0x00433870 on [0x00608620]
  void (*ff_landing)(float jolt) = nullptr;        // 0x00433800 on [0x00608620]
  void (*jet_pose)(Mech&) = nullptr;               // test hook (0x004177e7..0x004179fd): unset runs Mech::pose_jump
  void (*landing_dust)(Mech&) = nullptr;           // 0x004164f3..0x0041660b: a ray down, dust or splash
  void (*landing_pose)(Mech&) = nullptr;           // 0x0041660b: landf's Reset and root (+0x494)
  void (*controls_landed)(Mech&) = nullptr;        // controls vt +0xac (0x005ac5c0, nothing, in the base)
  void (*weapons_enable)(Mech&, int on) = nullptr; // 0x004b0bf0 on +0xd4
  void (*fall_camera)(Mech&) = nullptr;            // 0x00413af0
  void (*restore_camera)(Mech&) = nullptr;         // 0x00413b70
  void (*action_dust)(Mech&) = nullptr;            // 0x00416d3a..0x00416e91: landf/landb's ray, dust or splash
  void (*action_pose)(Mech&, MechAction&, const Vec3& hip, const float q[4]) = nullptr;  // test hook (0x00416fb8..0x00417293): unset runs Mech::pose_action
};
MoveCalls& move_calls();

struct MechDfn;

// What Mech::Update (0x00419fc0) calls. The first group are test hooks for
// ported pieces (unset runs the port); the rest are code not ported yet,
// which does nothing when unset.
struct UpdateCalls {
  int (*controls_update)(Mech&, float dt) = nullptr;  // controls vt +0x0c (player 0x00427d60, AI 0x004e96f0, net 0x0046ef60); unset: the base 0x00426960
  void (*select_gait)(Mech&, float dt) = nullptr;
  void (*update_action)(Mech&, float dt) = nullptr;
  void (*turn)(Mech&, float dt) = nullptr;
  void (*get_up)(Mech&) = nullptr;
  void (*finish_power_down)(Mech&) = nullptr;
  void (*update_heat)(Mech&, float dt) = nullptr;
  bool (*collide)(Mech&) = nullptr;
  const MechDfn* (*dfn)(Mech&) = nullptr;            // 0x0041e9a0; unset: mech_dfn(pilot->roster)

  void (*stop_cockpit_sound)(int index) = nullptr;   // 0x0041e7a0(i, 0)
  void (*add_dead)(Mech&) = nullptr;                 // 0x0041e350 on 0x00605510
  uint32_t (*key)(int command) = nullptr;            // 0x0051d300: input state, &3 held, &4 pressed
  void (*controls_view)(MechControls&, int mode) = nullptr;  // 0x00426780
  void (*player_frame)(Mech&, float dt) = nullptr;   // 0x0041ce60: the player's HUD per frame
  void (*translate)(Handle node, float x, float y, float z) = nullptr;  // 0x004fc1b0; unset: zcls::object3d_translate
  void (*moved)(Mech&) = nullptr;                    // 0x005a85c0
  void (*landmines)(const Vec3* position) = nullptr; // 0x00413560 on 0x00604e48
  bool (*weapons_changed)(Mech&) = nullptr;          // 0x004b2da0 on +0xd4
  void (*hud_weapons)(void* screen) = nullptr;       // 0x00446240 on screen +0x6520
  void (*weapons_fire)(Mech&, uint8_t fire) = nullptr;  // 0x004b0c20 on +0xd4
  void (*weapons_update)(Mech&, float dt) = nullptr;    // 0x004b0c30 on +0xd4
  void (*equipment_update)(Mech&, float dt) = nullptr;  // 0x00419200
  void (*set_volume)(Sound, float volume) = nullptr;    // 0x0056f370
  void (*set_position)(Sound, const Vec3* at) = nullptr;  // 0x00571f30(at, 0, 0)
  MechControls* (*unwrap)(MechControls&) = nullptr;  // controls primary vt +0x04
  void (*x68c_update)(Handle, float dt) = nullptr;   // 0x004e6480
  void (*net_update)(Mech&, float dt) = nullptr;     // vtable +0x158 (0x0046ef70)
  void (*purge)(Mech&, float dt) = nullptr;          // 0x00413cb0: the list at +0x6d0
};
UpdateCalls& update_calls();

}  // namespace mech3
