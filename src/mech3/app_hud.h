#pragma once
#include <cstdint>

// The program's HUD (our addition; docs/hud.md section 10): the player's HUD
// made on its mech data when first asked, the reticle placed each frame from
// the player's look and the weapons aimed at its world point, on the ported
// engine (zClass's segment query, the camera, zMath's unproject).
namespace mech3 {

void wire_hud();                  // the hooks onto the ports; called with the mech frame's
void unwire_hud();
void program_hud_update(float dt);  // 0x0040fac0's first call: the view target's 0x0041cd40
// For the tests: the view target's reticle (x, y), its world point and
// whether the ray hit something; false without a HUD.
bool program_hud_aim(int32_t reticle[2], float point[3], bool* hit);
// For the tests: the player's controls' look (-1..1 of the display's half
// size; PlayerControls +0x140), as the mouse would set it.
bool program_hud_look(float x, float y);

}  // namespace mech3
