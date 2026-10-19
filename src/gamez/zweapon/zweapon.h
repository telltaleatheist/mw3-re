#pragma once
#include <cstdint>

#include "gamez/zweapon/zwep_init.h"

// zWeapon's run time: the code from 0x00581230 to zwep_init.c (0x00583880)
// and after it to 0x00586b70 (docs/weapons.md, "zWeapon run time"). The file
// names are not in the exe; this is named after the module. Only the setters
// the game half calls before a shot are ported so far.
namespace zwep {

// What the game half hands the next shot or beam (globals 0x009a0ee0..,
// 0x009a17cc..; reset to these values by the startup 0x00583750 and by every
// spawn, docs/weapons.md "Fire modifiers").
struct FireState {
  int32_t* lock_flag = nullptr;        // 0x009a0ee0  the firing weapon's lock flag
  Handle* lock_target = nullptr;       // 0x009a0ee4  and its target
  int32_t beam_ground_fx = 1;          // 0x009a17cc  0: the beam plays no ground impact
  float damage_scale = 1;              // 0x009a17d0
  float beam_range_scale = 1;          // 0x009a17d4  0.5 under water
  float track_scale[2] = {1, 1};       // 0x009a17d8, 0x009a17dc  FCS: 2, 2 (read by nothing)
  int32_t terrain_follow = 0;          // 0x009a17e0  FCS: 1
};
FireState& fire_state();

void set_lock(int32_t* flag, Handle* target);                       // 0x00581f60
bool set_fire_sound(WeaponDef* def, int32_t choice);                 // 0x00581f70
bool set_fire_animation(WeaponDef* def, int32_t choice);             // 0x00581f90
void attach_fire(WeaponDef* def, Handle node);                       // 0x00581fb0
void attach_fire(WeaponDef* def, Handle node, const void* offset);   // 0x00581fd0

}  // namespace zwep
