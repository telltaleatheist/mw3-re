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
// 0x005e6078, 0x3c bytes each: the name, and the tech at +0x30.
const struct { const char* name; int32_t tech; } kMechs[19] = {
    {"annihilator", 2}, {"avatar", 2},    {"blackhawk", 1}, {"bushwacker", 2}, {"cauldron", 1},
    {"champion", 2},    {"daishi", 1},    {"elemental", 1}, {"firefly", 2},    {"madcat", 1},
    {"orion", 2},       {"owens", 2},     {"puma", 1},      {"shadowcat", 1},  {"strider", 2},
    {"sunder", 2},      {"supernova", 1}, {"thor", 1},      {"vulture", 1}};

// 0x005e6868, 5 dwords each; the kind is the first.
const char* const kEquipment[15] = {"ams",  "armor", "armorff", "fcs",  "bap", "case", "c3", "c3slave",
                                    "ecm",  "sink",  "dsink",   "jjets", "masc", "tag", "tc"};

// 0x005e64f0, 6 dwords each (docs/weapons.md); the index equals the position.
// FIX (bug): the 38th, the plain PPC, is the rebuild's (weapon_count).
const char* const kWeapons[38] = {
    "AC2",          "AC5",          "AC10",          "AC20",         "ERLaser_Small",     "ERLaser_Medium",
    "ERLaser_Large", "ERPPC",       "Flamer",        "GaussRifle",   "Laser_Small",       "Laser_Medium",
    "Laser_Large",  "LB2XAC",       "LB5XAC",        "LB10XAC",      "LB20XAC",           "LRM5",
    "LRM10",        "LRM15",        "LRM20",         "MGUN",         "NARC",              "PulseLaser_Small",
    "PulseLaser_Medium", "PulseLaser_Large", "SRM2", "SRM4",         "SRM6",              "StreakSRM2",
    "StreakSRM4",   "StreakSRM6",   "TAG",           "UltraAC2",     "UltraAC5",          "UltraAC10",
    "UltraAC20",    "PPC"};
}  // namespace

// Mech3.exe 1.2 @ 0x00484720
int32_t mech_tech(int32_t mech) { return mech >= 0 && mech < 19 ? kMechs[mech].tech : kAnyTech; }

// Mech3.exe 1.2 @ 0x00484650
const char* mech_name(int32_t mech) { return mech >= 0 && mech < 19 ? kMechs[mech].name : "notmech"; }

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
  for (int32_t i = 0; i < weapon_count(); ++i)
    if (crt::msvc_stricmp(kWeapons[i], name) == 0) return i;
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
// 0x005e60a0 + 0x3c * mech
const float kMaxMass[19] = {100, 70, 50, 55, 65, 60, 100, 10, 30, 75, 75, 35, 35, 45, 40, 90, 90, 70, 60};
WeaponLabFn g_weapon_lab = nullptr;

const WeaponLab* weapon_lab(int32_t weapon) { return g_weapon_lab ? g_weapon_lab(weapon_name(weapon)) : nullptr; }
}  // namespace

// Mech3.exe 1.2 @ 0x00484960: autocannons, Gauss, LB-X, missiles, MGUN and NARC.
bool weapon_uses_ammo(int32_t weapon) {
  static const uint8_t kUses[38] = {1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1,
                                    1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0};
  return weapon >= 0 && weapon < weapon_count() && kUses[weapon];
}

// Mech3.exe 1.2 @ 0x00484900
const char* weapon_name(int32_t weapon) { return weapon >= 0 && weapon < weapon_count() ? kWeapons[weapon] : "noweapon"; }

// The exe's table has 37 weapons. FIX (bug): weapons.zrd defines a plain PPC and
// Mech3Msg.dll names it (0x2fd, after the last weapon's 0x2fc), but its row was
// never added, so the mech lab could not offer it. Its row would be message
// 0x2fd, family 3 (as ERPPC), tech 2 (Inner Sphere, as the plain lasers), no ammo.
int32_t weapon_count() { return zfix::options().bugs ? 38 : 37; }

// Mech3.exe 1.2 @ 0x004846e0
float max_mass(int32_t mech) { return mech >= 0 && mech < 19 ? kMaxMass[mech] : 10.0f; }

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

}  // namespace mech3
