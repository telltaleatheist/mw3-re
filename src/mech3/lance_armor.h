#pragma once
#include <cstdint>

namespace mech3 {

// A vehicle as the armor display reads it (0x0047eba0, 0x0047ef10,
// 0x0047b260): its type (+0x684 +0xc), its loadout's armor (+0x3ec +0x38,
// eleven) and its parts' armor left (vt+0x44 of 0x0041d510's part) in the
// display's bar order (torso sections 0..5; head; the arms; the legs).
struct LanceArmor {
  int32_t type = -1;
  int32_t armor[11] = {};
  float left[11] = {};
};

}  // namespace mech3
