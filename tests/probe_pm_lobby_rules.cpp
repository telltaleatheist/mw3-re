// Pirate's Moon's lobby rules for test_pm_net_score.py (piece 2 of the multiplayer group, pm-tests.md).
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <memory>
#include <vector>

#include "gamez/zreader/zreader.h"
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
  g.kind = in[2], g.round_end = in[3], g.base_lost = in[4], g.x94 = in[5];
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

// Pirate's Moon's rounds reader (0x00438380) for test_pm_rounds.py: the tree read from a binary node file (as the
// archive holds mprounds.zrd), the fallback block's name and the round. out: the table's bits, then the salvage
// blocks handed on (each: its node's flattened strings, ints and floats' bits, -1 between).
namespace {
std::vector<int32_t>* g_rr;
void flatten(const zrdr::Node& n) {
  if (n.type == zrdr::List) {
    for (int32_t k = 1; k < n.list[0].i; ++k) flatten(n.list[k]);
  } else if (n.type == zrdr::String) {
    for (const char* c = n.s; *c; ++c) g_rr->push_back(static_cast<unsigned char>(*c));
    g_rr->push_back(0);
  } else {
    g_rr->push_back(n.i);
  }
}
}  // namespace

extern "C" int32_t probe_pm_round_rules(const char* path, const char* fallback, int32_t round, int32_t* out,
                                        int32_t cap) {
  std::vector<int32_t> lg;
  g_rr = &lg;
  std::FILE* f = std::fopen(path, "rb");
  if (!f) return -1;
  zrdr::Node root{};
  zrdr::read_node(f, &root);
  std::fclose(f);
  auto* saved = mech3::round_salvage;
  mech3::round_salvage = [](mech3::Store&, zrdr::Node* n) {
    flatten(*n);
    g_rr->push_back(-1);
  };
  mech3::RoundRules r;
  mech3::read_round_rules(r, &root, fallback, round);
  mech3::round_salvage = saved;
  zrdr::free_contents(&root);
  std::vector<int32_t> head(8);
  std::memcpy(head.data(), r.table, 32);
  lg.insert(lg.begin(), head.begin(), head.end());
  const int32_t m = static_cast<int32_t>(std::min<size_t>(lg.size(), static_cast<size_t>(cap)));
  std::copy(lg.begin(), lg.begin() + m, out);
  return m;
}

// Pirate's Moon's between rounds (0x00438580) for test_pm_rounds.py. in: place, round, players, REALLOC (+0x70), the
// rounds (+0x74), the tons (+0x80), the bonus (+0xd0, bits), the rules' table (8, bits), the salvage's mechs (count,
// 4 ids), the store (count, then 10 of: kind, id). Logged: the rules read (5, round), the salvage added (6, count);
// then 90, the bonus's bits, the tons, the store's items (kind, id) in order.
namespace {
const int32_t* g_nr;
}  // namespace

