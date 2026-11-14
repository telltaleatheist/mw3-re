// Probe for tests/differential/test_pm_weapon_name.py: Pirate's Moon's weapon name 0x00486a50 by its table (the
// port's weapon_name_message and the HUD hook's rule: the table's message for rows 0..0x37, else "noweapon").
#include <cstdint>

#include "mech3/mech_names.h"

extern "C" {
// The message id the name is the text of, or -1 for "noweapon".
int32_t probe_pm_weapon_name(int32_t index) { return mech3::weapon_name_message(index); }
}
