#include "mech3/mech_hud.h"

#include <cstring>

#include "mech3/hud.h"
#include "mech3/mech.h"
#include "mech3/mech_control.h"
#include "gamez/ztwod/twod.h"

namespace mech3 {

namespace {
MechHudCalls g_calls;
ztwod::Image* g_blips[3];  // [0x00605618]

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

// Mech3.exe 1.2 @ 0x0041d940: the intercept of a shot at the weapon's speed
// (beams and instant weapons: FLT_MAX) from the mech with the target's
// relative velocity, from the mech's own aim point (+0xec while +0xf8 is
// set) or the target's position. The square root is the bit trick
// (bits / 2 + 0x1fc00000). FIX (bug): with no group selected the original
// reads the group's weapon through a null pointer; here there is none.
bool lead_point(Mech* m, float out[3]) {
  HudHooks& h = hud_hooks();
  void* wm = h.mech_weapons(m);
  const int32_t gi = h.wm_group_index(wm);
  void* g = gi == -1 ? nullptr : h.wm_group(wm, gi);
  void* w = g ? h.group_current_weapon(g) : nullptr;
  void* contact = h.slot_contact(h.mech_target_slot(m));
  void* t = contact ? h.contact_vehicle(contact) : nullptr;
  if (!t || !w) return false;
  float p[3];
  if (!h.mech_aim_point(m, p)) h.vehicle_position(t, p);  // 0x005a8630
  const uint32_t flags = h.weapon_def_flags(w);
  const float speed = (flags & 0x800) || (flags & 2) ? 3.4028235e+38f : h.weapon_speed(w);  // [0x005bce58]
  const float inv = 1.0f / speed;  // [0x005bcd44]
  float o[3];
  h.mech_position(m, o);  // 0x005a85e0
  float d[3] = {p[0] - o[0], p[1] - o[1], p[2] - o[2]};
  d[0] = inv * d[0], d[1] = inv * d[1], d[2] = inv * d[2];
  float vo[3], vt[3];
  h.mech_velocity(m, vo);  // vt+0xc
  h.vehicle_velocity(t, vt);
  const float dv[3] = {vt[0] - vo[0], vt[1] - vo[1], vt[2] - vo[2]};
  const float v[3] = {inv * dv[0], inv * dv[1], inv * dv[2]};
  auto dot = [](const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
  const float c = 1.0f - dot(v, v);
  if (!(c > 0.0f)) return false;  // [0x005bcd3c]
  const float b = dot(v, d);
  const float a = dot(d, d);
  const float s = a * c + b * b;
  int32_t bits;
  std::memcpy(&bits, &s, 4);
  bits = (bits >> 1) + 0x1fc00000;
  float root;
  std::memcpy(&root, &bits, 4);
  const float time = (root + b) / c;
  out[0] = p[0] + time * dv[0];
  out[1] = p[1] + time * dv[1];
  out[2] = p[2] + time * dv[2];
  return true;
}

// Mech3.exe 1.2 @ 0x0041bd80
bool aim_at_point(Mech& m, const float p[3], bool check) {
  uint8_t torso[12];
  return m.aim_at(reinterpret_cast<const Vec3*>(p), torso, check);
}

// Mech3.exe 1.2 @ 0x00413ecb (part of 0x00413e60, the mechs' set-up)
void mech_radar_blips_create() {
  g_blips[0] = new ztwod::Image("hud_mech_ally_sm", 0, 0, 1);  // [0x005e2094]
  g_blips[1] = new ztwod::Image("hud_mech_enemy_sm", 0, 0, 1);
  g_blips[2] = new ztwod::Image("hud_mech_other_sm", 0, 0, 1);
}

// Mech3.exe 1.2 @ 0x0041406a (part of 0x00414000)
void mech_radar_blips_destroy() {
  for (ztwod::Image*& b : g_blips) {
    delete b;
    b = nullptr;
  }
}

// Mech3.exe 1.2 @ 0x0041ec60 (the Mech's vt+0x3c)
ztwod::Image* mech_radar_blip(int32_t kind) { return g_blips[kind]; }

}  // namespace mech3
