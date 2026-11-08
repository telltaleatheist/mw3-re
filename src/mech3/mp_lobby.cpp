#include "mech3/mech_ids.h"
#include "mech3/mp_lobby.h"
#include "mech3/mission.h"
#include "mech3/mods.h"

#include <cctype>
#include <iterator>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "gamez/znetwork/znet_dplay.h"
#include "gamez/znetwork/znet_session.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zfix.h"
#include "gamez/zutil/ztimer.h"
#include "gamez/zutil/zutl_msgdll.h"
#include "mech3/app_mech.h"
#include "mech3/briefing.h"
#include "mech3/pilot.h"

namespace mech3 {

int32_t mech_name_message(int32_t mech);  // mech3/mech_names.h (its Engine clashes with app_hooks.h's)
float max_mass(int32_t mech);              // the same

uint8_t g_lobby_dirty = 0;  // [0x006099cc]
uint8_t g_lobby_back = 0;   // [0x006099c8]

namespace {
constexpr uint32_t kModsMark = 0x53444f4d;  // "MODS": our block in a player's lobby data (the mods check)
// Pirate's Moon's player data, 7 words (0x0049a3c0); our add-on worlds' count follows it, then their values.
constexpr size_t kPmDataWords = 7;
}  // namespace
int32_t& net_rounds_played();  // mech_net.cpp: [0x0062cc38]
namespace {
LobbyScreen* g_lobby = nullptr;  // [0x006099d4]
int32_t g_order_next = 1;        // [0x005e7920]

void set_labels(ztwod::Control& c, const char* text) {  // 0x00495240
  for (std::vector<ztwod::Text*>* l : {&c.activate_labels, &c.disable_labels, &c.rollover_labels, &c.labels})
    if (!l->empty() && l->front()) l->front()->set_text_plain(text);
}
void set_enabled(ztwod::Control& c, int32_t on) {
  c.enabled = on;
  c.refresh();  // +0x80
}
// A player's data as the lobby reads it (0x00499030, 0x0049ab30): more than
// a word or nothing. Kind 1 (the lobby's): its skin, its state, then the
// add-on worlds it has (their count, the values); the launch's kind 2
// (0x0046da80) replaces it.
std::vector<uint32_t> player_data(uint32_t id) {
  uint32_t size = znet::player_data_size(id);
  if (size <= 7) return {};
  std::vector<uint32_t> words((size + 3) / 4, 0);
  if (!znet::get_player_data(id, words.data(), &size)) return {};
  return words;
}

// PM 0x0049c650 (MW3's 0x00499030): each network player's record, from its
// data (LobbyPlayer::from_data), its place in the count unless kept, its
// score line.
void add_player(uint32_t id, uint32_t team, bool host, const char* name, void*) {
  MultiplayerGame* g = g_lobby ? g_lobby->game : nullptr;
  if (!g) return;
  auto p = std::make_unique<LobbyPlayer>();
  p->id = id, p->team = team, p->host = host ? 1 : 0, p->name = name ? name : "";
  p->from_data(player_data(id), g_lobby->between_rounds != 0);
  if (!g_lobby->keep_order) p->order = g_order_next++;
  auto line = std::make_unique<ScoreLine>();
  line->kind = 1;
  p->score = line.get();
  g->score_lines.push_back(std::move(line));
  g->lobby_players.push_back(std::move(p));
}

// PM 0x0049c880 (MW3's 0x00499280): each network team's record; its data
// (PM 0x14 bytes, MW3's 8) the owner, skin, number and rounds won.
void add_team(uint32_t id, uint32_t, bool, const char* name, void*) {
  MultiplayerGame* g = g_lobby ? g_lobby->game : nullptr;
  if (!g) return;
  auto t = std::make_unique<LobbyTeam>();
  t->id = id, t->name = name ? name : "";
  uint32_t data[5] = {}, size = sizeof data;
  znet::get_team_data(id, data, &size);  // 0x005aa590
  t->owner = data[0], t->skin = static_cast<int32_t>(data[1]);
  t->word2 = static_cast<int32_t>(data[2]), t->rounds = static_cast<int32_t>(data[3]);
  auto line = std::make_unique<ScoreLine>();
  line->kind = 2;
  t->score = line.get();
  g->score_lines.push_back(std::move(line));
  g->lobby_teams.push_back(std::move(t));
}
}  // namespace

// The block (enter's): untrusted, read only as far as the data goes, at most
// 16 names of 64 bytes, control characters '?'. A player without one (no
// mods block): hash 0.
bool LobbyScreen::mods_of(const std::vector<uint32_t>& d, uint64_t* hash, std::vector<std::string>* names) {
  *hash = 0;
  names->clear();
  if (d.size() <= kPmDataWords || d[0] != 1) return false;
  const size_t worlds = d[kPmDataWords] < 64 ? d[kPmDataWords] : 64;
  size_t at = kPmDataWords + 1 + worlds;
  if (at + 4 > d.size() || d[at] != kModsMark) return true;
  *hash = static_cast<uint64_t>(d[at + 1]) | static_cast<uint64_t>(d[at + 2]) << 32;
  const uint32_t count = std::min<uint32_t>(d[at + 3], 16);
  at += 4;
  for (uint32_t k = 0; k < count && at < d.size(); ++k) {
    const uint32_t len = std::min<uint32_t>(d[at++], 64);
    std::string n;
    for (uint32_t b = 0; b < len && at + b / 4 < d.size(); ++b) {
      const char c = static_cast<char>(d[at + b / 4] >> (8 * (b % 4)) & 0xff);
      n += static_cast<unsigned char>(c) < 0x20 || c == 0x7f ? '?' : c;
    }
    at += (len + 3) / 4;
    names->push_back(n);
  }
  return true;
}

std::string LobbyScreen::mods_text(const std::vector<std::string>& names) {
  if (names.empty()) return "no gameplay mods";
  std::string t;
  for (const std::string& n : names) t += (t.empty() ? "" : ", ") + n;
  return t;
}

LobbyScreen* lobby_screen() { return g_lobby; }

// ---- the controls ----

// Mech3.exe 1.2 @ 0x00496880
void LobbyList::empty() {
  clear_items();
  records.clear();
  layout();  // +0xa4
}

void LobbyList::add(const std::string& text, LobbyEntry* e) {
  add_item(text.c_str(), static_cast<int32_t>(records.size()));
  records.push_back(e);
  layout();
}

LobbyEntry* LobbyList::chosen() {
  if (selected < 0 || selected >= item_count()) return nullptr;
  const int32_t v = items[static_cast<size_t>(selected)]->value;
  return v >= 0 && static_cast<size_t>(v) < records.size() ? records[static_cast<size_t>(v)] : nullptr;
}

// ---- the screen ----

// Mech3.exe 1.2 @ 0x00495f90
LobbyScreen::LobbyScreen(Engine& e, Handle s)
    : MenuScreen(e, s), chat_edit(e, this), cancel_btn(e, 1), start_btn(e, 0), mechlab_btn(e, 10), salvage_btn(e, 0xb),
      options_btn(e, 0xc), lock_btn(e, 0xd), game_type(e, 0xe), worlds(e, 0xf), salvage_award(e, 0x10),
      rounds_list(e, 0x11), kills_list(e, 0x12), time_list(e, 0x13), weight_list(e, 0x14), join_team_btn(e, 4),
      remove_team_btn(e, 5), kick_btn(e, 6), new_team_btn(e, 7), pick_skin_btn(e, 8), kick2_btn(e, 9),
      chat_log(e, this), scores_list(e, -1), teams_list(e, -1), players_list(e, 0x1e), player_name(e, -1),
      session_name(e, -1), internet_line(e), lan_line(e) {
  chat_edit.set_size(0x27);
  regen_box.id = 0x15, mobile_box.id = 0x16, stock_box.id = 0x17, ammo_box.id = 0x18, select_box.id = 0x19;
  jets_box.id = 0x1a, radar_box.id = 0x1b;
  ready_box.id = 0x1c;
  const bool moving = !e.fast_menu();
  if (load_from(e.main_menu_definition(), "MULTIPLAYER_HOST_DIALOG", moving)) {
    zimg::Texture* img = nullptr;
    uint8_t bg_moving = 0;
    e.main_menu_background(&img, &bg_moving);
    background.set_image(img);
    background.moving = bg_moving;
    if (!bg_moving) {
      background.set_pos(0, 0);
      background.src = &background.view;
    }
    add(&background);
    bind_primitive(&lines1, "LINES1");
    if (!moving) {
      bake();
      remove(&background);
      remove(&lines1);
    }
    bind_button(&session_name, "SESSION_NAME");
    bind_button(&cancel_btn, "CANCEL_BUTTON");  // PM: CANCEL_ZONE when started from a DirectPlay lobby (none here)
    bind_button(&start_btn, "START_BUTTON");
    bind_button(&mechlab_btn, "MECHLAB_BUTTON");
    bind_button(&salvage_btn, "SALVAGE_BUTTON");
    bind_button(&options_btn, "OPTIONS_BUTTON");
    bind_button(&lock_btn, "LOCK_OPTIONS_BUTTON");
    bind_button(&game_type, "GAME_TYPE");
    bind_button(&worlds, "DATABASE_LIST");
    bind_button(&salvage_award, "SALVAGE_AWARD");
    bind_button(&rounds_list, "ROUNDS_LIST");
    bind_button(&kills_list, "KILLS_LIST");
    bind_button(&time_list, "TIME_LIST");
    bind_button(&weight_list, "WEIGHT_LIST");
    bind_button(&regen_box, "REGEN_CHECKBOX");
    bind_button(&mobile_box, "MOBILE_CHECKBOX");
    bind_button(&stock_box, "STOCK_CHECKBOX");
    bind_button(&ammo_box, "AMMO_CHECKBOX");
    bind_button(&select_box, "SELECT_CHECKBOX");
    bind_button(&jets_box, "JETS_CHECKBOX");
    bind_button(&radar_box, "RADAR_CHECKBOX");
    bind_button(&player_name, "PLAYER_NAME");
    bind_button(&chat_edit, "CHAT_ENTER_EDIT");
    bind_button(&chat_team, "CHAT_TEAM_CHECKBOX");
    bind_button(&join_team_btn, "JOIN_TEAM_BUTTON");
    bind_button(&remove_team_btn, "REMOVE_TEAM_BUTTON");
    bind_button(&kick_btn, "KICK_PLAYER_BUTTON");
    bind_button(&new_team_btn, "NEW_TEAM_BUTTON");
    bind_button(&pick_skin_btn, "PICK_SKIN_BUTTON");
    bind_button(&kick2_btn, "KICK_PLAYER2_BUTTON");
    bind_button(&scores_list, "SCORES_LIST");
    bind_button(&teams_list, "TP_LIST");
    bind_button(&players_list, "PLAYERS_LIST");
    bind_button(&ready_box, "READY_CHECKBOX");
    bind_primitive(&chat_log, "CHAT_REC_LIST");
    bind_button(&chat_log.up_btn, "CHAT_UP");
    bind_button(&chat_log.down_btn, "CHAT_DOWN");
    teams_list.id = 0x1d;
  }
  // Ours (piece 9c): the host's two address lines, under CANCEL and LOCK.
  const Handle font = ztwod::host().font_find("ia_small_green");
  int32_t y = 460;
  for (CopyLine* l : {&internet_line, &lan_line}) {
    if (font) l->FontText::set_font(font);
    l->set_pos(20, y);
    y += 10;
    add(l);
    l->hide();
  }
  set_flags_all(0);
  set_active(1);
  g_lobby = this;
}

// Mech3.exe 1.2 @ 0x00496b90
LobbyScreen::~LobbyScreen() {
  if (handlers_on_) handlers(false);
  if (g_lobby == this) g_lobby = nullptr;
}

void LobbyScreen::take_pilot(Handle p, MultiplayerGame* g) {
  pilot = p;
  game = g;
}

// Mech3.exe 1.2 @ 0x00497720: the names; the scores; the local player's data
// made (kind 1: its skin, the first free one in a game without teams, and
// each add-on world it has, multiplayer-port.md 11); the rules' lists filled
// and set from the game; the lists and buttons cleared; the handlers; the
// game open again (its fourth word 1); the network's players and teams.
void LobbyScreen::enter(bool from_game) {
  Engine& e = engine();
  if (!game) return;
  const uint32_t me = znet::local_id();
  const char* my_name = znet::player_name(me);
  set_labels(player_name, my_name && *my_name ? my_name : "Unknown");  // [0x005e56f0]
  const std::string& game_name = znet::session().desc.name;
  set_labels(session_name, game_name.empty() ? "Some Game" : game_name.c_str());  // [0x005e7c30]
  scores(true);
  // Pirate's Moon 0x0049a3c0's rounds: extended, over, the next round or the match's end (the results list's lines).
  const RoundsEntry rounds = game->enter_rounds(from_game, &net_rounds_played(), me, znet::is_host(),
                                                [](LobbyTeam& t, bool kept) {
                                                  if (kept) {  // its data's fourth word its rounds (0x005aa550)
                                                    uint32_t d[5] = {}, size = sizeof d;
                                                    znet::get_team_data(t.id, d, &size);
                                                    d[3] = static_cast<uint32_t>(t.rounds);
                                                    znet::set_team_data(t.id, d, sizeof d);
                                                    return;
                                                  }
                                                  if (MultiplayerGame* g = g_lobby ? g_lobby->game : nullptr)
                                                    for (auto& p : g->lobby_players)  // 0x005a9480
                                                      if (p->team == t.id) znet::remove_from_team(t.id, p->id);
                                                  znet::destroy_team(t.id);  // 0x005a94f0
                                                });
  between_rounds = rounds.next;  // +0x1679d
  locked = rounds.next;          // +0x16799: the next round's options stay locked
  results = rounds.lines;        // the results list's (+0x15e14; piece 6c shows it)
  // PM's data (0x1c bytes): kind 1, the skin, the state, 0, the rounds won and place (between rounds), 0. Ours after
  // it: the add-on worlds (their count, the values) and the mods block (MW3's had the worlds from its fourth word).
  std::vector<uint32_t> data(kPmDataWords + 1, 0);
  data[0] = 1;
  data[1] = static_cast<uint32_t>(game->net_skin(me));
  data[4] = static_cast<uint32_t>(rounds.rounds), data[5] = static_cast<uint32_t>(rounds.place);
  keep_order = 0;
  if (rounds.next) game->next_round(static_cast<uint32_t>(rounds.place), net_rounds_played() - 1);  // 0x00438580
  refresh_lists();
  const bool team = multiplayer_team_play();
  if (data[1] == 0 && !team)
    for (int32_t i = 0; i < 8; ++i)
      if (free_skin(i + 1, -1) == 0) {
        data[1] = static_cast<uint32_t>(i + 1);
        break;
      }
  game_type.clear_items();  // PM 0x0049a951: the kinds
  game_type.add_item("Deathmatch without Teams", 0);
  game_type.add_item("Deathmatch with Teams", 1);
  game_type.add_item("Attack/Defend - One Base", 3);
  game_type.add_item("Attack/Defend - Two Bases", 4);
  worlds.clear_items();
  // PM 0x0049a9a6: the list starts at the shipped worlds, MW3's "UNAVAILABLE" row (0xfdf) is gone.
  worlds.add_item(e.message(0x320), 0x100);  // the shipped eight: (number << 8) | folder
  static const int32_t kShipped[7][2] = {{0, 2}, {1, 1}, {1, 2}, {2, 1}, {2, 2}, {3, 1}, {3, 2}};
  for (int32_t i = 0; i < 7; ++i) worlds.add_item(e.message(0x321 + i), kShipped[i][1] << 8 | kShipped[i][0]);
  // The add-on worlds (readermp3 on): each folder's numbered readers, named
  // by their mpinfo.zrd's db_name, recorded in the data. Ours: on past the
  // exe's six to the mods' worlds (9 on), t1, lab and dev1/dev2 still left out.
  for (int32_t dir = 0; dir < mission_dir_count(); ++dir) {
    if (dir >= 6 && dir < 9) continue;
    const std::string folder = e.mission_dir(static_cast<uint32_t>(dir));
    for (const std::string& file : e.list_files(("zbd\\" + folder + "\\readermp*.zbd").c_str())) {
      if (file.size() < 8) continue;
      const int32_t n = std::atoi(file.substr(8, file.find('.') == std::string::npos ? std::string::npos : file.find('.') - 8).c_str());
      if (n <= 2) continue;
      char path[160];
      std::snprintf(path, sizeof path, "zbd\\%s\\readermp%d.zbd", folder.c_str(), n);
      if (!zrdr::open_archive(path, false)) continue;
      zrdr::Node* info = zrdr::load("mpinfo.zrd");
      if (const char* s = zrdr::get_string(info, "db_name")) {
        const int32_t v = n << 8 | dir;
        worlds.add_item(s, v);
        data[kPmDataWords] += 1;  // 0x00498160 (MW3's)
        data.resize(kPmDataWords + 1 + data[kPmDataWords]);
        data[kPmDataWords + data[kPmDataWords]] = static_cast<uint32_t>(v);
      }
      if (info) zrdr::free_node(info);  // FIX (bug): the exe kept each tree (a leak per entry)
      zrdr::close_archives(false);
    }
  }
  // Ours (the mods check): after the add-on worlds, "MODS", the gameplay
  // hash (low, high), the gameplay mods' names (a count, then each one's
  // length and its bytes, four a word). The exe's readers stop at the worlds.
  {
    const ModsIdentity& mods = mods_identity();
    data.push_back(kModsMark);
    data.push_back(static_cast<uint32_t>(mods.hash));
    data.push_back(static_cast<uint32_t>(mods.hash >> 32));
    data.push_back(static_cast<uint32_t>(std::min<size_t>(mods.names.size(), 16)));
    for (size_t i = 0; i < mods.names.size() && i < 16; ++i) {
      const std::string cut = mods.names[i].substr(0, 64);
      data.push_back(static_cast<uint32_t>(cut.size()));
      for (size_t k = 0; k < cut.size(); k += 4) {
        uint32_t w = 0;
        for (size_t b = 0; b < 4 && k + b < cut.size(); ++b) w |= static_cast<uint32_t>(static_cast<uint8_t>(cut[k + b])) << (8 * b);
        data.push_back(w);
      }
    }
  }
  mods_checked = 0;
  znet::set_player_data(me, data.data(), static_cast<uint32_t>(data.size() * 4));
  // PM 0x0049aa76..0x0049afc8: REALLOC (the salvage's multiple: its value in halves), the kills, minutes and tons
  // (1000 none; MW3's kills doubled with teams, its none 100), the rounds (1..10, or to the match's between rounds),
  // then each picked from the game and the boxes set from it.
  salvage_award.clear_items();
  static const std::pair<const char*, int32_t> kRealloc[9] = {{"0", 0},     {"X .5", 1},   {"X 1", 2},
                                                              {"X 1.5", 3}, {"X 3", 6},    {"X -3", -6},
                                                              {"X -1.5", -3}, {"X -1", -2}, {"X -.5", -1}};
  for (const auto& r : kRealloc) salvage_award.add_item(r.first, r.second);
  kills_list.clear_items();
  for (int32_t k : {1, 2, 5, 10, 20, 40, 50, 60, 80}) kills_list.add_item(zutl::format_message_f(0x2b2, k).c_str(), k);
  kills_list.add_item(e.message(0x2b5), 1000);
  kills_list.select_value(5);
  time_list.clear_items();
  static const int32_t kMinutes[4] = {5, 10, 20, 30};
  for (int32_t i = 0; i < 4; ++i) time_list.add_item(e.message(0x2b6 + i), kMinutes[i]);
  time_list.add_item(e.message(0x2ba), 1000);
  weight_list.clear_items();
  for (int32_t t = 10; t < 0x69; t += 5) weight_list.add_item(zutl::format_message_f(0x2bb, t).c_str(), t);
  weight_list.select_value(0x37);
  rounds_list.clear_items();
  const int32_t rounds_n = between_rounds ? game->rounds_total : 10;
  for (int32_t i = 1; i <= rounds_n; ++i) rounds_list.add_item(std::to_string(i).c_str(), i);
  game_type.select_value(game->kind);
  worlds.select_value((game->x94 & 0xff) << 8 | (game->x90 & 0xff));  // vt+0x48, vt+0x50
  salvage_award.select_value(game->realloc);
  rounds_list.select_value(game->rounds_total);
  kills_list.select_value(std::min(game->x70, 1000));
  time_list.select_value(static_cast<int32_t>(game->x74));
  weight_list.select_value(game->x78);
  regen_box.set_checked(game->b8c);
  mobile_box.set_checked(game->ba8);
  stock_box.set_checked(game->b8f);
  stock_kept = game->b8f;
  ammo_box.set_checked(game->b8d);
  select_box.set_checked(game->b8e);
  jets_box.set_checked(game->jets_assist);
  radar_box.set_checked(game->radar);
  chat_team.set_checked(0);
  teams_list.empty();
  players_list.empty();
  chat_log.clear_log();  // 0x00499680
  chat_edit.set_text("");
  chat_edit.set_focus(0);
  chat_edit.set_focus(1);
  all_ready = 0, start_pending = 0;
  host_controls();
  rejoin_at = ztimer::state().total_scaled;  // PM 0x0049b103
  for (ztwod::Control* c : std::initializer_list<ztwod::Control*>{&join_team_btn, &remove_team_btn, &kick_btn,
                                                                  &new_team_btn, &pick_skin_btn, &kick2_btn,
                                                                  &mechlab_btn, &start_btn})
    set_enabled(*c, 0);
  ready_box.set_checked(0);
  set_enabled(ready_box, 0);
  if (team) chat_team.show_for(-1.0f);
  else chat_team.hide();
  g_lobby_dirty = 1;
  g_lobby_back = 0;
  handlers(true);
  znet::GameInfo info;
  if (znet::is_host() && znet::game_info(-1, &info)) {
    info.user[3] = 1;  // open (2 in progress, 3 locked)
    znet::set_game_info(info);  // 0x00586f20
  }
  znet::enum_players();
  if (team) {
    znet::enum_teams();
    znet::enum_team_players();
  }
}

// Mech3.exe 1.2 @ 0x004984e0. Ours: the host's addresses each half second.
void LobbyScreen::update(float dt) {
  if (g_lobby_back) {
    on_button(1, 0, 0);
    g_lobby_back = 0;
  }
  // PM 0x0049b66e: between rounds with no enemies left the match is over (its rounds those played, our team
  // given the round), the lobby entered again as the match's end.
  if (between_rounds && game && game->enemies_of(znet::local_id()) == 0) {
    engine().message_box_text(0x30, "Network Message", "Game over: No enemies left");
    game->rounds_total = net_rounds_played();
    game->award_team(znet::local_id());
    enter(false);
    return;
  }
  if (g_lobby_dirty) {
    send_rules();
    refresh_lists();
    refresh_buttons();
    g_lobby_dirty = 0;
  }
  // Ours (the mods check): a joiner whose gameplay mods differ from the
  // host's is turned back, once the host's data says what they are.
  if (!mods_checked && !znet::is_host() && znet::session().open) {
    uint64_t theirs = 0;
    std::vector<std::string> their_names;
    if (mods_of(player_data(znet::session().host_id), &theirs, &their_names)) {
      mods_checked = 1;
      const ModsIdentity& mine = mods_identity();
      if (theirs != mine.hash) {
        cursor_obj.erase();
        engine().message_box_plain(0x30, 0x2a8, "Your mods don't match the host's. The host uses: " + mods_text(their_names) +
                                                     ". You use: " + mods_text(mine.names) + ".");
        g_lobby_back = 1;
      }
    }
  }
  if (start_pending) start();
  if ((reach_timer -= dt) <= 0.0f) {
    show_reach();
    reach_timer = 0.5f;
  }
  Window::update(dt);
}

// Mech3.exe 1.2 @ 0x00497410
void LobbyScreen::on_button(int32_t id, int32_t a, int32_t b) {
  (void)a, (void)b;
  Engine& e = engine();
  switch (id) {
    case 1:  // CANCEL: the handlers off, back, the network off, the records gone (PM 0x0049a1b0's; started
             // from a DirectPlay lobby it went to state 4 instead: there is none here)
      handlers(false);
      between_rounds = 0;
      e.pop_state(1);
      e.network_set(0);
      g_lobby_dirty = 0;
      if (game) game->clear_lobby();
      return;
    case 0: start(); return;
    case 2: chat_send(0); return;  // 0x004999f0
    case 3: {                      // 0x00499a00
      const LobbyPlayer* me = game ? game->lobby_player(znet::local_id()) : nullptr;
      chat_send(me ? me->team : 0);
      return;
    }
    case 4: join_team(); return;
    case 7: open_new_team(); return;
    case 8: open_pick_skin(); return;
    case 5: remove_team(); return;
    case 10: mech_lab(); return;
    case 0xd: lock(); return;  // PM 0x0049e3f0: through the screen's +0x58 (0x0049e400)
    case 0x1c: ready(); return;
    case 0xf: world_changed(); return;  // ours: PM's is case 0xe's
    case 0x11:  // PM: ROUNDS also decides STOCK (0x0049b5e0)
      g_lobby_dirty = 1;
      set_enabled(stock_box, stock_control() ? 1 : 0);
      return;
    case 0x17:  // STOCK kept as the host leaves it
      stock_kept = stock_box.checked != 0;
      g_lobby_dirty = 1;
      return;
    case 0xe:
    case 0x10:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x18:
    case 0x19:
    case 0x1a:
    case 0x1b:  // a rule changed: sent with the next refresh
      g_lobby_dirty = 1;
      return;
    case 6: kick(teams_list); return;
    case 9: kick(players_list); return;
    case 0x1d: team_chosen(); return;    // PM 0x0049dc50
    case 0x1e: player_chosen(); return;  // PM 0x0049dfd0
    case 0xb: salvage(); return;  // PM's OPTIONS (0xc) has no case: the button does nothing
    default: return;
  }
}

// Mech3.exe 1.2 @ 0x00498550. FIX (A): with `report` and a lobby's launch
// ([0x00608e24]) the scores went to the game service's COM object
// ([0x007307e0], 0x00589d00..0x00589e30); there is no such service.
void LobbyScreen::scores(bool report) {
  (void)report;
  scores_list.clear_items();
  scores_list.layout();
  shown_scores.clear();
  if (!game) return;
  if (game->lobby_players.empty()) {
    scores_list.add_item(engine().message(0x2bd), 0);  // "  No Scores to report"
    return;
  }
  game->score_texts(0x15, 7);
  for (const auto& l : game->score_lines) {
    scores_list.add_item(l->text.c_str(), 0);
    shown_scores.push_back(l->text);
  }
}

// Mech3.exe 1.2 @ 0x00498710: the records made again from the network's
// players (and teams); TP_LIST each team ("name  {skin} ") with its players
// under it, PLAYERS_LIST the players without a team: the host's "++", "**"
// for one missing the chosen add-on world (PLAYERS_LIST only), "In MechLab"
// or "Ready!" (any other: not all ready), the skin outside team play, our
// own mech's name. Our slot's skin is our side's; a changed player count
// asks the session and the players again.
void LobbyScreen::refresh_lists() {
  if (!game) return;
  game->clear_lobby();
  g_order_next = 1;
  znet::each_player(add_player, nullptr);
  if (multiplayer_team_play()) znet::each_team(add_team, nullptr);
  // PM 0x0049bb70's lines (MultiplayerGame::lobby_lines), by GAME_TYPE's pick.
  static Engine* s_engine;
  s_engine = &engine();
  LobbyText tx;
  tx.message = [](int32_t id) { return s_engine->message(id); };
  tx.skin = [](int32_t skin) -> std::string {  // the screen's names (0x0049a3c0: messages 0xdd7 on)
    const char* m = s_engine->message(0xdd6 + skin);
    return m ? m : "";
  };
  std::vector<LobbyLine> tp, pl;
  all_ready = game->lobby_lines(game_type.selected_value(), znet::local_id(), tx, &tp, &pl) ? 1 : 0;
  teams_list.empty();
  for (const LobbyLine& l : tp) teams_list.add(l.text, l.entry);
  players_list.empty();
  for (const LobbyLine& l : pl) players_list.add(l.text, l.entry);
  game->slot_at(0)->skin = game->net_skin(znet::local_id()) - 1;  // 0x005a2b90
  znet::GameInfo info;
  if (znet::game_info(-1, &info) && game->lobby_players.size() != info.players) {
    znet::refresh_session();  // 0x00586e10
    znet::enum_players();
  }
}

// Pirate's Moon 1.0 @ 0x0049ca00 (lobby_buttons; MW3's 0x004993f0).
void LobbyScreen::refresh_buttons() { apply_buttons(lobby_buttons(view())); }

LobbyView LobbyScreen::view() {
  LobbyView v;
  v.host = znet::is_host(), v.team_play = multiplayer_team_play(), v.locked = locked;
  v.between_rounds = between_rounds, v.ready = ready_box.checked != 0, v.all_ready = all_ready;
  v.me = znet::local_id();
  if (const LobbyPlayer* p = game ? game->lobby_player(v.me) : nullptr) v.my_team = p->team, v.my_skin = p->skin;
  v.teams = game ? static_cast<int32_t>(game->lobby_teams.size()) : 0;
  if (const CycleButton::Item* it = game_type.item(game_type.index)) v.kind.has = true, v.kind.value = it->value;
  return v;
}

void LobbyScreen::apply_buttons(const LobbyButtons& b) {
  const std::pair<ztwod::Control*, int8_t> set[11] = {
      {&ready_box, b.ready},  {&mechlab_btn, b.mechlab},     {&salvage_btn, b.salvage}, {&options_btn, b.options},
      {&start_btn, b.start},  {&new_team_btn, b.new_team},   {&pick_skin_btn, b.pick_skin}, {&join_team_btn, b.join},
      {&remove_team_btn, b.remove}, {&kick_btn, b.kick},     {&kick2_btn, b.kick2}};
  for (const auto& [c, on] : set)
    if (on >= 0) set_enabled(*c, on);
  if (b.deselect_teams) teams_list.select_index(-1);
  if (b.deselect_players) players_list.select_index(-1);
}

// Pirate's Moon 1.0 @ 0x0049b3f0: the rules the host's until locked: the
// lists' arrows (the worlds' also between rounds while not ready), the boxes
// (STOCK by 0x0049b5e0), LOCK and (team play) NEW_TEAM. Ours after it, for
// the add-on worlds we list (MW3's 0x00498260): one without MOBILE (an
// add-on of folder 4) clears and disables it.
void LobbyScreen::host_controls() {
  const bool host = znet::is_host();
  const bool on = host && !locked;
  const bool world_on = (host && between_rounds && !ready_box.checked) || on;
  worlds.set_arrows(world_on, world_on);
  for (CycleButton* c : {&kills_list, &time_list, &weight_list, &game_type, &salvage_award, &rounds_list})
    c->set_arrows(on, on);
  set_enabled(regen_box, on ? 1 : 0);
  set_enabled(mobile_box, on ? 1 : 0);
  set_enabled(stock_box, stock_control() ? 1 : 0);
  for (ztwod::Control* c : std::initializer_list<ztwod::Control*>{&ammo_box, &select_box, &jets_box, &radar_box,
                                                                  &lock_btn})
    set_enabled(*c, on ? 1 : 0);
  set_enabled(new_team_btn, on && multiplayer_team_play() ? 1 : 0);
  if (!on) return;
  const int32_t w = worlds.item(worlds.index) ? worlds.item(worlds.index)->value : -1;
  if ((w & 0xff) == 4 && (w >> 8) > 2 && mobile_box.checked) {
    mobile_box.set_checked(0);
    set_enabled(mobile_box, 0);
    g_lobby_dirty = 1;
  }
}

// Pirate's Moon 1.0 @ 0x0049b5e0: STOCK is the host's while unlocked (as it
// left it); between the rounds of a match of more (0x0049ea90: ROUNDS over
// 1) it is off, the mechs kept from round to round.
bool LobbyScreen::stock_control() {
  if (!znet::is_host()) return false;
  if (rounds_list.selected_value() > 1 && between_rounds) {
    stock_box.set_checked(0);
    return false;
  }
  if (locked) return false;
  stock_box.set_checked(stock_kept);
  return true;
}

// Mech3.exe 1.2 @ 0x00497fbf..0x004980b6 (on), 0x00499a30 (off).
void LobbyScreen::handlers(bool on) {
  static const uint16_t kChange[] = {3, 4, 9, 0xa, 5, 6, 8, 2};
  for (uint16_t t : kChange) on ? (void)znet::on(t, on_change) : (void)znet::off(t, on_change);
  if (on) {
    znet::on(7, on_kicked);
    znet::on(0xd, on_host);
    znet::on(0x20, on_rules);
    znet::on(0x1e, on_start);
    znet::on(0x14, on_scores);
    znet::on(0x1f, on_chat);
  } else {
    znet::off(7, on_kicked);
    znet::off(0xd, on_host);
    znet::off(0x20, on_rules);
    znet::off(0x1e, on_start);
    znet::off(0x14, on_scores);
    znet::off(0x1f, on_chat);
  }
  handlers_on_ = on;
}

// Mech3.exe 1.2 @ 0x00499600
void LobbyScreen::on_change(uint32_t, const uint8_t*, size_t) { g_lobby_dirty = 1; }

// Mech3.exe 1.2 @ 0x0049a1d0: "You have been kicked out of this game.", then
// back to the connection screen (the update's CANCEL).
void LobbyScreen::on_kicked(uint32_t, const uint8_t*, size_t) {
  if (!g_lobby) return;
  g_lobby->cursor_obj.erase();  // [+0x58] vt+8
  g_lobby->engine().message_box(0x30, 0x2a8, 0x2c3);
  g_lobby_back = 1;
}

// Mech3.exe 1.2 @ 0x0049a200: a new host takes the rules.
void LobbyScreen::on_host(uint32_t, const uint8_t*, size_t) {
  if (g_lobby && znet::is_host()) g_lobby->host_controls();
  g_lobby_dirty = 1;
}

// Mech3.exe 1.2 @ 0x0049a150, 0x0049a7a0: the chosen player out of the game;
// not ourselves (0x2c1).
void LobbyScreen::kick(LobbyList& from) {
  LobbyEntry* en = from.chosen();
  if (!en || en->kind() != 1) return;
  if (en->id == znet::local_id()) {
    cursor_obj.erase();
    engine().message_box(0x30, 0x2a8, 0x2c1);  // "You can't kick yourself out of the game!"
    return;
  }
  znet::kick(en->id);
}

// Pirate's Moon 1.0 @ 0x0049dc50 (lobby_team_chosen; MW3's 0x0049a3a0).
void LobbyScreen::team_chosen() {
  LobbyEntry* en = teams_list.chosen();
  if (!en || !game) return;
  const LobbyView v = view();
  const LobbyButtons b = lobby_team_chosen(v, en->kind(), en->id, en->kind() == 2 && game->in_team(v.me, en->id));
  if (b.chose_team) chosen_team = en->id;
  apply_buttons(b);
}

// Pirate's Moon 1.0 @ 0x0049dfd0 (lobby_player_chosen; MW3's 0x0049a6a0).
void LobbyScreen::player_chosen() {
  LobbyEntry* en = players_list.chosen();
  if (!en || !game) return;
  apply_buttons(lobby_player_chosen(view(), en->kind(), en->id));
}

// Mech3.exe 1.2 @ 0x0049bd10: in team play, the team with that skin (not
// `except`), 0 none; outside team play always 0.
int32_t LobbyScreen::free_skin(int32_t skin, int32_t except) {
  if (!multiplayer_team_play() || !game || skin == except) return 0;
  for (auto& t : game->lobby_teams)
    if (t->skin == skin) return static_cast<int32_t>(t->id);
  return 0;
}

// Mech3.exe 1.2 @ 0x0049ab30
bool LobbyScreen::has_world(uint32_t player, int32_t world) {
  const std::vector<uint32_t> d = player_data(player);
  if (d.size() <= kPmDataWords || d[0] != 1) return false;  // PM's 7 words, then our worlds
  for (uint32_t k = 0; k < d[kPmDataWords] && kPmDataWords + 1 + k < d.size(); ++k)
    if (static_cast<int32_t>(d[kPmDataWords + 1 + k]) == world) return true;
  return false;
}

// ---- the rules (piece 11b) ----

namespace {
bool no_mobile(int32_t world) { return (world & 0xff) == 4 && (world >> 8) > 2; }
}  // namespace

// The lists and boxes as the rules message sees them, and back.
LobbyControls LobbyScreen::controls() {
  LobbyControls c;
  const std::pair<CycleButton*, LobbyChoice*> lists[7] = {
      {&kills_list, &c.kills}, {&time_list, &c.minutes}, {&weight_list, &c.tons},  {&salvage_award, &c.realloc},
      {&rounds_list, &c.rounds}, {&game_type, &c.kind},  {&worlds, &c.world}};
  for (const auto& [from, to] : lists) {
    for (const CycleButton::Item* it : from->items) to->values.push_back(it->value);
    to->pick = from->index;
  }
  c.respawn = regen_box.checked, c.ammo = ammo_box.checked, c.outside = select_box.checked;
  c.jets = jets_box.checked, c.radar = radar_box.checked, c.stock = stock_box.checked, c.mobile = mobile_box.checked;
  c.locked = locked;
  return c;
}

void LobbyScreen::show_controls(const LobbyControls& c) {
  const std::pair<CycleButton*, const LobbyChoice*> lists[7] = {
      {&kills_list, &c.kills}, {&time_list, &c.minutes}, {&weight_list, &c.tons},  {&salvage_award, &c.realloc},
      {&rounds_list, &c.rounds}, {&game_type, &c.kind},  {&worlds, &c.world}};
  for (const auto& [to, from] : lists)
    if (to->index != from->pick) to->select(from->pick);  // vt+0xa0
  const std::pair<InvertCheck*, bool> boxes[7] = {{&regen_box, c.respawn}, {&mobile_box, c.mobile},
                                                  {&stock_box, c.stock},   {&ammo_box, c.ammo},
                                                  {&select_box, c.outside}, {&jets_box, c.jets},
                                                  {&radar_box, c.radar}};
  for (const auto& [box, on] : boxes) box->set_checked(on ? 1 : 0);  // vt+0x8c
  locked = c.locked;
}

// Pirate's Moon 1.0 @ 0x0049d220: the host's, to every player, guaranteed
// (the screen's vt+0x14 before it is empty here).
void LobbyScreen::send_rules() {
  if (!znet::is_host()) return;
  const std::vector<uint8_t> m = lobby_rules_message(controls());
  znet::send(m.data(), m.size(), true, 0);
}

// Pirate's Moon 1.0 @ 0x0049d450: the host's rules shown and kept
// (MultiplayerGame::read_lobby_rules), the boxes not ours to change; on
// their first lock the team play as the host's kind, the chat's team box
// with it, the players again (not the teams, as MW3's) and our mech fitted
// to the rules. FIX (A): only from the host, and whole (the exe took any
// sender's, and read what the message lacked).
void LobbyScreen::on_rules(uint32_t from, const uint8_t* msg, size_t size) {
  LobbyScreen* l = g_lobby;
  if (!l || !l->game || size < 0x18 || from != znet::session().host_id || znet::is_host()) return;
  LobbyControls c = l->controls();
  bool team = multiplayer_team_play();
  const bool first = l->game->read_lobby_rules(msg, size, &c, std::rand(), &team);
  l->show_controls(c);
  for (InvertCheck* b : {&l->regen_box, &l->mobile_box, &l->stock_box, &l->ammo_box, &l->select_box, &l->jets_box,
                         &l->radar_box})
    set_enabled(*b, 0);
  g_lobby_dirty = 1;
  if (!first) return;
  set_multiplayer_team_play(team);  // [0x0062cc34]
  if (team) l->chat_team.show_for(-1.0f);
  else l->chat_team.hide();
  znet::enum_players();  // 0x005a9520
  l->lock_mech();
}

// Ours (MW3's 0x00497410 case 0xc, for the add-on worlds we list): a world
// without MOBILE (an add-on of folder 4) clears and disables it.
void LobbyScreen::world_changed() {
  const bool ok = !no_mobile(worlds.selected_value());
  if (!ok) mobile_box.set_checked(0);
  set_enabled(mobile_box, ok && !locked ? 1 : 0);
  g_lobby_dirty = 1;
}

// Pirate's Moon 1.0 @ 0x0049e400 (LOCK, 0x0049e3f0 through the screen's
// +0x58): the kind's rules (check_lobby_rules: an error box with the lists
// or RESPAWN put right, or a confirmation answered no), then locked: team
// play as the kind (the chat's team box with it), the players (and teams)
// again, the game shown as locked to the search (its fourth word 3), the
// host's controls off, the rules sent, our mech fitted. MW3's 0x0049aca0
// kept the rules' block here; PM's host keeps it at READY (0x0049e6f0).
// Ours first: the mods check and the add-on worlds' (MW3's 0xfe0).
void LobbyScreen::lock() {
  Engine& e = engine();
  cursor_obj.erase();
  // Ours (the mods check): every player's gameplay mods the host's, as the
  // add-on world below, with their names.
  if (game) {
    const ModsIdentity& mine = mods_identity();
    std::string lines;
    for (auto& p : game->lobby_players) {
      if (p->id == znet::local_id()) continue;
      uint64_t theirs = 0;
      std::vector<std::string> names;
      mods_of(player_data(p->id), &theirs, &names);
      if (theirs != mine.hash) lines += p->name + " uses: " + mods_text(names) + ". ";
    }
    if (!lines.empty()) {
      e.message_box_plain(0, 0x1e3, "Every player needs the same mods. " + lines + "You use: " + mods_text(mine.names) + ".");
      return;
    }
  }
  const int32_t w = worlds.selected_value();
  if ((w >> 8) > 2 && game) {
    std::string names;
    int32_t n = 0;
    for (auto& p : game->lobby_players)
      if (p->id != znet::local_id() && !has_world(p->id, w)) names += p->name + " ", ++n;
    if (n > 0) {
      e.message_box3(0, 0x1e3, 0xfe0, names.c_str(), nullptr, nullptr);
      return;
    }
  }
  static Engine* s_engine;
  s_engine = &e;
  const auto confirm = [](int32_t text) { return s_engine->message_box(1, 0x1e3, text) == 1; };  // 0x004afb30
  const auto error = [&](const char* text) { e.message_box_text(0, "Multiplayer Rule Error", text); };  // 0x004afa20
  bool team = multiplayer_team_play();
  switch (check_lobby_rules(controls().settings(), confirm, &team)) {
    case LobbyRule::one_base_unlimited:
      error("Attack/Defend with one base requires a time limit or a score limit");
      return;
    case LobbyRule::two_bases_limited:
      error("Attack/Defend with two bases requires unlimited time and unlimited score.");
      kills_list.select_value(1000);
      time_list.select_value(1000);
      return;
    case LobbyRule::no_respawn:
      error("Attack/Defend games and 'No RESPAWN' are incompatible.");
      regen_box.set_checked(1);
      return;
    case LobbyRule::declined: return;
    case LobbyRule::start: break;
  }
  set_multiplayer_team_play(team);  // [0x0062cc34]
  if (team) chat_team.show_for(-1.0f);
  else chat_team.hide();
  znet::enum_players();
  if (team) {
    znet::enum_teams();
    znet::enum_team_players();
  }
  znet::GameInfo info;
  if (znet::is_host() && znet::game_info(-1, &info)) {
    info.user[3] = 3;
    znet::set_game_info(info);
  }
  locked = 1;
  host_controls();
  send_rules();
  g_lobby_dirty = 1;
  lock_mech();
}

// Pirate's Moon 1.0 @ 0x0049eb20: the game's store filled here, not in its constructor, and refilled
// at each lock (0x004732b0), then its mechs out (0x0049e950).
void lobby_store_ready(Store& st) {
  pilot_hooks().store_fill(st, kMultiplayer);
  for (auto it = st.items.begin(); it != st.items.end();) {
    if ((*it)->kind() == 0) {
      delete *it;
      it = st.items.erase(it);
    } else {
      ++it;
    }
  }
}

// Mech3.exe 1.2 @ 0x0049b360 (0x0049b1c0, 0x0049b2d0), Pirate's Moon 1.0 @ 0x0049eb20: the store ready,
// then each mech within WEIGHT_LIST's tons in (marked); a slot mech over
// the limit (or none) becomes the heaviest within it with its first stock
// loadout; one within keeps its loadout, given the first stock one when it
// has none, or with STOCK on when it is not one of the mech's stock
// loadouts. The slot's loadout is its own copy, never one of the list's,
// so with STOCK on the exe always made it the first (multiplayer-port.md 11b).
void LobbyScreen::lock_mech() {
  if (!game) return;
  // PM 0x0049eb20's gate (0x0049ea90, 0x0049eb10; by reading): in a match of more rounds, only before its first.
  if (rounds_list.selected_value() > 1 && net_rounds_played() != 0) return;
  Engine& e = engine();
  const float tons = static_cast<float>(weight_list.selected_value());
  const bool stock = stock_box.checked != 0;
  Store& st = game->store;
  lobby_store_ready(st);
  int32_t best = -1;
  float best_mass = 0;
  for (int32_t m = 0; m < kMechCount; ++m) {  // Pirate's Moon 0x0049ea10: the Elemental too (0x004d5020 is always 0)
    const float mass = max_mass(m);
    if (tons < mass) continue;
    if (best == -1 || best_mass < mass) best = m, best_mass = mass;
    if (BaseItem* b = st.add_mech(m)) b->b10 = 1;
  }
  GameSlot& slot = game->slot;
  const auto first = [&](int32_t mech) -> const Config* {
    const ConfigList* l = mech >= 0 ? e.mech_loadouts(mech) : nullptr;
    return l && !l->configs.empty() ? l->configs[0] : nullptr;
  };
  if (slot.mech == -1 || max_mass(slot.mech) > tons) {
    slot.mech = best;
    slot.set_config(first(best));
    return;
  }
  if (!slot.config) {
    slot.set_config(first(slot.mech));
    return;
  }
  if (!stock) return;
  const ConfigList* l = e.mech_loadouts(slot.mech);
  int32_t at = -1;  // 0x00425b70: by pointer, which the slot's copy never is
  // FIX (bug): a chosen stock loadout kept, found by its name (fixes.md).
  const bool by_name = zfix::options().bugs && slot.config->name;
  if (l)
    for (size_t i = 0; i < l->configs.size(); ++i)
      if (by_name ? l->configs[i]->name && std::strcmp(l->configs[i]->name, slot.config->name) == 0
                  : l->configs[i] == slot.config) {
        at = static_cast<int32_t>(i);
        if (by_name) break;
      }
  if (l && at >= 0 && at <= l->count - 1) return;
  slot.set_config(first(slot.mech));
}

// ---- the start, and each player's own (piece 11c) ----

namespace {
// Our own data, the lobby's kind 1 (0x0049a970, 0x0049b500, 0x0049b660 read
// it so; anything else is left alone).
bool my_data(std::vector<uint32_t>* d) {
  *d = player_data(znet::local_id());
  return d->size() >= 3 && (*d)[0] == 1;
}
void set_my_data(const std::vector<uint32_t>& d) {
  znet::set_player_data(znet::local_id(), d.data(), static_cast<uint32_t>(d.size() * 4));
}

// The network game's lobby calls on the started game (0x00435910..).
MultiplayerGame* g_net_game = nullptr;
NetLobbyAccess net_lobby_access() {
  NetLobbyAccess a;
  a.team_of = [](uint32_t p) { return g_net_game ? g_net_game->net_team(p) : 0u; };
  a.start_of = [](uint32_t p) {
    const LobbyPlayer* r = g_net_game ? g_net_game->lobby_player(p) : nullptr;
    return r ? r->order : 0;
  };
  a.left = [](uint32_t p) {
    if (LobbyPlayer* r = g_net_game ? g_net_game->lobby_player(p) : nullptr) r->left = 1, r->rounds = 0;  // PM 0x0046f440
  };
  a.out = [](uint32_t p) {
    const LobbyPlayer* r = g_net_game ? g_net_game->lobby_player(p) : nullptr;
    return r ? r->out != 0 : true;
  };
  a.set_out = [](uint32_t p, bool out) {
    if (g_net_game) g_net_game->set_out(p, out);
  };
  a.round_reset = [] {
    if (g_net_game) g_net_game->round_reset();
  };
  a.suicide = [](uint32_t p) {
    if (g_net_game) g_net_game->add_deaths(p, 1);
  };
  a.kill = [](uint32_t p) {
    if (g_net_game) g_net_game->add_kills(p, 1);
  };
  a.kills = [](uint32_t p) { return g_net_game ? g_net_game->kills_of(p) : -1; };
  a.score = [](uint32_t p) { return g_net_game ? g_net_game->score_of(p) : -1; };
  a.deaths = [](uint32_t p) { return g_net_game ? g_net_game->deaths_of(p) : -1; };
  a.scores = [](uint32_t* out, int32_t cap) {
    int32_t n = 0;
    if (g_net_game)
      for (auto& r : g_net_game->lobby_players) {
        if (n >= cap) break;
        out[3 * n] = r->id, out[3 * n + 1] = static_cast<uint32_t>(r->kills), out[3 * n + 2] = static_cast<uint32_t>(r->deaths);
        ++n;
      }
    return n;
  };
  a.set_kills = [](uint32_t p, uint32_t v) {
    if (g_net_game) g_net_game->set_kills(p, static_cast<int32_t>(v));
  };
  a.set_deaths = [](uint32_t p, uint32_t v) {
    if (g_net_game) g_net_game->set_deaths(p, static_cast<int32_t>(v));
  };
  a.rescore = [] {
    if (g_net_game) g_net_game->rescore();
  };
  a.round_winner = [] { return g_net_game && g_net_game->round_winner(); };
  a.round_tally = [] {
    if (g_net_game) g_net_game->tally(true);
  };
  a.store_give = [](int32_t mech, const Config* c) {
    if (g_net_game) g_net_game->round_store.give_config(mech, c);
  };
  a.game_kind = [] { return g_net_game ? g_net_game->kind : 0; };
  a.spawn_base = [](uint32_t p) { return g_net_game ? g_net_game->spawn_base(p) : 24; };
  a.local_slot = [](int32_t* mech, const Config** c) {
    if (!g_net_game) return false;
    *mech = g_net_game->slot.mech, *c = g_net_game->slot.config;  // slot 0's +4, +8
    return true;
  };
  a.team_id = [](uint32_t p) { return g_net_game ? g_net_game->lobby_team_id(p) : 0u; };
  a.name = [](uint32_t p) -> const char* {
    const LobbyPlayer* r = g_net_game ? g_net_game->lobby_player(p) : nullptr;
    return r ? r->name.c_str() : nullptr;
  };
  return a;
}
}  // namespace

// Pirate's Moon 1.0 @ 0x0049ef20: the host tells everyone the players'
// orders and team numbers (0x1e, start_message) and keeps the order;
// everyone then gives the game the lobby's settings (apply_lobby: the
// limits, the boxes, the rules' flags, the world, the clock; 1000 none),
// drops the lobby's handlers (0x0049d170), gives the game the lines (TP_LIST
// in team play, else PLAYERS_LIST; 0x004377c0), starts a match of more
// rounds (0x00438550 with the rounds played) and goes to the Load state
// (0x004a02b0). MW3's 0x0049b790 also showed the game in progress (its
// fourth word 2); PM's doesn't. Ours: the network game's lobby calls on
// this game (the debug start's stand-ins otherwise).
void LobbyScreen::start() {
  Engine& e = engine();
  start_pending = 0;
  if (!game) return;
  if (znet::is_host()) {
    const std::vector<uint8_t> m = game->start_message();
    znet::send(m.data(), m.size(), true, 0);
    keep_order = 1;
  }
  handlers(false);
  bool clock_on = false;
  float clock_left = 0;
  game->apply_lobby(controls().settings(), &clock_on, &clock_left);
  program_net_clock(clock_left, clock_on);  // [0x0062da94], [0x0062da98]
  LobbyList& lines = multiplayer_team_play() ? teams_list : players_list;
  std::vector<std::string> roster;
  for (int32_t i = 0; i < lines.item_count(); ++i) roster.emplace_back(lines.items[static_cast<size_t>(i)]->text);
  game->set_roster(roster);
  if (rounds_list.selected_value() > 1) game->round_start(net_rounds_played());  // 0x0049ea90, 0x0049eb10
  g_net_game = game;
  program_net_lobby(net_lobby_access());
  e.pop_state(0);
  e.replace_state(6);
}

// Mech3.exe 1.2 @ 0x0049bcb0: the host's places for its players; then our
// start. FIX (A): only the host's, whole (the exe took any player's).
void LobbyScreen::on_start(uint32_t from, const uint8_t* msg, size_t size) {
  LobbyScreen* l = g_lobby;
  if (!l || !l->game || size < 8 || from != znet::session().host_id || znet::is_host()) return;
  l->game->read_start_message(msg, size);  // PM 0x0049f510
  l->keep_order = 1;
  l->start_pending = 1;
}

// Mech3.exe 1.2 @ 0x0049b150: the host's scores between games, each player's
// kills and suicides; the lines again. FIX (A): read only as far as the
// message goes.
void LobbyScreen::on_scores(uint32_t, const uint8_t* msg, size_t size) {
  LobbyScreen* l = g_lobby;
  if (!l || !l->game || size < 6) return;
  int16_t n;
  std::memcpy(&n, msg + 4, 2);
  for (int32_t k = 0; k < n && 6 + 12 * static_cast<size_t>(k + 1) <= size; ++k) {
    uint32_t w[3];
    std::memcpy(w, msg + 6 + 12 * k, 12);
    l->game->set_kills(w[0], static_cast<int32_t>(w[1]));
    l->game->set_deaths(w[0], static_cast<int32_t>(w[2]));
  }
  l->game->rescore();
  l->scores(false);
}

// Pirate's Moon 1.0 @ 0x0049e2e0: READY (after its press) into our data's
// state, 5 ready or 4 not (0x0049ecb0). Its check of another team with our
// skin (0x0049f570 with the skin as the exception, which never answers one)
// is dead. The host's controls follow the box and the host keeps the
// rules' block (0x0049e6f0, its rand() seeded with the time); in team play
// at most every 5 seconds the host leaves its team and joins it again
// (0x00498800, 0x005a9480, 0x0049daf0).
void LobbyScreen::ready() {
  if (!game) return;
  const uint32_t me_id = znet::local_id();
  const LobbyPlayer* me = game->lobby_player(me_id);
  if (ready_box.checked && me && free_skin(me->skin, me->skin) != 0) {
    ready_box.set_checked(0);  // (with 0xf47 / 0xf48 in team play)
    return;
  }
  if (znet::is_host()) {
    host_controls();
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    game->keep_lobby_rules(controls(), std::rand());
    const float now = ztimer::state().total_scaled;  // [0x008c1b50]
    if (multiplayer_team_play() && rejoin_at < now) {
      rejoin_at = now + 5.0f;
      const uint32_t team = me ? me->team : 0;
      znet::remove_from_team(team, me_id);
      if (game->lobby_team(team) && znet::add_to_team(team, me_id))
        if (LobbyPlayer* r = game->lobby_player(me_id)) r->team = team;
    }
  }
  std::vector<uint32_t> d;
  if (!my_data(&d)) return;
  d[2] = ready_box.checked ? 5 : 4;
  set_my_data(d);
}

// Mech3.exe 1.2 @ 0x0049b500: our state kept, then "In MechLab" (1); the
// lab on the game's store, stock mechs only with STOCK.
void LobbyScreen::mech_lab() {
  if (!game) return;
  std::vector<uint32_t> d;
  if (!my_data(&d)) return;
  saved_state = static_cast<int32_t>(d[2]);
  d[2] = 1;
  set_my_data(d);
  Engine& e = engine();
  e.mech_lab_open(e.store_handle(&game->store), stock_box.checked ? 1 : 0);  // 0x00479430, 0x00448830
}

// Pirate's Moon 1.0 @ 0x0049eef0: back from the lab, our state none (4;
// MW3's 0x0049b660 put back the state it kept); the chat's bar given the
// keyboard again.
void LobbyScreen::resume() {
  std::vector<uint32_t> d;
  if (my_data(&d)) {
    d[2] = 4;
    set_my_data(d);
  }
  chat_edit.set_focus(0);
  chat_edit.set_focus(1);
}

// Pirate's Moon 1.0 @ 0x0049ed90: "In Salvage" (2); slot 0 kept as it is,
// its mech and loadout back into the game's store (0x00473e70, 0x00434880
// with the store), no mech; then the Salvage State (0x0047f790, the
// briefing's), the store against the round's.
void LobbyScreen::salvage() {
  if (!game) return;
  chat_edit.set_focus(0);
  std::vector<uint32_t> d;
  if (my_data(&d)) {
    d[2] = 2;
    set_my_data(d);
  }
  GameSlot& slot = game->slot;
  if (!kept_slot) kept_slot = std::make_unique<GameSlot>();
  kept_slot->copy_from(slot);
  game->store.add_mech(slot.mech);
  slot.set_config(nullptr, &game->store, true);
  slot.mech = -1;
  engine().salvage_open();
}

// Pirate's Moon 1.0 @ 0x0049ee20: back from salvage, our state none (4), the
// chat's bar the keyboard; slot 0 fitted again from the store as the lance
// is (0x004079b0, not kept), from the mech and loadout SALVAGE kept.
void LobbyScreen::back_from_salvage() {
  std::vector<uint32_t> d;
  if (my_data(&d)) {
    d[2] = 4;
    set_my_data(d);
  }
  chat_edit.set_focus(0);
  chat_edit.set_focus(1);
  if (!game || !kept_slot) return;
  const Config* kept = kept_slot->config;
  game->slot.mech = kept ? kept->mech : kept_slot->mech;
  lance_slot(game->slot, game->store, false, kept);
}

// Pirate's Moon 1.0 @ 0x0049daf0: a player into a team (the network's, then
// the record), both listed. MW3's 0x0049a220 also made a team's first player
// its owner (its data's first word); PM's never sets it.
bool LobbyScreen::add_to_team(uint32_t team, uint32_t player) {
  if (!game || !game->lobby_team(team)) return false;
  LobbyPlayer* p = game->lobby_player(player);
  if (!p || !znet::add_to_team(team, player)) return false;
  p->team = team;
  return true;
}

// PM 0x0049db90 (MW3's 0x0049a300): a team made with its skin and number
// (its data {0, skin, number, 0, 0}) and its record; with `join` we leave ours
// for it. The number is the highest 0..7 no team has (0x00436650); PM picks it
// as the lobby opens (0x0049faf0's +0x123d4), here as the team is made (piece 6).
void LobbyScreen::new_team(const char* name, int32_t skin, bool join) {
  if (!game) return;
  const uint32_t id = znet::create_team(name);
  if (!id) return;
  uint32_t number = 0;
  for (int32_t i = 0; i < 8; ++i)
    if (!game->team_numbered(i)) number = static_cast<uint32_t>(i);
  const uint32_t data[5] = {0, static_cast<uint32_t>(skin), number, 0, 0};
  znet::set_team_data(id, data, sizeof data);
  add_team(id, 0, false, name, nullptr);  // 0x00499280
  if (!join) return;
  const uint32_t me = znet::local_id();
  LobbyPlayer* p = game->lobby_player(me);
  if (p && p->team && znet::remove_from_team(p->team, me)) p->team = 0;
  add_to_team(id, me);
}

// Pirate's Moon 1.0 @ 0x0049e1c0: outside team play our data's skin; in it
// the skin of the team TP_LIST last chose (+0x167b0; MW3's 0x0049a850 our
// own team's), none chosen nothing.
void LobbyScreen::set_skin(int32_t skin) {
  if (!game) return;
  if (!multiplayer_team_play()) {
    std::vector<uint32_t> d = player_data(znet::local_id());
    if (d.size() < 2) return;
    d[1] = static_cast<uint32_t>(skin);
    set_my_data(d);
    return;
  }
  if (!chosen_team) return;
  uint32_t data[5] = {}, size = sizeof data;
  znet::get_team_data(chosen_team, data, &size);  // 0x005aa590
  data[1] = static_cast<uint32_t>(skin);
  znet::set_team_data(chosen_team, data, sizeof data);
}

// Mech3.exe 1.2 @ 0x0049a550: into the chosen team (out of ours first).
void LobbyScreen::join_team() {
  if (!game) return;
  const uint32_t me = znet::local_id();
  LobbyPlayer* p = game->lobby_player(me);
  if (!p) return;
  LobbyEntry* en = teams_list.chosen();
  if (!en || en->kind() != 2 || en->id == 0) return;
  if (p->team && znet::remove_from_team(p->team, me)) p->team = 0;
  add_to_team(en->id, me);
  set_enabled(remove_team_btn, 0);
  set_enabled(join_team_btn, 0);
}

// Pirate's Moon 1.0 @ 0x0049df10: TP_LIST's team gone, its players taken
// out of it first (MW3's 0x0049a630 only an empty team), the first line
// chosen; REMOVE and PICK_SKIN off.
void LobbyScreen::remove_team() {
  LobbyEntry* en = teams_list.chosen();
  if (en) {
    if (en->kind() != 2) return;
    if (static_cast<LobbyTeam*>(en)->count > 0 && game)
      for (auto& p : game->lobby_players)
        if (p->team == en->id) znet::remove_from_team(en->id, p->id);  // 0x005a9480
    znet::destroy_team(en->id);  // 0x005a94f0
    teams_list.select_index(0);
  }
  set_enabled(remove_team_btn, 0);
  set_enabled(pick_skin_btn, 0);
}

// ---- the chat (piece 12) ----

ChatLog::ChatLog(Engine& e, LobbyScreen* s)
    : ztwod::TextBox(0xe), up_btn(e, this, true), down_btn(e, this, false), owner(s) {
  top = bottom = history.end();
}

// Mech3.exe 1.2 @ 0x004997a0
bool ChatLog::at_top() const { return top == history.begin(); }

// Mech3.exe 1.2 @ 0x00499760
bool ChatLog::at_bottom() const {
  return static_cast<int32_t>(history.size()) <= visible || (!history.empty() && bottom == std::prev(history.end()));
}

// Mech3.exe 1.2 @ 0x00499680: the lines gone but three spaces, shown, the
// window on it.
void ChatLog::clear_log() {
  history.clear();
  history.push_back("   ");  // 0x00495df0 ([0x005e7c4c])
  set_text("%s", "   ");
  top = bottom = history.begin();
}

// Mech3.exe 1.2 @ 0x004996d0: a full log at its top first scrolled a line
// (CHAT_DOWN's press); the line kept (the oldest past 100 dropped,
// 0x00495e60); shown when we were at the bottom, the window with it.
void ChatLog::add(const char* line) {
  if (at_top() && static_cast<int32_t>(history.size()) >= 100) down_btn.on_left_up();
  const bool was = at_bottom();
  history.push_back(line ? line : "");
  if (static_cast<int32_t>(history.size()) > max_lines) history.pop_front();
  if (!was) return;
  set_text("%s", line ? line : "");
  ++bottom;
  if (visible < static_cast<int32_t>(history.size())) ++top;
}

// Mech3.exe 1.2 @ 0x004997c0: up a line, the one above written at the top.
void ChatLog::scroll_up() {
  if (at_top()) return;
  --top, --bottom;
  set_text_top("%s", top->c_str());
}

// Mech3.exe 1.2 @ 0x00499820: down a line, the one below written at the bottom.
void ChatLog::scroll_down() {
  if (at_bottom()) return;
  ++top, ++bottom;
  set_text("%s", bottom->c_str());
}

void ChatScroll::on_left_up() { up ? log->scroll_up() : log->scroll_down(); }

// Mech3.exe 1.2 @ 0x00499880
void ChatBar::on_return() {
  if (!screen) return;
  if (multiplayer_team_play() && screen->chat_team.checked) screen->on_button(3, 0, 0);
  else screen->on_button(2, 0, 0);
}

// Mech3.exe 1.2 @ 0x004998b0: the bar's text trimmed (none: nothing), as
// 0x1f {flags (1 to a team), the text with its 0} in its length and 0xc,
// sent unguaranteed to the team (or all), shown here as it is to the others,
// the bar emptied.
void LobbyScreen::chat_send(uint32_t team) {
  std::string text = chat_edit.get_text() ? chat_edit.get_text() : "";
  const size_t a = text.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return;
  text = text.substr(a, text.find_last_not_of(" \t\r\n") - a + 1);
  const uint32_t size = static_cast<uint32_t>(text.size()) + 0xc;
  std::vector<uint8_t> m(size, 0);
  m[0] = 0x1f, m[1] = 0;
  m[2] = static_cast<uint8_t>(size), m[3] = static_cast<uint8_t>(size >> 8);
  if (team) m[4] |= 1;
  std::memcpy(m.data() + 8, text.c_str(), text.size() + 1);
  znet::send(m.data(), size, false, team);
  on_chat(znet::local_id(), m.data(), m.size());
  chat_edit.set_text("");
}

// Mech3.exe 1.2 @ 0x00499610: the sender's line ("< %1 to team >" 0xf43 or
// "< %1 to all >" 0xf44, 0x40 bytes at most), then the text. FIX (A): a
// message cut short is dropped, the text ends within it, its control
// characters are '?'; neither the text nor the name is a format (the
// message's %1 is filled once, zutl::format_message).
void LobbyScreen::on_chat(uint32_t from, const uint8_t* msg, size_t size) {
  LobbyScreen* l = g_lobby;
  if (!l || size < 9) return;
  const char* who = znet::player_name(from);
  std::string head = zutl::format_message_f((msg[4] & 1) ? 0xf43 : 0xf44, who ? who : "");
  if (head.size() > 0x3f) head.resize(0x3f);
  l->chat_log.add(head.c_str());
  const char* text = reinterpret_cast<const char*>(msg + 8);
  std::string line(text, strnlen(text, size - 8));
  for (char& c : line)  // FIX (A): no control characters on the screen; the text only ever a "%s" argument
    if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) c = '?';
  l->chat_log.add(line.c_str());
}