extern "C" int32_t probe_pm_next_round(const int32_t* in, int32_t* out, int32_t cap) {
  std::vector<int32_t> lg;
  g_rr = &lg;
  g_nr = in;
  mech3::MultiplayerGame g(nullptr);
  for (int32_t k = 0; k < in[2]; ++k) g.lobby_players.push_back(std::make_unique<mech3::LobbyPlayer>());
  g.realloc = in[3], g.rounds_total = in[4], g.x78 = in[5];
  std::memcpy(&g.bonus_tons, &in[6], 4);
  for (int32_t k = 0; k < in[17]; ++k) {
    mech3::ConfigItem* it;
    if (in[18 + 2 * k] == 0) {
      auto* b = new mech3::BaseItem;
      b->value = in[19 + 2 * k];
      it = b;
    } else if (in[18 + 2 * k] == 1) {
      auto* a = new mech3::WeaponItem;
      a->set_id(in[19 + 2 * k]);
      it = a;
    } else {
      auto* a = new mech3::AmmoItem;
      a->set_id(in[19 + 2 * k]);
      it = a;
    }
    it->count = 1;
    g.round_store.items.push_back(it);
  }
  auto* saved = mech3::round_rules_read;
  mech3::round_rules_read = [](mech3::RoundRules& r, const char*, int32_t round) {
    g_rr->push_back(5), g_rr->push_back(round);
    std::memcpy(r.table, &g_nr[7], 32);
    for (int32_t k = 0; k < g_nr[15]; ++k) r.salvage.add_mech(g_nr[16] + k);
    if (g_nr[15]) g_rr->push_back(6), g_rr->push_back(g_nr[15]);
  };
  g.next_round(static_cast<uint32_t>(in[0]), in[1]);
  mech3::round_rules_read = saved;
  int32_t b;
  std::memcpy(&b, &g.bonus_tons, 4);
  lg.push_back(90), lg.push_back(b), lg.push_back(g.x78);
  for (const mech3::ConfigItem* it : g.round_store.items) lg.push_back(it->kind()), lg.push_back(it->id());
  const int32_t m = static_cast<int32_t>(std::min<size_t>(lg.size(), static_cast<size_t>(cap)));
  std::copy(lg.begin(), lg.begin() + m, out);
  return m;
}

// Pirate's Moon's end of a round's lines (0x0049b730, with the score lines 0x00436db0) for test_pm_round_lines.py.
// in: team play, rounds played, extended, over, the saved total, the rounds (+0x74); the players (count, then 8 of:
// id, team, order, kills, deaths, rounds, score, key), the teams (count, then 4 of: id, rounds, score, key), the
// lines' order (12). Names: "P<id>", "T<id>"; message 0x2bd "M2BD". out: each line's bytes and 0, then -1, the most
// rounds won and how many have it.
namespace {
struct RoundLinesHooks : mech3::PilotHooks {
  const char* message(int32_t id) override { return id == 0x2bd ? "M2BD" : "?"; }
};
}  // namespace

extern "C" int32_t probe_pm_round_lines(const int32_t* in, int32_t* out, int32_t cap) {
  mech3::multiplayer_setup().team_play = in[0];
  RoundLinesHooks hooks;
  mech3::PilotHooks& old = mech3::pilot_hooks();
  mech3::set_pilot_hooks(&hooks);
  mech3::MultiplayerGame g(nullptr);
  g.rounds_total = in[5];
  std::vector<mech3::ScoreLine*> lines(12, nullptr);
  const int32_t* p = in + 7;
  for (int32_t k = 0; k < in[6]; ++k, p += 8) {
    auto r = std::make_unique<mech3::LobbyPlayer>();
    r->id = static_cast<uint32_t>(p[0]), r->team = static_cast<uint32_t>(p[1]), r->order = p[2];
    r->kills = p[3], r->deaths = p[4], r->rounds = p[5], r->name = "P" + std::to_string(p[0]);
    auto l = std::make_unique<mech3::ScoreLine>();
    l->kind = 1, l->score = p[6], l->key = static_cast<uint32_t>(p[7]);
    r->score = lines[k] = l.get();
    g.score_lines.push_back(std::move(l));
    g.lobby_players.push_back(std::move(r));
  }
  const int32_t tbase = 7 + 8 * 8;
  const int32_t* t = in + tbase + 1;
  for (int32_t k = 0; k < in[tbase]; ++k, t += 4) {
    auto r = std::make_unique<mech3::LobbyTeam>();
    r->id = static_cast<uint32_t>(t[0]), r->rounds = t[1], r->name = "T" + std::to_string(t[0]);
    auto l = std::make_unique<mech3::ScoreLine>();
    l->kind = 2, l->score = t[2], l->key = static_cast<uint32_t>(t[3]);
    r->score = lines[8 + k] = l.get();
    g.score_lines.push_back(std::move(l));
    g.lobby_teams.push_back(std::move(r));
  }
  const int32_t* order = in + tbase + 1 + 4 * 4;
  std::vector<std::unique_ptr<mech3::ScoreLine>> sorted;
  for (int32_t k = 0; k < 12; ++k) {
    if (order[k] < 0) continue;
    for (auto& u : g.score_lines)
      if (u && u.get() == lines[order[k]]) sorted.push_back(std::move(u));
  }
  g.score_lines = std::move(sorted);
  std::vector<int32_t> lg;
  for (const std::string& s : g.round_lines(in[1], in[2] != 0, in[3] != 0, in[4])) {
    for (unsigned char c : s) lg.push_back(c);
    lg.push_back(0);
  }
  lg.push_back(-1), lg.push_back(g.best), lg.push_back(g.best_count);
  mech3::set_pilot_hooks(&old);
  mech3::multiplayer_setup().team_play = 0;
  const int32_t m = static_cast<int32_t>(std::min<size_t>(lg.size(), static_cast<size_t>(cap)));
  std::copy(lg.begin(), lg.begin() + m, out);
  return m;
}

