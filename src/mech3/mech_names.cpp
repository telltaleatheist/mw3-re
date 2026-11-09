#include "mech3/mech_names.h"

#include <cmath>
#include <cstring>

#include "crt/msvc_math.h"
#include "crt/msvc_string.h"
#include "gamez/zutil/zfix.h"
#include "mech3/weapon.h"

namespace mech3 {

// Mech3.exe 1.2 @ 0x00484e40. lleg and rleg are aliases of the thighs.
int32_t part_id(const char* name) {
  static const struct { const char* name; int32_t id; } kIds[] = {
      {"hip", 15},    {"torso", 1},   {"larm", 2},   {"rarm", 3},   {"lhand", 9},  {"rhand", 10},
      {"lthigh", 4},  {"rthigh", 5},  {"lleg", 4},   {"rleg", 5},   {"lcalf", 11}, {"rcalf", 12},
      {"lfoot", 13},  {"rfoot", 14},  {"head", 6},   {"ltorso", 7}, {"rtorso", 8}};
  if (!name) return -1;
  for (const auto& e : kIds)
    if (std::strcmp(name, e.name) == 0) return e.id;
  return -1;
}

// Mech3.exe 1.2 @ 0x00484cc0
const char* part_name(int32_t id) {
  static const char* const kNames[] = {"torso", "larm",   "rarm",  "lthigh", "rthigh", "head",  "ltorso", "rtorso",
                                       "lhand", "rhand",  "lcalf", "rcalf",  "lfoot",  "rfoot", "hip"};
  if (id < 1 || id > 15) return "none";
  return kNames[id - 1];
}

namespace {
// Pirate's Moon 0x00608238, 0x3c bytes each: the name, +0x20 the message, +0x28 the tons, +0x2c the class, +0x30 the tech.
const struct { const char* name; int32_t message; float mass; int32_t weight_class; int32_t tech; } kMechs[kMechCount] = {
    {"annihilator", 0xd82, 100, 3, 2},
    {"atlas", 0xfeb, 100, 3, 2},
    {"avatar", 0xd83, 70, 2, 2},
    {"awesome", 0xfec, 80, 3, 2},
    {"blackhawk", 0xd84, 50, 1, 1},
    {"bushwacker", 0xd85, 55, 1, 2},
    {"cauldron", 0xd86, 65, 2, 1},
    {"centurion", 0xfed, 50, 1, 2},
    {"champion", 0xd87, 60, 2, 2},
    {"clint", 0xfee, 40, 1, 1},
    {"daishi", 0xd88, 100, 3, 1},
    {"elemental", 0xd89, 10, 0, 1},
    {"firefly", 0xd8a, 30, 0, 2},
    {"madcat", 0xd8b, 75, 2, 1},
    {"masakari", 0xfef, 85, 3, 1},
    {"orion", 0xd8c, 75, 2, 2},
    {"owens", 0xd8d, 35, 0, 2},
    {"puma", 0xd8e, 35, 0, 1},
    {"ryoken", 0xff0, 55, 1, 1},
    {"shadowcat", 0xd8f, 45, 1, 1},
    {"strider", 0xd90, 40, 1, 2},
    {"sunder", 0xd91, 90, 3, 2},
    {"supernova", 0xd92, 90, 3, 1},
    {"thor", 0xd93, 70, 2, 1},
    {"vulture", 0xd94, 60, 2, 1}};

// 0x005e6868, 5 dwords each; the kind is the first.
const char* const kEquipment[15] = {"ams",  "armor", "armorff", "fcs",  "bap", "case", "c3", "c3slave",
                                    "ecm",  "sink",  "dsink",   "jjets", "masc", "tag", "tc"};

// Pirate's Moon 0x00608818, 6 dwords each (docs/weapons.md): the name, the message, the family (0x004875a0), the
// index (the position), the techs (1 Clan, 2 Inner Sphere), uses ammo.
const struct { const char* name; int32_t message; int32_t family; uint8_t tech; uint8_t ammo; } kWeapons[kWeaponCount] = {
    {"AC2", 0x2d7, 1, 2, 1},
    {"AC5", 0x2d8, 1, 2, 1},
    {"AC10", 0x2d9, 1, 2, 1},
    {"AC20", 0x2da, 1, 2, 1},
    {"ERLaser_Small", 0x2db, 2, 3, 0},
    {"ERLaser_Medium", 0x2dc, 2, 3, 0},
    {"ERLaser_Micro", 0xff9, 2, 1, 0},
    {"ERLaser_Large", 0x2dd, 2, 3, 0},
    {"PPC", 0x2fd, 10, 2, 0},
    {"ERPPC", 0x2de, 3, 3, 0},
    {"Flamer", 0x2df, 5, 3, 0},
    {"GaussRifle", 0x2e0, 4, 3, 1},
    {"GaussRifleLt", 0xfe0, 4, 2, 1},
    {"HeavyLaser_Small", 0xfe1, 6, 1, 0},
    {"HeavyLaser_Medium", 0xfe2, 6, 1, 0},
    {"HeavyLaser_Large", 0xfe3, 6, 1, 0},
    {"Laser_Small", 0x2e1, 6, 2, 0},
    {"Laser_Medium", 0x2e2, 6, 2, 0},
    {"Laser_Large", 0x2e3, 6, 2, 0},
    {"LB2XAC", 0x2e4, 0, 3, 1},
    {"LB5XAC", 0x2e5, 0, 3, 1},
    {"LB10XAC", 0xe6, 0, 3, 1},
    {"LB20XAC", 0x2e7, 0, 3, 1},
    {"LRM5", 0x2e8, 7, 3, 1},
    {"LRM10", 0x2e9, 7, 3, 1},
    {"LRM15", 0x2ea, 7, 3, 1},
    {"LRM20", 0x2eb, 7, 3, 1},
    {"MRM10", 0xfe4, 7, 2, 1},
    {"MRM20", 0xfe5, 7, 2, 1},
    {"MRM30", 0xfe6, 7, 2, 1},
    {"MRM40", 0xfe7, 7, 2, 1},
    {"MGUN", 0x2ec, 8, 3, 1},
    {"MGUNH", 0xffa, 8, 1, 1},
    {"MGUNLT", 0xffb, 8, 1, 1},
    {"NARC", 0x2ed, 9, 3, 1},
    {"PulseLaser_Small", 0x2ef, 11, 3, 0},
    {"PulseLaser_Medium", 0x2f0, 11, 3, 0},
    {"PulseLaser_Large", 0x2f1, 11, 3, 0},
    {"SRM2", 0x2f2, 12, 3, 1},
    {"SRM4", 0x2f3, 12, 3, 1},
    {"SRM6", 0x2f4, 12, 3, 1},
    {"StreakSRM2", 0x2f5, 13, 3, 1},
    {"StreakSRM4", 0x2f6, 13, 3, 1},
    {"StreakSRM6", 0x2f7, 13, 3, 1},
    {"TAG", 0x2f8, 14, 0, 0},
    {"TBOLT5", 0xffc, 7, 2, 1},
    {"TBOLT10", 0xffd, 7, 2, 1},
    {"TBOLT15", 0xffe, 7, 2, 1},
    {"TBOLT20", 0xfff, 7, 2, 1},
    {"UltraAC2", 0x2f9, 1, 3, 1},
    {"UltraAC5", 0x2fa, 1, 3, 1},
    {"UltraAC10", 0x2fb, 1, 3, 1},
    {"UltraAC20", 0x2fc, 1, 3, 1},
    {"XPulseLaser_Small", 0xfe8, 11, 2, 0},
    {"XPulseLaser_Medium", 0xfe9, 11, 2, 0},
    {"XPulseLaser_Large", 0xfea, 11, 2, 0}};
}  // namespace

// Mech3.exe 1.2 @ 0x00484720
int32_t mech_tech(int32_t mech) { return mech >= 0 && mech < kMechCount ? kMechs[mech].tech : kAnyTech; }

// Mech3.exe 1.2 @ 0x00484650
const char* mech_name(int32_t mech) { return mech >= 0 && mech < kMechCount ? kMechs[mech].name : "notmech"; }

// Mech3.exe 1.2 @ 0x004845f0. The table's +0x24 is the entry's own index.
int32_t mech_index(const char* name) {
  for (int32_t i = 0; i < kMechCount; ++i)
    if (crt::msvc_stricmp(name, kMechs[i].name) == 0) return i;
  return -1;
}

// Mech3.exe 1.2 @ 0x004852b0. No name counts as Inner Sphere.
int32_t tech_id(const char* name) {
  if (!name) return kInnerSphere;
  if (std::strcmp(name, "clan") == 0) return kClan;
  if (std::strcmp(name, "sphere") == 0) return kInnerSphere;
  return kAnyTech;
}

// Mech3.exe 1.2 @ 0x004849a0. Case-sensitive; the exe crashes on null.
int32_t equip_kind(const char* name) {
  for (int32_t i = 0; i < 15; ++i)
    if (std::strcmp(name, kEquipment[i]) == 0) return i;
  return -1;
}

// Mech3.exe 1.2 @ 0x004848a0. Any case.
int32_t weapon_index(const char* name) {
  if (!name || !*name) return -1;
  for (int32_t i = 0; i < kWeaponCount; ++i)
    if (crt::msvc_stricmp(kWeapons[i].name, name) == 0) return i;
  return -1;
}

// Mech3.exe 1.2 @ 0x00432e30 (file unknown, near the mech lab). The table at
// 0x005e2f50 rates 10..400 in steps of 5; the index of the last one <= rating.
int32_t engine_index(int32_t rating) {
  if (rating > 400) rating = 400;
  else if (rating < 10) rating = 10;
  constexpr int32_t kEngines = 79;  // 0x005e3304
  int32_t last = 0;
  for (int32_t i = 0; i < kEngines; ++i) {
    if (rating < 10 + 5 * i) return last;
    last = i;
  }
  return last;
}

// Mech3.exe 1.2 @ 0x00424760 (file unknown). 16 points a ton, ferro-fibrous
// 1.2x (Clan) or 1.12x; the 0.0001 keeps exact products from flooring down.
int32_t armor_points(int32_t tech, bool ferro, float tons) {
  double k = 16.000099182128906;  // 0x005bd3e4, a float
  if (ferro) k = (tech == kClan ? 1.2000000476837158 : 1.12) * 16.000099182128906;
  return crt::msvc_ftol(std::floor(k * tons));
}

// Mech3.exe 1.2 @ 0x004d6dc0: a disabled check.
bool equipment_restricted(int32_t, int32_t, int32_t) { return false; }

namespace {
// 0x005e2f50 + 8: the tons of each engine, ratings 10..400 in steps of 5.
const float kEngineTons[79] = {
    0.5f,  0.5f,  0.5f,  0.5f,  1.0f,  1.0f,  1.0f,  1.0f,  1.5f,  1.5f,  1.5f,  2.0f,  2.0f,  2.0f,
    2.5f,  2.5f,  3.0f,  3.0f,  3.0f,  3.5f,  3.5f,  4.0f,  4.0f,  4.0f,  4.5f,  4.5f,  5.0f,  5.0f,
    5.5f,  5.5f,  6.0f,  6.0f,  6.0f,  7.0f,  7.0f,  7.5f,  7.5f,  8.0f,  8.5f,  8.5f,  9.0f,  9.5f,
    10.0f, 10.0f, 10.5f, 11.0f, 11.5f, 12.0f, 12.5f, 13.0f, 13.5f, 14.0f, 14.5f, 15.5f, 16.0f, 16.5f,
    17.5f, 18.0f, 19.0f, 19.5f, 20.5f, 21.5f, 22.5f, 23.5f, 24.5f, 25.5f, 27.0f, 28.5f, 29.5f, 31.5f,
    33.0f, 34.5f, 36.5f, 38.5f, 41.0f, 43.5f, 46.0f, 49.0f, 52.5f};
WeaponLabFn g_weapon_lab = nullptr;

const WeaponLab* weapon_lab(int32_t weapon) { return g_weapon_lab ? g_weapon_lab(weapon_name(weapon)) : nullptr; }
}  // namespace

// Mech3.exe 1.2 @ 0x00484960: autocannons, Gauss, LB-X, missiles, MGUN and NARC.
bool weapon_uses_ammo(int32_t weapon) {
  return weapon >= 0 && weapon < kWeaponCount && kWeapons[weapon].ammo;
}

// Pirate's Moon 1.0 @ 0x004875a0 (Mech3.exe 1.2 @ 0x004853c0): a weapon's family, its row's third
// word (0x00608820), which picks its "destroyed" voice (0x0062d438 + 4 x family, mech_lib's
// weapon_voice; the same 14 as MW3's): 1 autocannons, 2 ER lasers, 3 ER PPC, 4 Gauss, 5 flamer,
// 6 lasers, 7 LRM and MRM, 8 machine guns, 9 NARC, 10 PPC, 11 pulse lasers, 12 SRM, 13 Streak,
// 14 TAG; 0 (the LB-X autocannons) has none. The exe reads past the table out of range; 0 here.
int32_t weapon_family(int32_t weapon) { return weapon >= 0 && weapon < kWeaponCount ? kWeapons[weapon].family : 0; }

// Mech3.exe 1.2 @ 0x00484940: the techs a weapon comes in (1 Clan, 2 Inner
// Sphere; TAG none).
bool weapon_has_tech(int32_t weapon, int32_t tech) {
  return weapon >= 0 && weapon < kWeaponCount && (kWeapons[weapon].tech & tech) != 0;
}

// Mech3.exe 1.2 @ 0x00484a00: the techs an equipment kind comes in (1 Clan,
// 2 Inner Sphere); BAP, C3 and its slave Inner Sphere only.
bool equip_has_tech(int32_t kind, int32_t tech) {
  static const uint8_t kTech[15] = {3, 3, 3, 3, 2, 3, 2, 2, 3, 3, 3, 3, 3, 3, 3};
  return kind >= 0 && kind < 15 && (kTech[kind] & tech) != 0;
}

// Mech3.exe 1.2 @ 0x00484670: Mech3Msg.dll's ids in the mech table (Pirate's Moon's new mechs 0xfeb..).
int32_t mech_name_message(int32_t mech) { return mech >= 0 && mech < kMechCount ? kMechs[mech].message : -1; }

// Mech3.exe 1.2 @ 0x00484920
int32_t weapon_name_message(int32_t weapon) { return weapon >= 0 && weapon < kWeaponCount ? kWeapons[weapon].message : -1; }

// Mech3.exe 1.2 @ 0x00484980: 0x1c6.. by equipment kind.
int32_t equip_name_message(int32_t kind) { return kind >= 0 && kind < 15 ? 0x1c6 + kind : -1; }

// Mech3.exe 1.2 @ 0x00484900
const char* weapon_name(int32_t weapon) { return weapon >= 0 && weapon < kWeaponCount ? kWeapons[weapon].name : "noweapon"; }

// Pirate's Moon's table has the plain PPC MW3's lacked (index 8), so MW3's FIX (bug) has no place here.
int32_t weapon_count() { return kWeaponCount; }

// Mech3.exe 1.2 @ 0x00484700. Not the tonnage's class: the Annihilator is 2,
// the Mad Cat, Owens and Puma 1 (discoveries.md).
int32_t weight_class(int32_t mech) {
  return mech >= 0 && mech < kMechCount ? kMechs[mech].weight_class : 0;
}

// Mech3.exe 1.2 @ 0x004846e0
float max_mass(int32_t mech) { return mech >= 0 && mech < kMechCount ? kMechs[mech].mass : 10.0f; }

// Mech3.exe 1.2 @ 0x00485400 (and 0x00485430, 0x00485460, 0x00485490): the
// BattleTech table, a row per 5 t from 10 t (tonnage, CT, side torso, arm, leg).
int32_t internal_points(int32_t mech, int32_t column) {
  static const int32_t kTable[19][5] = {
      {10, 4, 3, 1, 2},     {15, 5, 4, 2, 3},     {20, 6, 5, 3, 4},     {25, 8, 6, 4, 6},     {30, 10, 7, 5, 7},
      {35, 11, 8, 6, 8},    {40, 12, 10, 6, 10},  {45, 14, 11, 7, 11},  {50, 16, 12, 8, 12},  {55, 18, 13, 9, 13},
      {60, 20, 14, 10, 14}, {65, 21, 15, 10, 15}, {70, 22, 15, 11, 15}, {75, 23, 16, 12, 16}, {80, 25, 17, 13, 17},
      {85, 27, 18, 14, 18}, {90, 29, 19, 15, 19}, {95, 30, 20, 16, 20}, {100, 31, 21, 17, 21}};
  const int32_t row = static_cast<int32_t>(max_mass(mech)) / 5 - 2;  // every roster mass is in the table
  return kTable[row][column];
}

// Mech3.exe 1.2 @ 0x00484aa0. Kinds as in equip_kind; tech 1 is Clan.
float equip_weight(int32_t kind, int32_t tech) {
  const bool clan = tech == kClan;
  switch (kind) {
    case 0: case 1: case 2: case 11: return 0.5f;
    case 3: case 9: case 10: case 13: case 14: return 1.0f;
    case 4: case 8: return clan ? 1.0f : 1.5f;
    case 5: return clan ? 0.0f : 0.5f;
    case 6: return clan ? 0.0f : 5.0f;
    case 7: return clan ? 0.0f : 1.0f;
    case 12: return clan ? 2.0f : 3.0f;
    default: return 0.0f;
  }
}

// Mech3.exe 1.2 @ 0x00484a20
int32_t equip_slots(int32_t kind, int32_t tech) {
  const int32_t other = tech != kClan;
  switch (kind) {
    case 0: case 3: case 9: case 11: case 13: case 14: return 1;
    case 4: case 8: return other + 1;
    case 5: case 7: return other;
    case 6: return other ? 5 : 0;
    case 10: return other + 2;
    case 12: return 3;
    default: return 0;
  }
}

// Mech3.exe 1.2 @ 0x00432f40. FIX (A): an index past the table read whatever
// followed it in the exe; here it weighs 0 (docs/fixes.md).
double engine_weight_x87(const Engine& e) {
  if (e.index < 0 || e.index >= 79) return 0.0;
  const float t = kEngineTons[e.index];
  return e.xl ? std::ceil(t) * 0.5 : t;
}
float engine_weight(const Engine& e) { return static_cast<float>(engine_weight_x87(e)); }

// Mech3.exe 1.2 @ 0x00432fa0. FIX (A) as above: 0 past the table.
int32_t engine_rating(const Engine& e) { return e.index >= 0 && e.index < 79 ? 10 + 5 * e.index : 0; }

// For lab_screen.cpp, where the app's Engine hides this one.
double engine_weight_of(int32_t index, uint8_t xl) { return engine_weight_x87(Engine{index, xl}); }
int32_t engine_rating_of(int32_t index) { return engine_rating(Engine{index, 0}); }

void set_weapon_lab(WeaponLabFn fn) { g_weapon_lab = fn; }

// Mech3.exe 1.2 @ 0x00484b70
float weapon_weight(int32_t weapon, int32_t tech) {
  if (g_weapon_lab) {
    const WeaponLab* w = weapon_lab(weapon);
    return w ? w->weight[tech != kClan] : 0.0f;
  }
  const WeaponType* t = weapon_type(weapon_name(weapon));
  return t ? t->lab[tech != kClan].tons : 0.0f;
}

// Mech3.exe 1.2 @ 0x00484bc0
float ammo_weight(int32_t weapon) {
  if (g_weapon_lab) {
    const WeaponLab* w = weapon_lab(weapon);
    return w ? w->ammo_weight : 0.0f;
  }
  const WeaponType* t = weapon_type(weapon_name(weapon));
  return t ? t->tons_per_round : 0.0f;
}

// Mech3.exe 1.2 @ 0x00484c00
int32_t weapon_slots(int32_t weapon, int32_t tech, bool none) {
  if (g_weapon_lab) {
    const WeaponLab* w = weapon_lab(weapon);
    return w && !none ? w->slots[tech != kClan] : 0;
  }
  const WeaponType* t = weapon_type(weapon_name(weapon));
  return t && !none ? t->lab[tech != kClan].slots : 0;
}

// Mech3.exe 1.2 @ 0x00484d70
const char* part_display_name(int32_t part, const char* (*message)(void*, int32_t), void* ctx) {
  static constexpr int32_t kMsg[9] = {0, 0x194, 0x197, 0x198, 0x199, 0x19a, 0x193, 0x196, 0x195};
  static const char* const kNames[7] = {"Left Hand", "Right Hand", "Left Calf", "Right Calf", "Left Foot",
                                        "Right Foot", "Hip"};
  if (part >= 1 && part <= 8) return message(ctx, kMsg[part]);
  if (part >= 9 && part <= 15) return kNames[part - 9];
  return "none";
}

}  // namespace mech3
