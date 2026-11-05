#include "mech3/mech_lod.h"

#include "mech3/mech_parts.h"
#include "mech3/weapon.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {
LodCalls g_calls;
LodTable g_table;
int32_t g_spent = 0;                // [0x006293a0]
int32_t g_lights = 0;               // [0x006293a4]
constexpr int32_t kLightLimit = 1;  // [0x006040b8]
constexpr float kFacing = 0.173648f;  // [0x00604184]: cos 80 degrees
}  // namespace

LodCalls& lod_calls() { return g_calls; }
int32_t& lod_spent() { return g_spent; }
LodTable& lod_table() { return g_table; }
int32_t& lod_lights_spent() { return g_lights; }

// Pirate's Moon 1.0 @ 0x0041f3d5. An AI headlight needs the mission's ai_headlights and the light map option not
// off; the exe also asks the card for two texture stages, which the GPU path always has.
void lod_budget(Mech& player) {
  const float t = heat_calls().throttle ? heat_calls().throttle(player) : 0.0f;  // +0x38c vt+0x1c
  g_spent = !(t < 0.0f || t > 0.0f) ? 0 : 1;  // fcomp 0.0, C3: equal (or NaN) leaves the budget
  const bool lit = g_calls.mission_lights && g_calls.mission_lights() &&
                   !(g_calls.light_map_option && g_calls.light_map_option() == -1);
  g_lights = lit ? 0 : kLightLimit;
}

// Pirate's Moon 1.0 @ 0x00414430 (its option and bands)
void lod_setup(int32_t object_lod, float far_clip) {
  LodTable& t = g_table;
  t.low = object_lod == 2;
  const float mid = 250.0f <= far_clip ? 250.0f : far_clip;
  const float far = 800.0f <= far_clip ? 800.0f : far_clip;
  t.near_sq = t.low ? 2500.0f : 5625.0f;
  t.mid_sq = t.low ? mid * mid : 62500.0f;
  t.far_sq = far * far;
  t.near_bits = t.low ? 0x14u : 0xfffffffeu;
  t.mid_bits = t.low ? 0x10u : 0x1cu;
  t.far_bits = 0x10u;
  t.player_bits = t.low ? 0x15u : 0xffffffffu;
  t.sound_limit = t.low ? 1 : 2;
}

// Pirate's Moon 1.0 @ 0x0041f4d0 (MW3's 0x0041e870, with the player's bits, the near band's bit 0 apart and the
// headlight new). Within the mid band the frame's first mechs get the sounds' bit 0 and the first enemy still
// fighting gets its headlight; beyond it the gait only, out to the far band or for the player's target. The sound
// budget is spent only while the player stands still (docs/discoveries.md). The low table also drops the finer
// bits of a mech the camera is not facing, and the gait of one powered down, and never swaps a model.
// Not ported: the never-set [0x006293a9]; vt+0x64, nonzero only on the network mech 0x005e59a8, which skips the turn.
int lod_update(Mech& m) {
  Mech* player = world().mission_player;
  const LodTable& t = g_table;
  if (&m == player) {
    m.detail = t.player_bits;
    return g_calls.controls_answer ? g_calls.controls_answer(m) : 1;
  }
  const bool targeted = player && player->target && g_calls.player_target && g_calls.player_target(*player) == &m;
  const bool may_light = !(player && (player->team & m.allies)) && m.active && m.anim_state != 10;  // 0x0041ade0
  const float d = m.player_contact ? m.player_contact->dist2 : t.far_sq;
  bool facing = true;
  if (t.low && d < t.mid_sq) {  // 0x0041f460
    m.player_contact->distance();
    const float* dir = m.player_contact->dir;
    const float* axis = g_calls.camera_axis ? g_calls.camera_axis() : nullptr;
    if (axis) facing = !(-(axis[2] * dir[2] + axis[1] * dir[1] + axis[0] * dir[0]) < kFacing);
  }
  uint32_t bits = 0;
  bool light = false;
  if (d < t.near_sq || d < t.mid_sq) {
    bits = d < t.near_sq ? t.near_bits : t.mid_bits;
    if (g_spent < t.sound_limit) bits |= 1, ++g_spent;
    if (may_light && g_lights < kLightLimit) ++g_lights, light = true;
  } else if (d < t.far_sq || targeted) {
    bits = t.far_bits;
  }
  if (!facing) bits &= ~0x1cu;
  if (t.low && (m.power_down_pending || m.powered_down)) bits &= ~0x10u;  // 0x0041da90
  m.detail = bits;
  if (!t.low) {
    const MechData& data = *m.pilot;
    const auto swap = [&m](int32_t lod) { g_calls.switch_model ? g_calls.switch_model(m, lod) : lod_switch(m, lod); };
    if (!targeted && m.x690 == 0 && data.range_sq[0] < d) swap(1);
    else if (m.x690 == 1 && (d < data.range_sq[1] || targeted)) swap(0);
  }
  if (g_calls.light_turn) g_calls.light_turn(m, light && !m.powered_down);  // 0x0041f8d0
  if (g_calls.light_lod) g_calls.light_lod(m, d);
  return g_calls.controls_answer ? g_calls.controls_answer(m) : 1;
}

// Mech3.exe 1.2 @ 0x0041e9f0
void lod_switch(Mech& m, int32_t lod) {
  Handle* models = &m.model_node;  // +0x694, +0x698
  if (static_cast<int32_t>(m.x690) == lod || !models[lod]) return;
  const auto* wm = static_cast<const WeaponManager*>(m.ammo);
  bool idle = true;
  for (uint32_t i = 0; wm && i < wm->count && idle; ++i)
    if (const Weapon* w = wm->weapons[i]) idle = !w->is_firing();
  if (!idle) return;
  const bool jets = m.jet_effect != nullptr;
  if (jets) {
    if (g_calls.stop_effect) g_calls.stop_effect(m.jet_effect);
    m.jet_effect = nullptr;
  }
  for (MechPart* p : m.parts) p->switch_lod(lod);
  if (g_calls.remove_child) g_calls.remove_child(m.node, models[m.x690]);
  if (g_calls.add_child) g_calls.add_child(m.node, models[lod]);
  m.x690 = static_cast<uint32_t>(lod);
  m.jet_node = g_calls.find ? g_calls.find(m.node, "butt") : nullptr;  // 0x005e2364
  // The exe restarts the flames with no node (a copy of the canned one), not
  // at the jet node it just found.
  if (jets && g_calls.jets_effect) m.jet_effect = g_calls.jets_effect(m);
}

// Mech3.exe 1.2 @ 0x00467290 (vt+0x5c): the part's node for model `lod`
// (+0x0c full, +0x10 low), its pose carried over; a destroyed part's copy in
// the other model is hidden instead.
void MechPart::switch_lod(int32_t lod) {
  const Handle target = lod == 0 ? found : this->lod;
  LodCalls& c = g_calls;
  if (destroyed && node != target) {
    if (target && c.node_set_active) c.node_set_active(target, 0);
    return;
  }
  if (node == target || !node || !target) return;
  if (c.node_set_matrix) c.node_set_matrix(target, rotation);
  if (c.set_node) c.set_node(*this, target);
}

// Mech3.exe 1.2 @ 0x004672f0: the toes counted in that model (+0x98, +0x9c).
void Foot::switch_lod(int32_t lod) {
  MechPart::switch_lod(lod);
  toe_count = lod == 0 ? x98 : x9c;
}

}  // namespace mech3
