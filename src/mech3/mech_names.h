#pragma once
#include <cstdint>

// Mech part names and ids (docs/mechdef-parts.md). Original file unknown:
// between mech_waynet.cpp and mission.cpp.
namespace mech3 {

int32_t part_id(const char* name);   // 0x00484e40, -1 when unknown
const char* part_name(int32_t id);   // 0x00484cc0, "none" when unknown

}  // namespace mech3
