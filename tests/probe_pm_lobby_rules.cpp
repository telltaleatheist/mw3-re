// Pirate's Moon's lobby rules for test_pm_net_score.py (piece 2 of the multiplayer group, pm-tests.md).
#include <cstdint>
#include <memory>
#include <vector>

#include "mech3/mp_connect.h"
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

// Pirate's Moon's rescore (0x004374f0) for test_pm_rescore.py. in: team play, the rounds flag, the players (count,
// then 8 of: id, team, order, kills, deaths, rounds, a score line), the teams (count, then 4 of: id, the third data
// word, rounds, a score line). out: each player's line (score, key; -1 -1 none), each team's (score, key) and kills.
extern "C" void probe_pm_rescore(const int32_t* in, int32_t* out) {
  mech3::multiplayer_setup().team_play = in[0];
  mech3::MultiplayerGame g(nullptr);
  std::vector<std::unique_ptr<mech3::ScoreLine>> lines;
  const int32_t* p = in + 3;
  for (int32_t k = 0; k < in[2]; ++k, p += 7) {
    auto r = std::make_unique<mech3::LobbyPlayer>();
    r->id = static_cast<uint32_t>(p[0]), r->team = static_cast<uint32_t>(p[1]), r->order = p[2];
    r->kills = p[3], r->deaths = p[4], r->rounds = p[5];
    if (p[6]) lines.push_back(std::make_unique<mech3::ScoreLine>()), r->score = lines.back().get();
    g.lobby_players.push_back(std::move(r));
  }
  const int32_t* t = in + 3 + 8 * 7 + 1;
  for (int32_t k = 0; k < in[3 + 8 * 7]; ++k, t += 4) {
    auto r = std::make_unique<mech3::LobbyTeam>();
    r->id = static_cast<uint32_t>(t[0]), r->word2 = t[1], r->rounds = t[2], r->kills = -5;
    if (t[3]) lines.push_back(std::make_unique<mech3::ScoreLine>()), r->score = lines.back().get();
    g.lobby_teams.push_back(std::move(r));
  }
  for (auto& l : lines) l->score = -7, l->key = 0xdead;
  g.rescore(in[1] != 0);
  for (const auto& r : g.lobby_players)
    *out++ = r->score ? r->score->score : -1, *out++ = r->score ? static_cast<int32_t>(r->score->key) : -1;
  for (const auto& r : g.lobby_teams)
    *out++ = r->score ? r->score->score : -1, *out++ = r->score ? static_cast<int32_t>(r->score->key) : -1,
    *out++ = r->kills;
  mech3::multiplayer_setup().team_play = 0;
}
