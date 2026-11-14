// Probe for tests/differential/test_pm_screen_modes.py: Pirate's Moon's screen modes, the range 0x004af210 and
// Shift+V 0x004af360 with its step 0x004af2d0 (the port's mech3::Engine video_mode_range, vmode_toggle and
// next_vmode with the table's screen_modes_listed), the card's mode list, the current mode and the switch
// answered from the case and logged.
#include <cstdint>
#include <vector>

#include "gamez/zgame/zgame_table.h"
#include "gamez/zutil/zfix.h"
#include "mech3/app_hooks.h"
#include "pm/pm_game.h"

namespace {
std::vector<uint32_t> g_log;
struct Fake : mech3::Engine {
  int32_t device = 0, current = 0, in_game = -1;
  uint32_t listed = 0;   // bit m: mode m on the card's list
  uint32_t refused = 0;  // bit m: the switch keeps the old mode
  int32_t hw_card_dev() override { return device; }
  int32_t vmode() override { return current; }
  bool video_mode_listed(int32_t, int32_t mode) override {
    g_log.insert(g_log.end(), {1u, static_cast<uint32_t>(mode)});
    return (listed >> mode) & 1;
  }
  int32_t set_screen_mode(int32_t mode) override {
    g_log.insert(g_log.end(), {2u, static_cast<uint32_t>(mode)});
    if ((refused >> mode) & 1) return current;
    current = mode;
    return mode;
  }
  void set_in_game_vmode(int32_t v) override { in_game = v; }
};
}  // namespace

extern "C" {
// which 0: the range (out lo, hi, then the list's asks); 1: Shift+V (out the mode after, the in-game mode (-1
// untouched), then the asks and switches).
int probe_pm_screen_modes(int which, int32_t device, int32_t current, uint32_t listed, uint32_t refused,
                          uint32_t* out) {
  zgame::set_game(pm::table());
  Fake e;
  e.device = device, e.current = current, e.listed = listed, e.refused = refused;
  g_log.clear();
  int n = 0;
  if (which == 0) {
    int32_t lo = -1, hi = -1;
    e.video_mode_range(device, &lo, &hi);
    out[n++] = static_cast<uint32_t>(lo), out[n++] = static_cast<uint32_t>(hi);
  } else {
    e.vmode_toggle();
    out[n++] = static_cast<uint32_t>(e.current), out[n++] = static_cast<uint32_t>(e.in_game);
  }
  for (uint32_t v : g_log) out[n++] = v;
  return n;
}
// The always-on fixes (zfix fixed: the endless round given up), for the test's second part.
void probe_pm_screen_modes_fixed(int on) { zfix::options().fixed = on != 0; }
}  // extern "C"
