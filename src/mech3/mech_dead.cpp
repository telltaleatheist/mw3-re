#include "mech3/mech_dead.h"

#include <cmath>
#include <cstring>

#include "gamez/zutil/zfix.h"

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

// Pirate's Moon 1.0 @ 0x00433020. Off the list: one still active (the exe's
// own +0x08 test), an instant action wreck at 180 s, or the network's answer
// for the dead player. The exe works out an opacity to hand dead_fade, which
// ignores it (0x00432e80 recomputes): left out, with the 0.25 cap on it. So a
// wreck with +0x658 set only starts fading from the first frame.
bool dead_step(Mech& m, float dt) {
  bool gone = m.active;
  const float t = world().time - m.dead_time;
  if (g_calls.update) g_calls.update(m, dt);  // 0x004332e0
  else m.dead_update(dt);
  switch (g_calls.game_type ? g_calls.game_type() : 0) {
    case 1:
    case 5:
      dead_cloak(m, dt);  // 0x0041d730
      break;
    case 2:
      if (kIaFade < t || m.x634 != 0) dead_fade(m, true);
      if (kIaGone < t) {
        dead_fade(m, false);
        dead_hide(m, false);
        return true;
      }
      break;
    case 3:
      if (g_calls.net_update) g_calls.net_update(m, dt);
      if (g_calls.net_fade && g_calls.net_fade() && kNetFade < t) dead_fade(m, true);
      if (&m == world().mission_player && kNetGone < t) {
        dead_fade(m, false);
        return g_calls.respawn ? g_calls.respawn(m) : gone;
      }
      break;
    default:
      break;
  }
  return gone;
}

// Pirate's Moon 1.0 @ 0x0041d730: a campaign wreck with +0x658 set thins to
// 0.25 by 0.1 a frame; with it clear it comes back to 1, opacity off there.
void dead_cloak(Mech& m, float dt) {
  // FIX (B): 0.1 a frame (at 33 ms) with per_second.
  const float step = zfix::options().per_second ? 0.1f * zfix::frame_steps(dt) : 0.1f;  // 0x0060416c
  auto clamp01 = [](float v) { return v > 1.0f ? 1.0f : v < 0.0f ? 0.0f : v; };
  const bool on = m.x634 != 0;
  if (on && 0.25f < m.x63c) {  // 0x00604170
    m.x63c = clamp01(m.x63c - step);
    if (g_calls.set_opacity) g_calls.set_opacity(m.node, m.x63c);
    if (g_calls.use_opacity) g_calls.use_opacity(m.node, 1);
    return;
  }
  if (on) {
    if (g_calls.use_opacity) g_calls.use_opacity(m.node, 1);
    return;
  }
  if (!(m.x63c < 1.0f)) return;
  m.x63c = clamp01(step + m.x63c);
  if (g_calls.set_opacity) g_calls.set_opacity(m.node, m.x63c);
  if (m.x63c == 1.0f && g_calls.use_opacity) g_calls.use_opacity(m.node, 0);
}

// Pirate's Moon 1.0 @ 0x00432e80: the opacity runs from 1 to 0 over the last
// stretch (instant action: from 170 s to 180 s); off, it is 1 and unused. PM
// also hides it from the flag-8 queries once half gone.
void dead_fade(Mech& m, bool fading) {
  if (!m.node) return;
  if (fading && !m.active) {
    const float t = world().time - m.dead_time;
    float v = networked() ? (kNetGone - t) / kNetFade : (kIaGone - t) / kIaFade;
    if (v > 1.0f) v = 1.0f;
    else if (v < 0.0f) v = 0.0f;
    if (g_calls.use_opacity) g_calls.use_opacity(m.node, 1);
    if (g_calls.set_opacity) g_calls.set_opacity(m.node, v);
    if (g_calls.set_flag8) g_calls.set_flag8(m.node, v > 0.5f ? 1 : 0);
    return;
  }
  if (g_calls.set_opacity) g_calls.set_opacity(m.node, 1.0f);
  if (g_calls.use_opacity) g_calls.use_opacity(m.node, 0);
  if (g_calls.set_flag8) g_calls.set_flag8(m.node, m.active ? 0 : 1);
}

// Pirate's Moon 1.0 @ 0x00432fb0 with 0x0040ce90. Hidden, in instant action its kills go to the
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

// Pirate's Moon 1.0 @ 0x004332e0: a dead mech's frame. No impulse; while
// still, its speeds forward and sideways run down (x(1 - 3 dt), zero below
// 0.05; the frame clock caps dt at 1/8 s, 0x00575a00, so the factor stays
// positive), it stops turning, follows the ground (the move predicted there
// is taken whole, nothing to collide with), and its flown parts fade.
void Mech::dead_update(float dt) {
  impulse = {0, 0, 0};  // 0x0066f5e0
  if (active) return;
  constexpr float kDecay = 3.0f;  // 0x006050f0
  vec_3c.x = -vec_3c.x * kDecay * dt + vec_3c.x;
  if (std::fabs(-vec_3c.x) < 0.05) vec_3c.x = 0.0f;
  vec_3c.z = -vec_3c.z * kDecay * dt + vec_3c.z;
  if (std::fabs(-vec_3c.z) < 0.05) vec_3c.z = 0.0f;
  vec_58 = {0, 0, 0};
  if (g_calls.ground_step) g_calls.ground_step(*this, dt);
  else ground_step(dt);  // 0x00416d70
  std::memcpy(orientation, pending_q, sizeof orientation);
  velocity = pending_velocity;
  std::memcpy(transform, pending_m, sizeof pending_m);
  if (g_calls.entity_moved) g_calls.entity_moved(*this);
  else entity_moved();  // 0x005cb240
  if (g_calls.fade_debris) g_calls.fade_debris(*this, dt);
  else fade_debris(dt);  // 0x00433500
}

}  // namespace mech3
