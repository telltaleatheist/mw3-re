#include "mech3/mech_dead.h"

#include <cmath>

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {
DeadCalls g_calls;
std::list<Mech*> g_dead;
constexpr float kNetGone = 7.1f;    // 0x005e2024: the network's dead player respawns
constexpr float kNetFade = 1.0f;    // 0x005e2028
constexpr float kIaGone = 180.0f;   // 0x005e202c
constexpr float kIaFade = 10.0f;    // 0x005e2030
bool networked() { return g_calls.networked && g_calls.networked(); }
bool instant_action() { return g_calls.instant_action && g_calls.instant_action(); }
}  // namespace

DeadCalls& dead_calls() { return g_calls; }
std::list<Mech*>& dead_mechs() { return g_dead; }

// Mech3.exe 1.2 @ 0x0041e350
void dead_add(Mech& m) {
  for (Mech* d : g_dead)
    if (d == &m) return;
  if (m.active) return;
  m.dead_time = world().time;
  g_dead.push_back(&m);
}

// Mech3.exe 1.2 @ 0x0041e3f0 (the remove_if over 0x0041e220)
void dead_tick(float dt) {
  for (auto it = g_dead.begin(); it != g_dead.end();) {
    Mech* m = *it;
    it = dead_step(*m, dt) ? g_dead.erase(it) : std::next(it);
  }
}

// Mech3.exe 1.2 @ 0x0041e220. Off the list: one still active (the exe's
// own +0x08 test), an instant action wreck at 180 s, or the network's
// answer for the dead player.
bool dead_step(Mech& m, float dt) {
  bool gone = m.active;
  if (g_calls.update) g_calls.update(m, dt);  // 0x00419ef0
  else m.dead_update(dt);
  if (!networked()) {
    if (instant_action()) {
      const float t = world().time - m.dead_time;
      if (kIaFade < t) dead_fade(m, true);
      if (kIaGone < t) {
        dead_fade(m, false);
        dead_hide(m, false);
        gone = true;
      }
    }
    return gone;
  }
  if (g_calls.net_update) g_calls.net_update(m, dt);
  const float t = world().time - m.dead_time;
  if (g_calls.net_fade && g_calls.net_fade() && kNetFade < t) dead_fade(m, true);
  if (&m == world().mission_player && kNetGone < t) {
    dead_fade(m, false);
    return g_calls.respawn ? g_calls.respawn(m) : gone;
  }
  return gone;
}

// Mech3.exe 1.2 @ 0x0041e060: the opacity runs from 1 to 0 over the last
// stretch (instant action: from 170 s to 180 s); off, it is 1 and unused.
void dead_fade(Mech& m, bool fading) {
  if (fading && !m.active) {
    const float t = world().time - m.dead_time;
    float v = networked() ? (kNetGone - t) / kNetFade : (kIaGone - t) / kIaFade;
    if (v > 1.0f) v = 1.0f;
    else if (v < 0.0f) v = 0.0f;
    if (g_calls.use_opacity) g_calls.use_opacity(m.node, 1);
    if (g_calls.set_opacity) g_calls.set_opacity(m.node, v);
    return;
  }
  if (g_calls.set_opacity) g_calls.set_opacity(m.node, 1.0f);
  if (g_calls.use_opacity) g_calls.use_opacity(m.node, 0);
}

// Mech3.exe 1.2 @ 0x0041e140. Hidden, in instant action its kills go to the
// match (the player's +0x4c, an ally's +0x50, the others' +0x54), the
// sensors forget it and it is deleted.
void dead_hide(Mech& m, bool show) {
  if (show) {
    if (g_calls.node_set_active) g_calls.node_set_active(m.node, 1);
    return;
  }
  if (g_calls.node_set_active) g_calls.node_set_active(m.node, 0);
  if (instant_action()) {
    Mech* player = world().mission_player;
    const int32_t kills = static_cast<int32_t>(m.x3f4[0]);
    if (m.pilot && m.pilot->roster != -1 && kills != 0) {
      // `player &&`: the exe reads the player's +0xc4 unchecked (there is
      // always one in instant action).
      const int32_t field = &m == player ? 0x4c : (player && (m.team & player->allies)) ? 0x50 : 0x54;
      if (g_calls.ia_tally) g_calls.ia_tally(field, kills);
      m.x3f4[0] = 0;
    }
  }
  if (g_calls.sensors_forget) g_calls.sensors_forget(m);  // 0x004aa6b0: every vehicle drops it
  if (g_calls.free) g_calls.free(m);
}

// Mech3.exe 1.2 @ 0x00419ef0: a dead mech's frame. No impulse; while still,
// its speeds forward and sideways run down (x(1 - 3 dt), zero below 0.05;
// the frame clock caps dt at 1/8 s, 0x00575a00, so the factor stays positive),
// it stops turning, follows the ground, moves and its flown parts fade.
void Mech::dead_update(float dt) {
  impulse = {0, 0, 0};  // 0x0063d870
  if (active) return;
  constexpr float kDecay = 3.0f;  // 0x005e1ff8
  vec_3c.x = -vec_3c.x * kDecay * dt + vec_3c.x;
  if (std::fabs(-vec_3c.x) < 0.05) vec_3c.x = 0.0f;
  vec_3c.z = -vec_3c.z * kDecay * dt + vec_3c.z;
  if (std::fabs(-vec_3c.z) < 0.05) vec_3c.z = 0.0f;
  vec_58 = {0, 0, 0};
  if (g_calls.ground_step) g_calls.ground_step(*this, dt);
  else ground_step(dt);  // 0x004162c0
  if (g_calls.entity_step) g_calls.entity_step(*this, dt);
  else entity_step(dt);  // 0x005a8000
  if (g_calls.fade_debris) g_calls.fade_debris(*this, dt);
  else fade_debris(dt);  // 0x00413cb0
}

}  // namespace mech3
