// Probe for tests/differential/test_pm_mech_sounds.py: Pirate's Moon's mech data sounds 0x00430a70 (the port's
// mech3::load_sounds with the table's targeting_sound): each lookup's name in order, and the slot it lands in.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "gamez/zgame/zgame_table.h"
#include "mech3/mech.h"
#include "mech3/mech_data.h"
#include "pm/pm_game.h"

namespace {
std::vector<std::string> g_names;
struct Fake : mech3::MechDataHooks {
  mech3::Handle sound_find(const char* name) override {
    g_names.emplace_back(name);
    return reinterpret_cast<mech3::Handle>(static_cast<uintptr_t>(g_names.size()));  // 1, 2, ... by lookup
  }
};
}  // namespace

extern "C" {
// in: the mech's name, pm (PM's table or MW3's). out: the names looked up, each NUL-ended, then a blank; slots:
// each slot's lookup number (0 none). Returns the names' bytes.
int probe_pm_mech_sounds(const char* mech, int pm, char* names, int cap, uint32_t* slots) {
  zgame::set_game(pm ? pm::table() : zgame::mw3_table());
  static Fake fake;
  mech3::set_mech_data_hooks(&fake);
  g_names.clear();
  mech3::MechData d;
  d.name = mech;
  mech3::load_sounds(d);
  mech3::set_mech_data_hooks(nullptr);
  zgame::set_game(pm::table());
  for (int i = 0; i < 25; ++i) slots[i] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(d.sounds[i]));
  int n = 0;
  for (const std::string& s : g_names) {
    if (n + static_cast<int>(s.size()) + 1 >= cap) break;
    std::memcpy(names + n, s.c_str(), s.size() + 1);
    n += static_cast<int>(s.size()) + 1;
  }
  return n;
}
}  // extern "C"
