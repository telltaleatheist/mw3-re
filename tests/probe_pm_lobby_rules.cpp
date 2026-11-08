// Pirate's Moon's lobby rules for test_pm_net_score.py (piece 2 of the multiplayer group, pm-tests.md).
#include <cstdint>
#include <memory>

#include "mech3/pilot.h"

// Pirate's Moon's suicide (0x00436820) for test_pm_net_score.py: player 7 listed or not, its kills and deaths, the
// deaths to add; out: the answer, its deaths after (-9 not listed).
extern "C" void probe_pm_add_deaths(int32_t listed, int32_t kills, int32_t deaths, int32_t n, int32_t* out) {
  mech3::MultiplayerGame g(nullptr);
  mech3::LobbyPlayer* p = nullptr;
  if (listed) {
    auto r = std::make_unique<mech3::LobbyPlayer>();
    r->id = 7, r->kills = kills, r->deaths = deaths;
    p = r.get();
    g.lobby_players.push_back(std::move(r));
  }
  out[0] = g.add_deaths(7, n);
  out[1] = p ? p->deaths : -9;
}
