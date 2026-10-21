#include "mech3/mech_lod.h"

#include "mech3/mech_parts.h"
#include "mech3/weapon.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {
LodCalls g_calls;
int32_t g_spent = 0;               // [0x00605634]
constexpr int32_t kLimit = 1;      // [0x005e2034]
constexpr float kNear = 5625.0f;   // 0x005bcd30: 75 m squared
constexpr float kMid = 62500.0f;   // 0x005bcd34: 250 m
constexpr float kFar = 640000.0f;  // 0x005bcd38: 800 m, also with no contact
}  // namespace

LodCalls& lod_calls() { return g_calls; }
int32_t& lod_spent() { return g_spent; }

// Mech3.exe 1.2 @ 0x0041e834..0x0041e855 (after the brain's 0x004aedd0).
void lod_budget(Mech& player) {
  const float t = heat_calls().throttle ? heat_calls().throttle(player) : 0.0f;  // +0x37c vt+0x1c
  g_spent = !(t < 0.0f || t > 0.0f) ? 0 : 1;  // fcomp 0.0, C3: equal (or NaN) leaves the budget
}

// Mech3.exe 1.2 @ 0x0041e870. Within 75 m the first mech in the frame's
// budget gets everything (-1), the rest 0x1c; within 250 m 0x1d with the
// budget (the sounds' bit 0 among them), else 0x1c; within 800 m, or the
// player's target, 0x10 (the gait only); beyond, 0. The budget is spent
// only while the player stands still, so an AI mech is heard only then
// (docs/discoveries.md).
int lod_update(Mech& m) {
  Mech* player = world().mission_player;
  if (&m != player) {
    const float d = m.player_contact ? m.player_contact->dist2 : kFar;
    const bool targeted = player && player->target && g_calls.player_target && g_calls.player_target(*player) == &m;
    if (d < kNear && g_spent < kLimit) {
      m.detail = 0xffffffffu;
      ++g_spent;
    } else if (d < kMid) {
      uint32_t bits = 0x1c;
      if (g_spent < kLimit) ++g_spent, bits = 0x1d;
      m.detail = bits;
    } else {
      m.detail = (d < kFar || targeted) ? 0x10u : 0u;
    }
    const MechData& data = *m.pilot;
    if (!targeted && m.x690 == 0 && data.range_sq[0] < d) {
      lod_switch(m, 1);
    } else if (m.x690 == 1 && (d < data.range_sq[1] || targeted)) {
      lod_switch(m, 0);
    }
  }
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
