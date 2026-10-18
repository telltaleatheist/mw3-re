#pragma once
#include <cstdint>

// The game's music calls. Original file unknown: both functions sit between
// veh_basic.cpp and weapon.cpp.
namespace mech3_music {

void play(int track, int mode);                  // 0x004ac290
void set_volume(uint16_t left, uint16_t right);  // 0x004ac230
bool enabled();                                  // 0x005e855c

}  // namespace mech3_music
