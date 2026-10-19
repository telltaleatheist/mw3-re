#include "mech3/mech_names.h"

#include <cmath>
#include <cstring>

#include "crt/msvc_math.h"
#include "crt/msvc_string.h"

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
const char* const kWeapons[37] = {
    "AC2",          "AC5",          "AC10",          "AC20",         "ERLaser_Small",     "ERLaser_Medium",
    "ERLaser_Large", "ERPPC",       "Flamer",        "GaussRifle",   "Laser_Small",       "Laser_Medium",
    "Laser_Large",  "LB2XAC",       "LB5XAC",        "LB10XAC",      "LB20XAC",           "LRM5",
    "LRM10",        "LRM15",        "LRM20",         "MGUN",         "NARC",              "PulseLaser_Small",
    "PulseLaser_Medium", "PulseLaser_Large", "SRM2", "SRM4",         "SRM6",              "StreakSRM2",
    "StreakSRM4",   "StreakSRM6",   "TAG",           "UltraAC2",     "UltraAC5",          "UltraAC10",
    "UltraAC20"};
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
  for (int32_t i = 0; i < 37; ++i)
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

}  // namespace mech3
