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
const char* weapon_name(int32_t weapon);   // 0x00484900, "noweapon" when out of range
float max_mass(int32_t mech);              // 0x004846e0, 10 when out of range
float equip_weight(int32_t kind, int32_t tech);  // 0x00484aa0
int32_t equip_slots(int32_t kind, int32_t tech); // 0x00484a20

// The engine table 0x005e2f50 (79 entries: rating, name id, tons), through
// a config's engine (+0x24 index, +0x28 xl).
struct Engine {
  int32_t index;
  uint8_t xl;
};
float engine_weight(const Engine& e);      // 0x00432f40, XL: half the tons rounded up
double engine_weight_x87(const Engine& e); // the same, as left on the x87 stack
int32_t engine_rating(const Engine& e);    // 0x00432fa0

// A weapon's mech-lab block (+0x1f0 of the weapon zWeapon keeps, from
// weapons.zrd), not ported yet: the front end or a test sets the lookup.
struct WeaponLab {
  float weight[2];                 // +0x1c Clan, +0x28 otherwise
  int32_t slots[2];                // +0x20, +0x2c
  float ammo_weight;               // +0x34
};
using WeaponLabFn = const WeaponLab* (*)(const char* name);  // 0x00581ed0 then +0x1f0; 0 when none
void set_weapon_lab(WeaponLabFn fn);
float weapon_weight(int32_t weapon, int32_t tech);           // 0x00484b70
float ammo_weight(int32_t weapon);                           // 0x00484bc0
int32_t weapon_slots(int32_t weapon, int32_t tech, bool none);  // 0x00484c00

}  // namespace mech3
