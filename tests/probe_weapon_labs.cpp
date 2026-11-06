// The weapons' mech-lab blocks for the probes: test_lab_screen.py, test_mechsave.py and their neighbours.
#include <cstdint>
#include <cstring>

#include "mech3/mech_names.h"

namespace {
mech3::WeaponLab g_labs[37];
uint8_t g_lab_present[37];
// As lab_weapons.py's WEAPON_NAMES: the order the test hands them in (PM's own table runs differently).
const char* const kNames[37] = {
    "AC2", "AC5", "AC10", "AC20", "ERLaser_Small", "ERLaser_Medium", "ERLaser_Large", "ERPPC", "Flamer", "GaussRifle",
    "Laser_Small", "Laser_Medium", "Laser_Large", "LB2XAC", "LB5XAC", "LB10XAC", "LB20XAC", "LRM5", "LRM10", "LRM15",
    "LRM20", "MGUN", "NARC", "PulseLaser_Small", "PulseLaser_Medium", "PulseLaser_Large", "SRM2", "SRM4", "SRM6",
    "StreakSRM2", "StreakSRM4", "StreakSRM6", "TAG", "UltraAC2", "UltraAC5", "UltraAC10", "UltraAC20"};
}  // namespace

extern "C" {

// The weapons' mech-lab blocks the test hands both sides; absent ones are 0.
void probe_set_weapon_labs(const float* table, const uint8_t* present) {
  std::memcpy(g_labs, table, sizeof g_labs);
  std::memcpy(g_lab_present, present, sizeof g_lab_present);
  mech3::set_weapon_lab([](const char* name) -> const mech3::WeaponLab* {
    for (int i = 0; i < 37; ++i)
      if (std::strcmp(kNames[i], name) == 0) return g_lab_present[i] ? &g_labs[i] : nullptr;
    return nullptr;
  });
}

}  // extern "C"
