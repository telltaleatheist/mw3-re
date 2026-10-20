#include "mech3/mech_hud.h"

#include "mech3/hud.h"
#include "mech3/mech.h"
#include "mech3/mech_control.h"

namespace mech3 {

namespace {
MechHudCalls g_calls;

Hud* viewed_hud(Mech& m) {
  if (!g_calls.is_view_target || !g_calls.is_view_target(m) || !m.pilot) return nullptr;
  return pilot_hud(*m.pilot);
}
}  // namespace

MechHudCalls& mech_hud_calls() { return g_calls; }

// Mech3.exe 1.2 @ 0x00430aa0 (with 0x00430ad0): only for a loaded type.
Hud* pilot_hud(Pilot& p) {
  if (!p.x20) return nullptr;
  if (!p.hud) {
    p.hud = new Hud(nullptr);
    if (g_calls.cockpit) g_calls.cockpit(p);
  }
  return static_cast<Hud*>(p.hud);
}

// Mech3.exe 1.2 @ 0x0041ce60: the reticle where the player looks; the zoom
// camera kept on while its window shows.
void player_frame(Mech& m) {
  const float* look = m.controls ? m.controls->look_position() : nullptr;
  if (!look) return;
  Hud* h = viewed_hud(m);
  if (!h) return;
  h->reticle.place(look[0], look[1]);
  if (!(h->reticle.zoom.flags & ztwod::kHidden) && h->reticle.zoom.x174 == 0 && g_calls.zoom_camera)
    g_calls.zoom_camera(m, 0x14);
}

// Mech3.exe 1.2 @ 0x0041cd40: the HUD's frame, then the weapons aimed at the
// reticle's point (not while +0x638 is set).
void hud_handoff(Mech& m, float dt) {
  Hud* h = viewed_hud(m);
  if (!h) return;
  h->update(dt);  // vt+0x00, 0x0043c820
  if (m.hud_fall == 0) aim_at_point(m, h->reticle.aim_point, true);
}

// Mech3.exe 1.2 @ 0x0041bd80
bool aim_at_point(Mech& m, const float p[3], bool check) {
  uint8_t torso[12];
  return m.aim_at(reinterpret_cast<const Vec3*>(p), torso, check);
}

}  // namespace mech3
