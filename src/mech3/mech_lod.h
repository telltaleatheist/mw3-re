#pragma once
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
};
LodCalls& lod_calls();

// 0x0041e810's tail (vt+0xc8, the sensors' per-vehicle call): the frame's
// budget of full-detail mechs, 0 while the player's throttle is 0, else 1
// (the limit [0x005e2034] is 1).
void lod_budget(Mech& player);
int32_t& lod_spent();  // [0x00605634]
// 0x0041e870 (vt+0xd0, after the sensors' frame): the detail bits by the
// player contact's squared distance (+0x98 +0x0c) and the model switch;
// returns the controls' answer (0x004aee40). Nothing for the player's own.
int lod_update(Mech& m);
// 0x0041e9f0: model `lod` (0 full, 1 low), unless it is already, missing,
// or a weapon is firing; every part follows (vt+0x5c) and the jets' node is
// found again ("butt").
void lod_switch(Mech& m, int32_t lod);

}  // namespace mech3