// Pirate's Moon's game kinds' queries (piece 4a) for test_pm_game_kinds.py. op 1 the spawn base (0x00437a40): in
// kind, +0xb0, the player, the player listed, its team, the team listed, its third word. op 2 the base check
// (0x00437920): kind, +0xb0, the base. op 3 the base destroyed (0x00472b40): kind, +0xb0, the base, now (bits), the
// reason, the base lost, the clock (bits) before; out the reason, the base lost, the clock's bits. op 4 the jets'
// assist (vt+0x44): the multiplayer game current, its +0xa7, the MW2 option.
#include "mech3/options.h"
extern "C" int32_t probe_pm_game_kind(int32_t op, const int32_t* in, int32_t* out) {
  mech3::MultiplayerGame g(nullptr);
  g.kind = in[0], g.x94 = in[1];
  switch (op) {
    case 1: {
      if (in[3]) {
        auto r = std::make_unique<mech3::LobbyPlayer>();
        r->id = static_cast<uint32_t>(in[2]), r->team = static_cast<uint32_t>(in[4]);
        g.lobby_players.push_back(std::move(r));
      }
      if (in[5]) {
        auto t = std::make_unique<mech3::LobbyTeam>();
        t->id = static_cast<uint32_t>(in[4]), t->word2 = in[6];
        g.lobby_teams.push_back(std::move(t));
      }
      out[0] = g.spawn_base(static_cast<uint32_t>(in[2]));
      return 1;
    }
    case 2: out[0] = g.base_counts(in[2]) ? 1 : 0; return 1;
    case 3: {
      float now, clock;
      std::memcpy(&now, &in[3], 4), std::memcpy(&clock, &in[6], 4);
      g.round_end = in[4], g.base_lost = in[5], g.clock_next = clock;
      g.base_destroyed(in[2], now);
      out[0] = g.round_end, out[1] = g.base_lost;
      std::memcpy(&out[2], &g.clock_next, 4);
      return 3;
    }
    case 4: {
      g.jets_assist = static_cast<uint8_t>(in[1]);
      const uint32_t saved = mech3::options().game_options;
      mech3::set_mw2_jump_jets(in[2]);
      out[0] = mech3::jets_assist_of(in[0] ? &g : nullptr) ? 1 : 0;
      mech3::set_game_options(saved);
      return 1;
    }
  }
  return 0;
}