// ---- the new team's dialog (piece 13) ----

// Mech3.exe 1.2 @ 0x0049a2b0: a new team, its name and skin, JOIN checked;
// the chat's bar without the keyboard, TP_LIST's choice off; page 3.
void LobbyScreen::open_new_team() {
  if (!state) return;
  team_join = team_new = team_skin = 1;
  chat_edit.set_focus(0);  // PM 0x0049db50 (MW3's also undid TP_LIST's choice)
  engine().multiplayer_page(state, 3, 1);  // 0x0049cb00
}

// Mech3.exe 1.2 @ 0x0049a820: our skin alone; page 3.
void LobbyScreen::open_pick_skin() {
  team_skin = 1;
  team_join = team_new = 0;
  if (state) engine().multiplayer_page(state, 3, 1);
}

void TeamNameBar::on_return() { screen->on_button(0, 0, 0); }

// Mech3.exe 1.2 @ 0x0049bd80
TeamScreen::TeamScreen(Engine& e, Handle s)
    : MenuScreen(e, s), cancel_btn(e, 1), ok_btn(e, 0), name_edit(e, this), skins(e, 2) {
  name_edit.editor.set_reject(":|\\<>/?\"*");  // [0x005e28e8]
  name_edit.set_size(0x19);
  if (load_from(e.main_menu_definition(), "MULTIPLAYER_NEW_TEAM_DIALOG", true)) {
    add(&backdrop);
    bind_primitive(&skin_image, "SKIN_TEXTURES");
    bind_button(&cancel_btn, "CANCEL_BUTTON");
    bind_button(&ok_btn, "OK_BUTTON");
    bind_button(&name_edit, "NEW_TEAM_NAME_EDIT");
    bind_button(&join_box, "JOIN_TEAM_CHECKBOX");
    bind_button(&skins, "SKIN_LIST");
  }
  set_flags_all(0);
  set_active(1);
}

