#pragma once
#include <cstdint>
#include <string>

// Mech, part, weapon and equipment names and ids (docs/mechdef-parts.md,
// docs/mechdef-configs.md). Original file unknown: between mech_waynet.cpp and
// mission.cpp (the engine and armor helpers sit elsewhere, noted on each).
namespace mech3 {

int32_t part_id(const char* name);   // 0x00484e40, -1 when unknown
const char* part_name(int32_t id);   // 0x00484cc0, "none" when unknown
// 0x00484d70: a part's name for the player, Mech3Msg.dll's for 1..8 (by
// `message`), the exe's English for 9..15 (hands, calves, feet, hip), else "none".
const char* part_display_name(int32_t id, const char* (*message)(void* ctx, int32_t), void* ctx);

enum Tech : int32_t { kClan = 1, kInnerSphere = 2, kAnyTech = 3 };
int32_t mech_tech(int32_t mech);           // 0x00484720
const char* mech_name(int32_t mech);       // 0x00484650, "notmech" when out of range
int32_t mech_index(const char* name);      // 0x004845f0, any case; -1 when unknown
int32_t tech_id(const char* name);         // 0x004852b0
int32_t equip_kind(const char* name);      // 0x004849a0, -1 when unknown
int32_t weapon_index(const char* name);    // 0x004848a0, -1 when unknown
int32_t engine_index(int32_t rating);      // 0x00432e30
int32_t armor_points(int32_t tech, bool ferro, float tons);  // 0x00424760
bool equipment_restricted(int32_t mech, int32_t kind, int32_t tech);  // 0x004d6dc0
const char* weapon_name(int32_t weapon);   // 0x00484900, "noweapon" when out of range
int32_t weapon_count();                    // 37 in the exe, 38 with the plain PPC (FIX (bug)); and the mods' rows
bool weapon_uses_ammo(int32_t weapon);     // 0x00484960, the table's byte at 0x005e6504
bool weapon_has_tech(int32_t weapon, int32_t tech);  // 0x00484940, the table's mask at 0x005e6500
int32_t weapon_family(int32_t weapon);     // 0x004853c0, the table's word at 0x005e64f8: its destroyed voice
bool equip_has_tech(int32_t kind, int32_t tech);    // 0x00484a00, the table's mask at 0x005e6878
// The Mech3Msg.dll ids of the shown names (the tables' first words): a mech's
// 0x00484670 (0x005e6098), a weapon's 0x00484920 (0x005e64f4), equipment's
// 0x00484980 (0x005e6874); -1 out of range ("notmech", "noweapon", "none").
int32_t mech_name_message(int32_t mech);
int32_t weapon_name_message(int32_t weapon);
int32_t equip_name_message(int32_t kind);
float max_mass(int32_t mech);              // 0x004846e0, 10 when out of range
int32_t weight_class(int32_t mech);        // 0x00484700: 0..3 (the roster's +0x2c), 0 when out of range
// 0x00485400 / 0x00485430 / 0x00485460 / 0x00485490: the internal structure
// points by tonnage (0x005e6998), column 1 centre torso, 2 side torso, 3 arm, 4 leg.
int32_t internal_points(int32_t mech, int32_t column);
float equip_weight(int32_t kind, int32_t tech);  // 0x00484aa0

// Mods' mechs (ours, docs/modding.md): rows from 20, from mod.txt's mech=
// lines, so the exe's 19 keep their indices and 19 (0x13) stays the exe's
// "no mech" (saved pilots, the debriefing, the waiting mech). A row's model (its own name
// when not given) names the mech whose models, motions and sounds it uses;
// its dfn_<name>.zrd and saved configs are its own.
struct ModMech {
  std::string name, model, title;
  int32_t tech = kInnerSphere;
  int32_t mass = 0;          // tons, 10..100 in steps of 5 (the internal structure's rows)
  int32_t weight_class = 0;  // 0 light .. 3 assault
};
constexpr int32_t kBuiltinMechs = 19;
constexpr int32_t kNoMech = 19;             // 0x13: none
constexpr int32_t kFirstModMech = 20;
int32_t mech_count();                       // 19, or 20 and the mods' rows (19 none)
const char* mech_model_name(int32_t mech);  // the row's model, else its name
const char* mech_title(int32_t mech);       // a mod row's title=, null for the exe's
// One mech= line's value: `NAME tech=clan|sphere|any mass=T class=light|medium|
// heavy|assault [model=MECH] [title=TEXT]`, title= last (the rest of the line).
bool mech_row_parse(const std::string& value, ModMech* out, std::string* error);
bool mech_row_add(const ModMech& row, std::string* error);  // refused when the name is taken
void mech_rows_clear();                     // the mods' rows gone

// Mods' weapons (ours): rows after the exe's 37 and the plain PPC (38th, FIX
// (bug), offered only with the bug fixes on), from mod.txt's weapon= lines:
// `NAME ammo=yes|no tech=clan|sphere|any|none [title=TEXT]`. The weapon
// itself is weapons.zrd's definition of that name.
struct ModWeapon {
  std::string name, title;
  bool ammo = false;
  int32_t tech = kAnyTech;  // the mask: 1 Clan, 2 Inner Sphere, 0 none (as TAG)
};
constexpr int32_t kBuiltinWeapons = 38;
const char* weapon_title(int32_t weapon);   // a mod row's title=, null for the exe's
bool weapon_row_parse(const std::string& value, ModWeapon* out, std::string* error);
bool weapon_row_add(const ModWeapon& row, std::string* error);  // refused when the name is taken
void weapon_rows_clear();
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
double engine_weight_of(int32_t index, uint8_t xl);  // the same two on plain numbers
int32_t engine_rating_of(int32_t index);

// A weapon's mech-lab block: by default read from the loaded weapon
// definitions (WeaponType, mech3/weapon.h); a test can set its own lookup.
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
