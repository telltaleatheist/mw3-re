#pragma once
// The two sums of 0x004814e0 (app_states.cpp), shared by its handle version
// and the program's typed upkeep (app_mech.cpp); docs/render-shadows.md.
#include <cstdint>

namespace mech3 {

// The fade by the squared distance from the player: 1 within 50 m, 0 from 100 m.
float shadow_fade(float d2);
// The grey from the sun's ambient and diffuse (both times the clouds) and its
// direction's y: the share of the light the shadow takes away.
uint8_t shadow_grey(float ambient, float diffuse, float dir_y, float fade);

}  // namespace mech3