// Mech3.exe 1.2 @ 0x0049c170
TeamScreen::~TeamScreen() {
  if (still_tex) ztwod::host().image_release(still_tex);
}

// Mech3.exe 1.2 @ 0x0049c2c0: the screen under it kept as its backdrop; the
// name's bar shown with the keyboard for a new team; SKIN_LIST on to choose
// a skin, holding each one no other team has (every one outside team play),
// its image; a new team named by its first free skin's letter ("A Team",
// 0xf64); JOIN shown and checked for a new team.
void TeamScreen::enter(LobbyScreen* l) {
  lobby = l;
  still_tex = ztwod::host().screen_grab(2);
  if (still_tex) backdrop.set_image(still_tex);
  if (!lobby) return;
  if (lobby->team_new) {
    name_edit.show_for(-1.0f);
    name_edit.set_focus(1);
  } else {
    name_edit.set_focus(0);
    name_edit.hide();
  }
  skins.set_baked(lobby->team_skin != 0);  // +0x90: the button and its arrows on or off
  if (lobby->team_join) join_box.show_for(-1.0f);
  else join_box.hide();
  Engine& e = engine();
  int32_t first = 1000;
  skins.clear_items();
  for (int32_t i = 0; i < 8; ++i)
    if (lobby->free_skin(i + 1, -1) == 0) {
      skins.add_item(e.message(0xdd6 + i + 1), i + 1);
      if (first >= i) first = i;
    }
  show_skin();
  // PM 0x0049fc39: the first of the eight houses (messages 0x1000..0x1007, the lobby's +0x11d14) no team
  // is named; MW3's "%1 Team" (0xf64) with A, B, ... by the first free skin. All eight taken can't happen
  // here: eight teams leave NEW_TEAM off.
  (void)first;
  if (lobby->team_new) {
    int32_t house = -1;
    for (int32_t i = 0; i < 8 && house < 0; ++i)
      if (!znet::team_named(e.message(0x1000 + i))) house = i;  // 0x005aa410
    name_edit.set_text(house >= 0 ? e.message(0x1000 + house) : "");
  } else {
    name_edit.set_text("");
  }
  if (lobby->team_join) join_box.set_checked(1);
}