// Pirate's Moon's game kinds' helpers (piece 4b) for test_pm_kind_helpers.py. in: op, team play, the player asked,
// +0xb0, the scenario (+0xac), best (+0xbc); the players (count, then 8 of: id, team, order, rounds, place), the
// teams (count, then 4 of: id, number, rounds). out per op: 1 the objectives key's bytes; 2 the scenario's text id;
// 3 the enemies; 4 the team's round (the count, then each player's rounds); 5 the runner-up's rounds; 6 rounds_of;
// 7 place_of; 8 net_team; 9 the team with that number (its id, 0 none).
extern "C" int32_t probe_pm_kind_helpers(const int32_t* in, int32_t* out) {
  mech3::multiplayer_setup().team_play = in[1];
  mech3::MultiplayerGame g(nullptr);
  g.x94 = in[3], g.x90 = in[4], g.best = in[5];
  const int32_t* p = in + 7;
  for (int32_t k = 0; k < in[6]; ++k, p += 5) {
    auto r = std::make_unique<mech3::LobbyPlayer>();
    r->id = static_cast<uint32_t>(p[0]), r->team = static_cast<uint32_t>(p[1]), r->order = p[2];
    r->rounds = p[3], r->place = p[4];
    g.lobby_players.push_back(std::move(r));
  }
  const int32_t* t = in + 7 + 8 * 5 + 1;
  for (int32_t k = 0; k < in[7 + 8 * 5]; ++k, t += 3) {
    auto r = std::make_unique<mech3::LobbyTeam>();
    r->id = static_cast<uint32_t>(t[0]), r->word2 = t[1], r->rounds = t[2];
    g.lobby_teams.push_back(std::move(r));
  }
  const uint32_t who = static_cast<uint32_t>(in[2]);
  int32_t* o = out;
  switch (in[0]) {
    case 1:
      for (unsigned char c : g.objectives_key()) *o++ = c;
      break;
    case 2: *o++ = g.scenario_message(); break;
    case 3: *o++ = g.enemies_of(who); break;
    case 4:
      *o++ = g.award_team(who);
      for (auto& r : g.lobby_players) *o++ = r->rounds;
      break;
    case 5: *o++ = g.runner_up_rounds(); break;
    case 6: *o++ = g.rounds_of(who); break;
    case 7: *o++ = g.place_of(who); break;
    case 8: *o++ = static_cast<int32_t>(g.net_team(who)); break;
    case 9: {
      const mech3::LobbyTeam* r = g.team_numbered(in[2]);
      *o++ = r ? static_cast<int32_t>(r->id) : 0;
      break;
    }
  }
  mech3::multiplayer_setup().team_play = 0;
  return static_cast<int32_t>(o - out);
}

// Pirate's Moon's lobby settings (piece 4d) for test_pm_lobby_settings.py. in: op; the picks (kills, minutes, tons,
// realloc, rounds, kind, world: has, value each), the boxes (respawn, ammo, outside, jets, radar, stock, mobile), the
// rules' flags before; for op 2 the players (count, then 8 of: id, team, order) and the teams (count, then 4 of: id,
// number) and team play; for op 3 the two confirmations' answers. out: op 1 the game's fields and the clock; op 2 the
// 0x1e's words; op 3 the rule, the confirmations asked, team play.
namespace {
const int32_t* g_ls;
std::vector<int32_t>* g_ls_log;
}  // namespace

