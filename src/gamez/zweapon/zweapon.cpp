#include "gamez/zweapon/zweapon.h"

namespace zwep {

namespace {
FireState g_fire;
}

FireState& fire_state() { return g_fire; }

// Mech3.exe 1.2 @ 0x00581f60
void set_lock(int32_t* flag, Handle* target) {
  g_fire.lock_flag = flag;
  g_fire.lock_target = target;
}

// Mech3.exe 1.2 @ 0x00581f70: which FIRE sound the next shot plays, -1 none.
bool set_fire_sound(WeaponDef* def, int32_t choice) {
  if (!def || choice >= def->fire.sound_count) return false;
  def->fire.sound_choice = choice;
  return true;
}

// Mech3.exe 1.2 @ 0x00581f90
bool set_fire_animation(WeaponDef* def, int32_t choice) {
  if (!def || choice >= 2) return false;
  def->fire.animation_choice = choice;
  return true;
}

// Mech3.exe 1.2 @ 0x00581fb0
void attach_fire(WeaponDef* def, Handle node) {
  def->fire.attach_node = node;
  def->fire.attach_offset = nullptr;
}

// Mech3.exe 1.2 @ 0x00581fd0
void attach_fire(WeaponDef* def, Handle node, const void* offset) {
  def->fire.attach_node = node;
  def->fire.attach_offset = offset;
}

}  // namespace zwep
