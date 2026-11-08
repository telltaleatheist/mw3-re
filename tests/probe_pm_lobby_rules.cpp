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

// Pirate's Moon's tally (0x00436980) for test_pm_tally.py. in: team play, award, the game's kind (+0x98), end reason
// (+0x90), base lost (+0x8c), +0xb0; the players (count, then 8 of: id, team, order, kills, deaths, rounds, left,
// place), the teams (count, then 4 of: id, word2, rounds, place), the lines' order (12: a player's index, or 8 + a
// team's). out: the answer, each player's (rounds, place, line score, key, rank), each team's (rounds, place, line
// score, key, rank, kills).
// winner: the round winner (0x00437970) instead, award the game's +0xaa.
static int32_t tally_run(const int32_t* in, int32_t* out, bool winner) {
  mech3::multiplayer_setup().team_play = in[0];
  mech3::MultiplayerGame g(nullptr);
  g.kind = in[2], g.round_end = in[3], g.base_lost = in[4], g.xb0 = in[5];
  std::vector<mech3::ScoreLine*> lines(12, nullptr);
  const int32_t* p = in + 7;
  for (int32_t k = 0; k < in[6]; ++k, p += 8) {
    auto r = std::make_unique<mech3::LobbyPlayer>();
    r->id = static_cast<uint32_t>(p[0]), r->team = static_cast<uint32_t>(p[1]), r->order = p[2];
    r->kills = p[3], r->deaths = p[4], r->rounds = p[5], r->left = static_cast<uint8_t>(p[6]), r->place = p[7];
    auto l = std::make_unique<mech3::ScoreLine>();
    l->kind = 1, l->score = -7, l->key = 0xdead, l->rank = -9;
    r->score = lines[k] = l.get();
    g.score_lines.push_back(std::move(l));
    g.lobby_players.push_back(std::move(r));
  }
  const int32_t tbase = 7 + 8 * 8;
  const int32_t* t = in + tbase + 1;
  for (int32_t k = 0; k < in[tbase]; ++k, t += 4) {
    auto r = std::make_unique<mech3::LobbyTeam>();
    r->id = static_cast<uint32_t>(t[0]), r->word2 = t[1], r->rounds = t[2], r->place = t[3], r->kills = -5;
    auto l = std::make_unique<mech3::ScoreLine>();
    l->kind = 2, l->score = -7, l->key = 0xdead, l->rank = -9;
    r->score = lines[8 + k] = l.get();
    g.score_lines.push_back(std::move(l));
    g.lobby_teams.push_back(std::move(r));
  }
  // the lines in the order given
  const int32_t* order = in + tbase + 1 + 4 * 4;
  std::vector<std::unique_ptr<mech3::ScoreLine>> sorted;
  for (int32_t k = 0; k < 12; ++k) {
    if (order[k] < 0) continue;
    for (auto& u : g.score_lines)
      if (u && u.get() == lines[order[k]]) sorted.push_back(std::move(u));
  }
  g.score_lines = std::move(sorted);
  int32_t* o = out;
  if (winner) g.winner_set = static_cast<uint8_t>(in[1]);
  *o++ = (winner ? g.round_winner() : g.tally(in[1] != 0)) ? 1 : 0;
  for (const auto& r : g.lobby_players)
    *o++ = r->rounds, *o++ = r->place, *o++ = r->score->score, *o++ = static_cast<int32_t>(r->score->key),
    *o++ = r->score->rank;
  for (const auto& r : g.lobby_teams)
    *o++ = r->rounds, *o++ = r->place, *o++ = r->score->score, *o++ = static_cast<int32_t>(r->score->key),
    *o++ = r->score->rank, *o++ = r->kills;
  mech3::multiplayer_setup().team_play = 0;
  return static_cast<int32_t>(o - out);
}

extern "C" int32_t probe_pm_tally(const int32_t* in, int32_t* out) { return tally_run(in, out, false); }
extern "C" int32_t probe_pm_round_winner(const int32_t* in, int32_t* out) { return tally_run(in, out, true); }