extern "C" int32_t probe_pm_lobby_settings(const int32_t* in, int32_t* out) {
  g_ls = in;
  mech3::LobbySettings s;
  mech3::LobbyPick* picks[7] = {&s.kills, &s.minutes, &s.tons, &s.realloc, &s.rounds, &s.kind, &s.world};
  for (int k = 0; k < 7; ++k) picks[k]->has = in[1 + 2 * k] != 0, picks[k]->value = in[2 + 2 * k];
  bool* boxes[7] = {&s.respawn, &s.ammo, &s.outside, &s.jets, &s.radar, &s.stock, &s.mobile};
  for (int k = 0; k < 7; ++k) *boxes[k] = in[15 + k] != 0;
  mech3::MultiplayerGame g(nullptr);
  g.rules.flags = static_cast<uint32_t>(in[22]);
  std::vector<int32_t> lg;
  switch (in[0]) {
    case 1: {
      bool on = false;
      float left = -1;
      g.apply_lobby(s, &on, &left);
      float x74 = g.x74;
      int32_t b74, bl;
      std::memcpy(&b74, &x74, 4), std::memcpy(&bl, &left, 4);
      lg = {g.x70, g.b8c, g.b8d, g.b8e, g.jets_assist, g.radar, g.b8f, g.ba8, g.x78, b74, g.realloc, g.rounds_total,
            g.kind, g.rules.world, g.rules.kills, g.rules.time, g.rules.weight, static_cast<int32_t>(g.rules.flags),
            on ? 1 : 0, bl, g.x90, g.x94};
      break;
    }
    case 2: {
      mech3::multiplayer_setup().team_play = in[23];
      const int32_t* p = in + 25;
      for (int32_t k = 0; k < in[24]; ++k, p += 3) {
        auto r = std::make_unique<mech3::LobbyPlayer>();
        r->id = static_cast<uint32_t>(p[0]), r->team = static_cast<uint32_t>(p[1]), r->order = p[2];
        g.lobby_players.push_back(std::move(r));
      }
      const int32_t* t = in + 25 + 8 * 3 + 1;
      for (int32_t k = 0; k < in[25 + 8 * 3]; ++k, t += 2) {
        auto r = std::make_unique<mech3::LobbyTeam>();
        r->id = static_cast<uint32_t>(t[0]), r->word2 = t[1];
        g.lobby_teams.push_back(std::move(r));
      }
      const std::vector<uint8_t> b = g.start_message();
      lg.resize(b.size() / 4);
      std::memcpy(lg.data(), b.data(), b.size());
      mech3::multiplayer_setup().team_play = 0;
      break;
    }
    case 3: {
      g_ls_log = &lg;
      bool team = false;
      const mech3::LobbyRule r = mech3::check_lobby_rules(s, [](int32_t text) {
        g_ls_log->push_back(7), g_ls_log->push_back(text);
        return g_ls[text == 0xfbd ? 23 : 24] != 0;
      }, &team);
      lg.push_back(90), lg.push_back(static_cast<int32_t>(r)), lg.push_back(team ? 1 : 0);
      break;
    }
  }
  std::copy(lg.begin(), lg.end(), out);
  return static_cast<int32_t>(lg.size());
}

// Pirate's Moon's target killed (0x004e2f50) for test_pm_lobby_settings.py's base part. in: health (bits), the
// base (mpoption), active. out: the base told (-9 none), destroyed, active, health's bits.
#include "mech3/zai_target.h"
#include "mech3/zrdraiv.h"
namespace {
int32_t g_base_told;
struct BaseHooks : mech3::ai::TargetHooks {
  void entity_init(mech3::ai::Handle) override {}
  int32_t node_box(mech3::ai::Handle, float* box) override {
    for (int k = 0; k < 6; ++k) box[k] = 0;
    return 1;
  }
  void base_destroyed(int32_t base) override { g_base_told = base; }
};
}  // namespace

extern "C" void probe_pm_target_kill(const int32_t* in, int32_t* out) {
  BaseHooks hooks;
  mech3::ai::set_target_hooks(&hooks);
  mech3::ai::TargetRec rec;
  rec.height = 5.0f;
  mech3::ai::TargetDesc d;
  std::memcpy(&d.health, &in[0], 4);
  d.mpoption = in[1];
  auto* t = new mech3::ai::CZAiTarget(1, 0, d, &rec, false);
  t->active = static_cast<uint8_t>(in[2]);
  g_base_told = -9;
  t->kill();
  out[0] = g_base_told, out[1] = t->destroyed, out[2] = t->active;
  std::memcpy(&out[3], &t->desc.health, 4);
  delete t;
  mech3::ai::set_target_hooks(nullptr);
}

// Pirate's Moon's lobby lists' lines (0x0049bb70) for test_pm_lobby_lines.py. in: team play, the kinds list's value,
// our id, our slot's mech; the players (count, then 8 of: id, team, host, state, skin), the teams (count, then 4 of:
// id, skin, number). Names "P<id>" and "T<id>", texts "M<id in hex>", skins "S<n>". out: TP_LIST's lines then
// PLAYERS_LIST's, each its bytes, 0 and its record (a player's id, a team's id + 1000), -1 between and after; then
// whether everyone is ready.
namespace {
const char* lines_message(int32_t id) {
  static std::string s;
  char b[16];
  std::snprintf(b, sizeof b, "M%x", static_cast<unsigned>(id));
  s = b;
  return s.c_str();
}
std::string lines_skin(int32_t skin) { return "S" + std::to_string(skin); }
}  // namespace

