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
// Tests only: the player's HUD as it stands (hud.md sections 12, 13): the
// manager's weapons, the shown weapon lines, those named by their weapon's
// DESC, the bracket shown, the lock state.
struct HudReport {
  int weapons = 0, lines = 0, named = 0, bracketed = 0, lock = 0;
  float lock_timer = 0.0f;
};
bool program_hud_report(HudReport* r);
// Tests only: the first group that locks selected and the player's target
// set to its contact on `mech`; the group, -1 for none.
int program_hud_lock_on(void* mech, bool* target_set);
// Tests only: the reticle counted over TARGET's box, so the lock can run
// before TARGET's tracking (0x00440b00, 0x00440a30) is ported.
void program_hud_force_over(bool on);
void program_hud_draw3d(float dt);  // the view target's vt+0xe4, 0x0041cd90: the HUD's Direct3D pass
bool program_hud_aim(int32_t reticle[2], float point[3], bool* hit);
// For the tests: the player's controls' look (-1..1 of the display's half
// size; PlayerControls +0x140), as the mouse would set it.
bool program_hud_look(float x, float y);

}  // namespace mech3