void TeamScreen::show_skin() {
  char name[32];
  std::snprintf(name, sizeof name, "popup_box%d", skins.selected_value());  // [0x005e7cb8]
  skin_image.set_image(name);
}

// Mech3.exe 1.2 @ 0x0049c550: OK, CANCEL (the backdrop freed after either),
// SKIN_LIST's change its image.
void TeamScreen::on_button(int32_t id, int32_t, int32_t) {
  if (id == 2) {
    show_skin();
    return;
  }
  if (id != 0 && id != 1) return;
  if (id == 0) ok();
  else back();
  if (still_tex) ztwod::host().image_release(still_tex);  // 0x00517b10
  still_tex = nullptr;
  backdrop.set_image(static_cast<zimg::Texture*>(nullptr));
}

// Mech3.exe 1.2 @ 0x0049c680: the chat's bar the keyboard again; the lobby.
void TeamScreen::back() {
  if (!state || !lobby) return;
  lobby->chat_edit.set_focus(0);
  lobby->chat_edit.set_focus(1);
  engine().multiplayer_page(state, 2, 0);
}

// Mech3.exe 1.2 @ 0x0049c6c0: a new team needs a name (0x1e5) no team has
// (0x1e6, 0x00589680), then is made with SKIN_LIST's skin, joined with JOIN
// (0x0049a300); else our skin (0x0049a850); back to the lobby.
void TeamScreen::ok() {
  if (!state || !lobby) return;
  Engine& e = engine();
  if (lobby->team_new) {
    std::string name = name_edit.get_text() ? name_edit.get_text() : "";
    const size_t a = name.find_first_not_of(" \t\r\n");
    name = a == std::string::npos ? "" : name.substr(a, name.find_last_not_of(" \t\r\n") - a + 1);
    const int32_t why = name.empty() ? 0x1e5 : znet::team_named(name.c_str()) ? 0x1e6 : 0;
    if (why) {
      cursor_obj.erase();
      e.message_box(0x30, 0x1e4, why);
      set_flags_all(0);
      return;
    }
    lobby->new_team(name.c_str(), skins.selected_value(), join_box.checked != 0);
  } else {  // PM 0x0049fed0: whenever not a new team
    lobby->set_skin(skins.selected_value());
  }
  lobby->chat_edit.set_focus(0);
  lobby->chat_edit.set_focus(1);
  engine().multiplayer_page(state, 2, 0);
}

// The host's lines (the connection screen's, mp_connect.cpp); none for a
// player who joined.
void LobbyScreen::show_reach() {
  if (!znet::is_host()) {
    internet_line.show("", "");
    lan_line.show("", "");
    return;
  }
  const Engine::NetReach r = engine().network_reach();
  const std::string line = reach_line(r);
  if (line != internet_line.line) internet_line.show(line, reach_copy(r));
  const std::string lan = r.lan.empty() ? "" : "LAN: " + r.lan + "  - click to copy";
  if (lan != lan_line.line) lan_line.show(lan, r.lan);
}

}  // namespace mech3