extern "C" int32_t probe_pm_lobby_lines(const int32_t* in, int32_t* out, int32_t cap) {
  mech3::multiplayer_setup().team_play = in[0];
  mech3::MultiplayerGame g(nullptr);
  g.slot.mech = in[3];
  const int32_t* p = in + 5;
  for (int32_t k = 0; k < in[4]; ++k, p += 5) {
    auto r = std::make_unique<mech3::LobbyPlayer>();
    r->id = static_cast<uint32_t>(p[0]), r->team = static_cast<uint32_t>(p[1]), r->host = static_cast<uint8_t>(p[2]);
    r->state = p[3], r->skin = p[4], r->name = "P" + std::to_string(p[0]);
    g.lobby_players.push_back(std::move(r));
  }
  const int32_t* t = in + 5 + 8 * 5 + 1;
  for (int32_t k = 0; k < in[5 + 8 * 5]; ++k, t += 3) {
    auto r = std::make_unique<mech3::LobbyTeam>();
    r->id = static_cast<uint32_t>(t[0]), r->skin = t[1], r->word2 = t[2], r->name = "T" + std::to_string(t[0]);
    g.lobby_teams.push_back(std::move(r));
  }
  mech3::LobbyText tx;
  tx.message = lines_message, tx.skin = lines_skin;
  std::vector<mech3::LobbyLine> teams, players;
  const bool ready = g.lobby_lines(in[1], static_cast<uint32_t>(in[2]), tx, &teams, &players);
  std::vector<int32_t> lg;
  for (auto* list : {&teams, &players}) {
    for (const mech3::LobbyLine& l : *list) {
      for (unsigned char c : l.text) lg.push_back(c);
      lg.push_back(0);
      lg.push_back(static_cast<int32_t>(l.entry->id) + (l.entry->kind() == 2 ? 1000 : 0));
    }
    lg.push_back(-1);
  }
  lg.push_back(ready ? 1 : 0);
  mech3::multiplayer_setup().team_play = 0;
  const int32_t m = static_cast<int32_t>(std::min<size_t>(lg.size(), static_cast<size_t>(cap)));
  std::copy(lg.begin(), lg.begin() + m, out);
  return m;
}

// Pirate's Moon's player record from its data (0x0049c650) for test_pm_lobby_lines.py's second part. in: between
// rounds, the data's word count (0: none), its words (8). out: state, skin, rounds, place.
extern "C" void probe_pm_player_data(const int32_t* in, int32_t* out) {
  std::vector<uint32_t> d(in + 2, in + 2 + in[1]);
  mech3::LobbyPlayer p;
  p.from_data(d, in[0] != 0);
  out[0] = p.state, out[1] = p.skin, out[2] = p.rounds, out[3] = p.place;
}

// Pirate's Moon's lobby rounds on entering (0x0049a3c0) for test_pm_lobby_enter.py. in: from a game, played, our id,
// host, team play, the rounds (+0x74); the players (count, then 8 of: id, team, kills, deaths, rounds, place), the
// teams (count, then 4 of: id, rounds, number). out: extended, over, over total, next, the rounds, played, team play
// after, our place and rounds, the most rounds and how many have it; then each team's end (its id, kept).
namespace {
std::vector<int32_t>* g_er;
}  // namespace

