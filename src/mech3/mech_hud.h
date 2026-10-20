#pragma once
#include <cstdint>

// The mech's side of the HUD (mech.cpp 0x0041cd40, 0x0041ce60, 0x0041bd80;
// mech_data.cpp 0x00430aa0 / 0x00430ad0): the reticle placed from the
// player's look each frame, then the weapons aimed at its world point
// (docs/hud.md section 10).
namespace mech3 {

struct Mech;
struct Pilot;
class Hud;

struct MechHudCalls {
  bool (*is_view_target)(const Mech&) = nullptr;  // the player view's target ([0x005f4c8c] +0x64)
  void (*zoom_camera)(Mech&, int32_t) = nullptr;  // the mech's vt+0xfc (0x14: the zoom camera, not ported)
  void (*cockpit)(Pilot&) = nullptr;              // 0x00430ad0's second half: the cockpit's node at +0x228
};
MechHudCalls& mech_hud_calls();

Hud* pilot_hud(Pilot& p);                 // 0x00430aa0: made when first asked (0x00430ad0)
void player_frame(Mech& m);               // 0x0041ce60
void hud_handoff(Mech& m, float dt);      // 0x0041cd40 (the mech's vt+0xe0)
bool aim_at_point(Mech& m, const float p[3], bool check);  // 0x0041bd80
// 0x0041d940: where to aim the selected group's current weapon so its shot
// meets the target (TARGET's lead circle, hud.md section 14); false
// without a target or a weapon, or when the target outruns the shot.
bool lead_point(Mech* m, float out[3]);

}  // namespace mech3
