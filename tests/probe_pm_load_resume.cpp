// The Load State's resume back from the lab on Pirate's Moon (0x00407fb0): an Instant Action
// line-up's mechs into the Action screen's setup, for test_pm_load_resume.py.
#include <cstdint>

#include "mech3/pilot.h"

// n slots (0 the player's), their mechs and sides (side[0] unused); the setup's eleven entries
// (+0x40.. in PM: g820 then g838) start as `before` and come back in `out`.
extern "C" void probe_pm_load_resume(int32_t n, const int32_t* mechs, const uint8_t* sides, const int32_t* before,
                                     int32_t* out) {
  mech3::ActionGame a(nullptr);
  a.slot.mech = mechs[0];
  a.count = n - 1;
  for (int32_t i = 1; i < n; ++i) a.lineup[i - 1].mech = mechs[i], a.side[i - 1] = sides[i];
  for (int32_t i = 0; i < 5; ++i) a.setup.g820[i] = before[i];
  for (int32_t i = 0; i < 6; ++i) a.setup.g838[i] = before[5 + i];
  mech3::action_setup_from_lineup(a);
  for (int32_t i = 0; i < 5; ++i) out[i] = a.setup.g820[i];
  for (int32_t i = 0; i < 6; ++i) out[5 + i] = a.setup.g838[i];
}