extern "C" int32_t probe_pm_enter_rounds(const int32_t* in, int32_t* out, int32_t cap) {
  mech3::set_multiplayer_team_play(in[4] != 0);
  mech3::MultiplayerGame g(nullptr);
  g.rounds_total = in[5];
  std::vector<std::unique_ptr<mech3::ScoreLine>> keep;
  const int32_t* p = in + 7;
  for (int32_t k = 0; k < in[6]; ++k, p += 6) {
    auto r = std::make_unique<mech3::LobbyPlayer>();
    r->id = static_cast<uint32_t>(p[0]), r->team = static_cast<uint32_t>(p[1]), r->kills = p[2], r->deaths = p[3];
    r->rounds = p[4], r->place = p[5], r->name = "P" + std::to_string(p[0]);
    auto l = std::make_unique<mech3::ScoreLine>();
    l->kind = 1, r->score = l.get();
    g.score_lines.push_back(std::move(l));
    g.lobby_players.push_back(std::move(r));
  }
  const int32_t* t = in + 7 + 8 * 6 + 1;
  for (int32_t k = 0; k < in[7 + 8 * 6]; ++k, t += 3) {
    auto r = std::make_unique<mech3::LobbyTeam>();
    r->id = static_cast<uint32_t>(t[0]), r->rounds = t[1], r->word2 = t[2], r->name = "T" + std::to_string(t[0]);
    auto l = std::make_unique<mech3::ScoreLine>();
    l->kind = 2, r->score = l.get();
    g.score_lines.push_back(std::move(l));
    g.lobby_teams.push_back(std::move(r));
  }
  std::vector<int32_t> lg;
  g_er = &lg;
  std::vector<int32_t> ends;
  static std::vector<int32_t>* s_ends;
  s_ends = &ends;
  int32_t played = in[1];
  const mech3::RoundsEntry r = g.enter_rounds(in[0] != 0, &played, static_cast<uint32_t>(in[2]), in[3] != 0,
                                              [](mech3::LobbyTeam& tm, bool kept) {
                                                s_ends->push_back(static_cast<int32_t>(tm.id)), s_ends->push_back(kept ? 1 : 0);
                                                if (kept) s_ends->push_back(tm.rounds);
                                              });
  lg = {r.extended, r.over, r.over_total, r.next, g.rounds_total, played, mech3::multiplayer_team_play() ? 1 : 0,
        r.place, r.rounds, g.best, g.best_count};
  lg.insert(lg.end(), ends.begin(), ends.end());
  if (r.next) lg.push_back(7), lg.push_back(r.place), lg.push_back(played - 1);  // the screen's next_round
  mech3::set_multiplayer_team_play(false);
  const int32_t m = static_cast<int32_t>(std::min<size_t>(lg.size(), static_cast<size_t>(cap)));
  std::copy(lg.begin(), lg.begin() + m, out);
  return m;
}

// Pirate's Moon's 0x1e read (0x0049f510) for test_pm_lobby_settings.py's fifth part. in: the players (count, then 8
// ids), the message's words (count, then the words). out: each player's order after.
extern "C" void probe_pm_read_start(const int32_t* in, int32_t* out) {
  mech3::MultiplayerGame g(nullptr);
  for (int32_t k = 0; k < in[0]; ++k) {
    auto r = std::make_unique<mech3::LobbyPlayer>();
    r->id = static_cast<uint32_t>(in[1 + k]), r->order = -5;
    g.lobby_players.push_back(std::move(r));
  }
  const int32_t words = in[9];
  std::vector<uint8_t> msg(4 * static_cast<size_t>(words));
  std::memcpy(msg.data(), in + 10, msg.size());
  g.read_start_message(msg.data(), msg.size());
  int32_t k = 0;
  for (auto& r : g.lobby_players) out[k++] = r->order;
}

