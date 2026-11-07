#pragma once
#include "mech3/cloud_band.h"
#include <cstdint>

#include "mech3/mech.h"

// A mech's level of detail by its distance from the player (original file
// mech3\mech.cpp, 0x0041e810..0x0041eb1c; docs/movement.md, Level of
// detail): the detail bits (+0x688) that gate its sounds and finer motion,
// and its full or low-detail model (+0x690, +0x694 / +0x698).
namespace mech3 {

// What it calls outside the mech code; unset ones do nothing (the models
// stay as they are, no target counts).
struct LodCalls {
  // 0x004e6ff0 on the player's target (+0x6c): the mech it is on, if any.
  Mech* (*player_target)(Mech& player) = nullptr;
  int (*controls_answer)(Mech& m) = nullptr;           // 0x004aee40; unset 1
  void (*remove_child)(Handle parent, Handle child) = nullptr;  // 0x004ebb40
  void (*add_child)(Handle parent, Handle child) = nullptr;     // 0x004eb9b0
  Handle (*find)(Handle node, const char* name) = nullptr;      // 0x004f9d60
  void (*node_set_active)(Handle node, int32_t on) = nullptr;   // 0x004eb170
  void (*node_set_matrix)(Handle node, const float* m) = nullptr;  // 0x004fc300
  void (*set_node)(MechPart& p, Handle node) = nullptr;         // vt+0x0c (0x00460190)
  void (*stop_effect)(Handle effect) = nullptr;                 // 0x00506ca0
  Handle (*jets_effect)(Mech& m) = nullptr;  // 0x00432830(1, 0, zero, zero) on +0x684
  // Pirate's Moon's:
  const float* (*camera_axis)() = nullptr;     // [0x0061874c]'s camera, its data +0x74
  bool (*mission_lights)() = nullptr;          // [0x0062da38], the mission's ai_headlights
  int32_t (*light_map_option)() = nullptr;     // [0x008ccc64]
  void (*light_turn)(Mech& m, bool on) = nullptr;     // 0x00465da0: +0x3ac, its headlight if any
  void (*light_lod)(Mech& m, float d2) = nullptr;     // 0x00465e00
  void (*switch_model)(Mech& m, int32_t lod) = nullptr;  // unset, lod_switch; the tests'
};
LodCalls& lod_calls();

struct CloudCalls {
  CloudBand (*band)() = nullptr;
  float (*height)(Mech& m) = nullptr;     // the node's world y, brought up to date (0x00538470..0x004fefa0)
  bool (*viewed)(Mech& m) = nullptr;      // [0x0061874c]+0x64
  void (*camera)(int32_t id) = nullptr;   // 0x00408a70
  void (*prep)(Mech& m) = nullptr;        // 0x0041de60
  void (*kill)(Mech& m) = nullptr;        // vt+0x78
};
CloudCalls& cloud_calls();
void cloud_kill(Mech& m);  // 0x004b2b40, vt+0x16c

// 0x0041e810's tail (vt+0xc8, the sensors' per-vehicle call): the frame's
// budget of full-detail mechs, 0 while the player's throttle is 0, else 1
// (the limit [0x005e2034] is 1).
void lod_budget(Mech& player);
int32_t& lod_spent();  // [0x00605634]; Pirate's Moon [0x006293a0]
int32_t& lod_lights_spent();  // Pirate's Moon [0x006293a4]
// Pirate's Moon's bands and bits, set with the mechs (0x00414430, its first and last parts): by the ObjectLOD
// option, 2 being the low table, and the camera's far clip.
struct LodTable {
  bool low = false;                 // [0x006293a8]
  float near_sq = 5625.0f;          // [0x006040c8]
  float mid_sq = 62500.0f;          // [0x006040cc]
  float far_sq = 640000.0f;         // [0x006040d0]
  uint32_t near_bits = 0xfffffffeu; // [0x006040d4]
  uint32_t mid_bits = 0x1c;         // [0x006040d8]
  uint32_t far_bits = 0x10;         // [0x006040dc]
  uint32_t player_bits = 0xffffffffu;  // [0x006040e0]
  int32_t sound_limit = 2;          // [0x006040b4]
};
LodTable& lod_table();
bool lod_faces_camera(Mech& m);   // 0x0041f460
void lod_setup(int32_t object_lod, float far_clip);
// 0x0041e870 (vt+0xd0, after the sensors' frame): the detail bits by the
// player contact's squared distance (+0x98 +0x0c) and the model switch;
// returns the controls' answer (0x004aee40). Nothing for the player's own.
int lod_update(Mech& m);
// 0x0041e9f0: model `lod` (0 full, 1 low), unless it is already, missing,
// or a weapon is firing; every part follows (vt+0x5c) and the jets' node is
// found again ("butt").
void lod_switch(Mech& m, int32_t lod);

}  // namespace mech3
