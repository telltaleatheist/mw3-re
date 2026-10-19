#pragma once
#include <cstdint>

// Mech, part, weapon and equipment names and ids (docs/mechdef-parts.md,
// docs/mechdef-configs.md). Original file unknown: between mech_waynet.cpp and
// mission.cpp (the engine and armor helpers sit elsewhere, noted on each).
namespace mech3 {

int32_t part_id(const char* name);   // 0x00484e40, -1 when unknown
const char* part_name(int32_t id);   // 0x00484cc0, "none" when unknown

enum Tech : int32_t { kClan = 1, kInnerSphere = 2, kAnyTech = 3 };
int32_t mech_tech(int32_t mech);           // 0x00484720
const char* mech_name(int32_t mech);       // 0x00484650, "notmech" when out of range
int32_t tech_id(const char* name);         // 0x004852b0
int32_t equip_kind(const char* name);      // 0x004849a0, -1 when unknown
int32_t weapon_index(const char* name);    // 0x004848a0, -1 when unknown
int32_t engine_index(int32_t rating);      // 0x00432e30
int32_t armor_points(int32_t tech, bool ferro, float tons);  // 0x00424760
bool equipment_restricted(int32_t mech, int32_t kind, int32_t tech);  // 0x004d6dc0

}  // namespace mech3