// Pirate's Moon's rules message 0x20 for test_pm_lobby_rules_msg.py: the lists (kills, minutes, tons, realloc, rounds,
// kind, world: each its count, pick and 12 values), the boxes (respawn, ammo, outside, jets, radar, stock, mobile), the
// lock, the message read, the rand() word. Out: the message sent (6 words), then after the read the picks, the boxes,
// the lock, the first lock and its team play, the block (flags, world, kills, minutes, tons, seed) and its CRC.
extern "C" void probe_pm_lobby_rules_msg(const int32_t* in, int32_t* out) {
  mech3::LobbyControls c;
  mech3::LobbyChoice* lists[7] = {&c.kills, &c.minutes, &c.tons, &c.realloc, &c.rounds, &c.kind, &c.world};
  for (int k = 0; k < 7; ++k) {
    const int32_t* l = in + 14 * k;
    lists[k]->values.assign(l + 2, l + 2 + l[0]);
    lists[k]->pick = l[1];
  }
  bool* boxes[7] = {&c.respawn, &c.ammo, &c.outside, &c.jets, &c.radar, &c.stock, &c.mobile};
  for (int k = 0; k < 7; ++k) *boxes[k] = in[98 + k] != 0;
  c.locked = in[105] != 0;
  const std::vector<uint8_t> m = mech3::lobby_rules_message(c);
  std::memcpy(out, m.data(), m.size());
  uint8_t msg[0x18];
  std::memcpy(msg, in + 106, sizeof msg);
  mech3::MultiplayerGame g(nullptr);
  bool team = in[113] != 0;
  const bool first = g.read_lobby_rules(msg, sizeof msg, &c, in[112], &team);
  for (int k = 0; k < 7; ++k) out[6 + k] = lists[k]->pick;
  for (int k = 0; k < 7; ++k) out[13 + k] = *boxes[k] ? 1 : 0;
  const int32_t r[10] = {c.locked ? 1 : 0, first ? 1 : 0, team ? 1 : 0, static_cast<int32_t>(g.rules.flags),
                         g.rules.world, g.rules.kills, g.rules.time, g.rules.weight, g.rules.seed,
                         static_cast<int32_t>(g.rules_crc)};
  std::memcpy(out + 20, r, sizeof r);
}

// READY's block (0x0049e6f0) for test_pm_lobby_rules_msg.py: the lists, boxes and lock as probe_pm_lobby_rules_msg's,
// in[112] the rand() word. Out: the block (flags, world, kills, minutes, tons, seed) and its CRC.
extern "C" void probe_pm_keep_lobby_rules(const int32_t* in, int32_t* out) {
  mech3::LobbyControls c;
  mech3::LobbyChoice* lists[7] = {&c.kills, &c.minutes, &c.tons, &c.realloc, &c.rounds, &c.kind, &c.world};
  for (int k = 0; k < 7; ++k) {
    const int32_t* l = in + 14 * k;
    lists[k]->values.assign(l + 2, l + 2 + l[0]);
    lists[k]->pick = l[1];
  }
  bool* boxes[7] = {&c.respawn, &c.ammo, &c.outside, &c.jets, &c.radar, &c.stock, &c.mobile};
  for (int k = 0; k < 7; ++k) *boxes[k] = in[98 + k] != 0;
  c.locked = in[105] != 0;
  mech3::MultiplayerGame g(nullptr);
  g.rules.flags = 0xdead, g.rules_crc = 0xbeef;  // whatever was there goes
  g.keep_lobby_rules(c, in[112]);
  const int32_t r[7] = {static_cast<int32_t>(g.rules.flags), g.rules.world, g.rules.kills, g.rules.time,
                        g.rules.weight, g.rules.seed, static_cast<int32_t>(g.rules_crc)};
  std::memcpy(out, r, sizeof r);
}

// A match's start (0x00438550): the store's and the round store's item counts and `full`, the tons, the rounds
// played. Out: the counts and `full` after, the bonus tons' bits.
extern "C" void probe_pm_round_start(const int32_t* in, int32_t* out) {
  mech3::MultiplayerGame g(nullptr);
  g.store.clear(), g.round_store.clear();
  for (int32_t k = 0; k < in[0]; ++k) g.store.add_mech(k);
  for (int32_t k = 0; k < in[2]; ++k) g.round_store.add_mech(k);
  g.store.full = static_cast<uint8_t>(in[1]), g.round_store.full = static_cast<uint8_t>(in[3]);
  g.x78 = in[4];
  std::memcpy(&g.bonus_tons, in + 5, 4);
  g.round_start(in[6]);
  out[0] = g.store.count(), out[1] = g.store.full, out[2] = g.round_store.count(), out[3] = g.round_store.full;
  std::memcpy(out + 4, &g.bonus_tons, 4);
}
