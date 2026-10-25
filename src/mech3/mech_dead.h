#pragma once
#include <cstdint>
#include <list>

#include "mech3/mech.h"

// The dead mechs (original file mech3\mech.cpp, 0x0041e060..0x0041e3f0, the
// list object 0x00605510; docs/damage.md, After death): a mech's update adds
// it once it is dead and still (0x0041e350); each frame the list's tick
// (0x0041e3f0) gives each its slide and ground (0x00419ef0). In instant
// action a wreck fades out from 170 s and is taken away at 180 s, its kills
// counted into the match; in multiplayer the network's rules apply.
namespace mech3 {

struct DeadCalls {
  bool (*networked)() = nullptr;                          // 0x00479480 on [0x0060861c]
  bool (*instant_action)() = nullptr;                     // 0x00479460
  void (*net_update)(Mech& m, float dt) = nullptr;        // vt+0x158 (0x0046ef70)
  bool (*net_fade)() = nullptr;                           // the network game's +0x8c
  bool (*respawn)(Mech& player) = nullptr;                // 0x0046dde0 on the pilot's game, its answer
  void (*node_set_active)(Handle node, int32_t on) = nullptr;  // 0x004eb170
  void (*use_opacity)(Handle node, int32_t on) = nullptr;      // 0x004fbb90
  void (*set_opacity)(Handle node, float v) = nullptr;         // 0x004fba20
  void (*ia_tally)(int32_t field, int32_t kills) = nullptr;   // the instant action game's +0x4c / +0x50 / +0x54
  void (*sensors_forget)(Mech& m) = nullptr;              // 0x004aa6b0
  void (*free)(Mech& m) = nullptr;                        // vt+0 (1): mech_delete
  // Test hooks; unset run the port: 0x00419ef0, and its 0x004162c0,
  // 0x005a8000, 0x00413cb0.
  void (*update)(Mech& m, float dt) = nullptr;
  void (*ground_step)(Mech& m, float dt) = nullptr;
  void (*entity_step)(Mech& m, float dt) = nullptr;
  void (*fade_debris)(Mech& m, float dt) = nullptr;
};
DeadCalls& dead_calls();

std::list<Mech*>& dead_mechs();       // 0x00605510 +4, count +8
void dead_add(Mech& m);               // 0x0041e350: once, when it is still (+0x08 off); +0x6b4 the time
void dead_tick(float dt);             // 0x0041e3f0: each one stepped, the finished taken off
bool dead_step(Mech& m, float dt);    // 0x0041e220: true when it is to leave the list
void dead_fade(Mech& m, bool fading); // 0x0041e060
void dead_hide(Mech& m, bool show);   // 0x0041e140: hidden; in instant action counted and deleted

}  // namespace mech3
