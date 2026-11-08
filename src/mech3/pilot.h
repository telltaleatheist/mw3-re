#pragma once
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "gamez/zgame/unported.h"
#include "mech3/mech_config.h"

namespace zrdr {
struct Node;
}

namespace mech3 {
class StandaloneGame;
}

// The pilot (docs/pilot.md): the player's record at [0x0060861c], one per
// file in pilots\, and the five games it owns (the campaign saves, Instant
// Action, multiplayer, training and the mech lab's), with their stores of
// mechs and parts. Original files unknown: the pilot's functions sit between
// mech3\mech_skin.cpp and mech3\mech_waynet.cpp (0x004779f0..0x004794f0),
// the games and the slot between mech3\mech_data.cpp and mech3\mech_map.cpp
// (0x00433cc0..0x004379e0, by the option screens), the store between
// mech3\mech_salvage.cpp and mech3\mech_skin.cpp (0x00470ce0..0x00473400).
// The original's Win32 file handle is a std::FILE* here (FIX (A), pilot.md).
namespace mech3 {

using zgame::Handle;
class Pilot;

// The calls out of the pilot module into parts not ported, by address.
class PilotHooks : public zgame::Unported {
 public:
  // Fills a store with everything the mech lab offers (0x00470e30: every
  // mech, weapon, ammo and equipment the tech tables allow; marked `full`).
  // `game` is the kind of the game being made (GameType).
  virtual void store_fill(class Store& s, int32_t game);
  // A new campaign's starting mech (0x004733b0): `mech` with the dfn's
  // configuration named `config` added to the store; 1 on success.
  virtual int32_t store_add_mech(class Store& s, int32_t mech, const char* config);
  // The slot's saved configuration (0x004242a0 with the name "temp", and
  // 0x00424480): the ported Config's load and save by default.
  virtual Config* config_load(const char* name, std::FILE* f);
  virtual void config_save(Config* c, std::FILE* f);
  virtual const char* message(int32_t id);  // 0x00576040, Mech3Msg.dll's text
  virtual const char* message_named(const char* name);  // Pirate's Moon 0x00596db0: the message of that name
  // 0x00575fd0: message `id` with FormatMessage's %1!d!, %2!d! ... filled.
  virtual std::string format_ints(int32_t id, std::initializer_list<int32_t> args);
  virtual int32_t campaign_missions(int32_t campaign);  // 0x00484840, ported
  // A slot's configuration copied (0x00424010(store 0, flag 0): ECX the
  // name, EDX the configuration) and its free slots counted again
  // (0x00425bd0): the ported Config's by default.
  virtual Config* config_copy(const Config& c);
  virtual void config_recount(Config& c);
  // The mission's salvage ([0x00609990], a store the mission fills).
  virtual class Store* mission_salvage() { return nullptr; }
  // 0x005a8a70: zSave writes the world into a saved game (`kind` 2 here);
  // the ported zSave by default.
  virtual void save_world(const char* path, int32_t kind);
  // 0x005a8a90: zSave reads a saved game into the world; false when it did not.
  virtual bool load_world(const char* path, int32_t kind);
  // 0x004ac150: the game's message box; 1 for yes.
  virtual int32_t message_box(int32_t type, int32_t caption, int32_t text) {
    return static_cast<int32_t>(unported("0x004ac150", {type, caption, text}));
  }
};
void set_pilot_hooks(PilotHooks* h);
PilotHooks& pilot_hooks();

// The store (0x00470ce0, vtable 0x005c36e0), 0x14 bytes: a list of items
// (the loadouts' kinds, mech_config.h) with their counts. A `full` store is
// the mech lab's everything (store_fill) and saves as no items.
class Store;
// 0x00424010 (ECX the name, EDX the loadout, then the store and `take`): a
// copy; with a store, cut to the items it holds (then modified, the sinks
// recounted) and with `take` taken out of it.
Config* config_copy_store(const char* name, const Config& c, Store* store, bool take);

class Store {
 public:
  explicit Store(uint8_t flag = 0);  // 0x00470ce0
  ~Store();                          // 0x00470d20
  Store(const Store&) = delete;
  Store& operator=(const Store&) = delete;
  ConfigItem* add_copy(const ConfigItem* item, int32_t n);  // vt+0x00  0x00472630
  EngineItem* add_engine(int32_t engine, uint8_t xl);       // vt+0x04  0x00471b20: one
  AmmoItem* add_ammo(int32_t weapon, int32_t n);            // vt+0x08  0x00471940
  EquipmentItem* add_equipment(int32_t kind, int32_t tech, int32_t n);  // vt+0x0c  0x00471a20
  WeaponItem* add_weapon(int32_t weapon, int32_t tech);     // vt+0x10  0x00471840: one
  bool remove(ConfigItem* item, int32_t n);                 // vt+0x14  0x00471d20
  void remove_mech(int32_t mech);                           // 0x00472a30: one of the mech
  ConfigItem* find(const ConfigItem* item);   // 0x00471460: the item itself, else one like it
  ConfigItem* find_like(const ConfigItem* item);  // 0x004714c0: by vt+0x24
  void add_all(const Store& from);          // 0x004732b0: each of its items, by count
  void assign(const Store& from);           // 0x00473300: emptied, then add_all
  BaseItem* add_mech(int32_t mech);         // 0x00471c10: one of the mech (not -1 or PM's 0x19)
  bool add_config(const Config* c);         // 0x00472cf0: each item of the loadout, by count (false: none)
  bool remove_config(const Config* c);      // 0x00472d40: each item of the loadout, by count
  ConfigItem* has_mech(int32_t mech);       // 0x00409f00: the mech's item, or none
  int32_t count_like(const ConfigItem* item) const;  // 0x00473510
  // 0x004735f0: the store (and `other`'s, given) holds the mech (unless -1)
  // and every item of the loadout by count; a full store or no loadout:
  // true (the exe's `other` == loadout test never holds).
  bool has_config(int32_t mech, const Store* other, const Config* c);
  bool has_config(int32_t mech, const Config* other, const Config* c);  // the slot's loadout as `other`
  bool take_config(int32_t mech, const Config* c);  // 0x00473440: the mech and the items out (not a full store)
  bool give_config(int32_t mech, const Config* c);  // 0x00473400: back in
  void clear();                             // the items deleted, the list emptied
  void fill_all();                          // 0x00470e30: everything the lab offers, `full` set
  uint32_t save(std::FILE* f) const;        // 0x00471790 -> 0x004717a0
  void load(std::FILE* f);                  // 0x00471530 -> 0x00471540
  int32_t count() const { return static_cast<int32_t>(items.size()); }

  uint8_t b04;                       // +0x04  the constructor's flag
  std::list<ConfigItem*> items;      // +0x08, its count +0x0c
  uint8_t full = 0;                  // +0x10  set with b04, and by store_fill
};

// Pirate's Moon's lobby settings, as the lobby screen's lists and boxes give
// them (0x0049ef20, 0x0049e400): a list's pick (none: no item selected).
struct LobbyPick {
  bool has = false;
  int32_t value = 0;
};
struct LobbySettings {
  LobbyPick kills, minutes, tons, realloc, rounds, kind, world;  // KILLS, TIME, WEIGHT, REALLOC, ROUNDS, the kinds, SCENARIO
  bool respawn = false, ammo = false, outside = false, jets = false, radar = false, stock = false, mobile = false;
};
// What 0x0049e400 decides when the host locks the options.
enum class LobbyRule {
  start,              // locked: team play set (any kind but 0)
  one_base_unlimited, // "Attack/Defend with one base requires a time limit or a score limit"
  declined,           // a confirmation (0x1e3 with 0xfbd no limits, or 0x2c4) answered no
  two_bases_limited,  // "...two bases requires unlimited time and unlimited score." (both lists to 1000)
  no_respawn,         // "Attack/Defend games and 'No RESPAWN' are incompatible." (respawn turned on)
};
LobbyRule check_lobby_rules(const LobbySettings& s, bool (*confirm)(int32_t text), bool* team_play);

// Pirate's Moon: a game's vt+0x44, the jump jets' assist: the multiplayer
// game's 0x004365f0 (its +0xa7) when it is `g`, else every other game's
// 0x00435070 (the MW2 jump jets option, 0x0040afd0, the other way round).
bool jets_assist_of(const class MultiplayerGame* g);

// Pirate's Moon's rules for a multiplayer round (0x00438380), from
// mprounds.zrd: 8 numbers by place (the defaults [0x0060554c] when its block
// has none) and the salvage the round gives.
struct RoundRules {
  float table[8] = {70, 50, 40, 35, 30, 25, 20, 15};  // +0x00
  Store salvage{0};                                    // +0x20
  float reward(int32_t place, float scale) const;     // 0x004384f0
};
// 0x00438380: "round<round>" from mprounds.zrd, else the `fallback` block.
void read_round_rules(RoundRules& r, const char* fallback, int32_t round);
void read_round_rules(RoundRules& r, zrdr::Node* root, const char* fallback, int32_t round);  // the file's tree given
bool read_round_block(RoundRules& r, zrdr::Node* block);  // 0x00438460: false without a block
extern void (*round_salvage)(Store& s, zrdr::Node* salvage);  // salvage_into (0x00473690); a test's stand-in
extern void (*round_rules_read)(RoundRules& r, const char* fallback, int32_t round);  // read_round_rules; a test's

// A mech in a game (0x00433cc0, vtable 0x005bec38), 0x18 bytes: the player's
// own (a game's +0x20), the campaign's lancemates, Instant Action's and the
// mech lab's line-ups. Its configuration is its own (deleted with it).
struct GameSlot {
  explicit GameSlot(int32_t mech = -1, Config* config = nullptr, int32_t skin = 0);  // 0x00433cc0
  ~GameSlot();                       // 0x00433d60
  GameSlot(const GameSlot&) = delete;
  GameSlot& operator=(const GameSlot&) = delete;
  void save(Pilot& p);               // vt+0x00  0x00433ef0
  void load(Pilot& p);               // vt+0x04  0x00433f80
  void copy_from(const GameSlot& s); // 0x00433d90
  bool set_config(const Config* c);  // 0x00433df0(c, 0, 0): a copy of c, or none
  // 0x00433df0 with a store: the old loadout's items given back, the new
  // one copied out of the store (`take`), cut to what it holds.
  bool set_config(const Config* c, Store* store, bool take);

  int32_t mech;                      // +0x04
  Config* config = nullptr;          // +0x08
  int32_t x0c = 0;                   // +0x0c
  int32_t skin;                      // +0x10
  int32_t x14 = 0;                   // +0x14
};

enum GameType : int32_t { kMechLab = 0, kCampaign = 1, kInstantAction = 2, kMultiplayer = 3, kTraining = 4, kStandalone = 5 };

// A game of the pilot's (vtable 0x005beca0, docs/gametypes.md): its store,
// the player's mech, and what its kind adds. Only the slots the pilot and its
// screen use are virtual here.
class Game {
 public:
  explicit Game(Pilot* p);           // the shared head of every constructor
  virtual ~Game() = default;         // vt+0x00  0x004377a0 (deleting 0x00437840)
  virtual void save();               // vt+0x18  0x00434560: the slot, then the store
  virtual void load();               // vt+0x1c  0x00434580
  // vt+0x28 (0x004b3d30): the mission load's "restored from a saved game?";
  // true for every game but the campaign's (docs/save.md).
  virtual bool restore() { return true; }
  virtual int32_t type() const { return -1; }  // vt+0x2c  0x004a5260
  // vt+0x04 .. +0x10, the briefing's game_type, db_name, scenario_name and
  // scenario_desc (campaign.md section 3): 0x0040bae0's 0 unless a game has its own.
  virtual const char* game_text(int32_t slot) { (void)slot; return nullptr; }
  virtual GameSlot* player_slot() { return &slot; }  // what the launch and the lab read

  Pilot* pilot;                      // +0x04
  Store store;                       // +0x08
  std::string text;                  // +0x1c  (a CString: the campaign's name or its file)
  GameSlot slot;                     // +0x20  the player's mech (PM: 5 at first)
};

// "Mech lab" (0x004373e0, vtable 0x005bee08), 0x84 bytes: the mech lab's and
// the debug test levels' game, the startup default. Saves only the head.
class LabGame : public Game {
 public:
  explicit LabGame(Pilot* p);        // 0x004373e0
  ~LabGame() override;               // 0x004374d0
  int32_t type() const override { return kMechLab; }  // 0x0040bae0
  int32_t level = 7;                 // +0x38  vt+0x50 (0x004379e0) sets it: 6 the lab, 7, 8 the dev levels
  GameSlot lineup[3];                // +0x3c, +0x54, +0x6c  mechs 3, 8, 13
};

// A campaign (0x004345a0 new, 0x00434820 read from the file; vtable
// 0x005bec40), 200 bytes. The pilot's first is a new one; the others are
// the saved games, each kept only while its file in pilots\games exists.
class CampaignGame : public Game {
 public:
  CampaignGame(Pilot* p, int32_t campaign, int32_t mission);  // 0x004345a0
  explicit CampaignGame(Pilot* p);   // 0x00434820: read from the pilot's file
  ~CampaignGame() override;          // 0x00434740
  void save() override;              // vt+0x18  0x00434990
  void load() override;              // vt+0x1c  0x00434ae0
  int32_t type() const override { return kCampaign; }  // 0x00499270
  const char* game_text(int32_t slot) override { return slot == 0x0c ? name() : nullptr; }
  const char* name();                // vt+0x0c  0x00434900: "Op N, Mission M" (into text)
  const char* path();                // vt+0x14  0x00434980: its saved game (into text)
  const char* path_for(const char* pilot_name);  // 0x00434950
  bool readable();                   // vt+0x20  0x00435330: its saved game exists
  bool restore() override;           // vt+0x28  0x004352d0: its saved game's world read (zSave)
  void merge_salvage();              // 0x00434cb0, at the end of load
  int32_t operation() const { return campaign; }  // vt+0x48  0x00437870
  int32_t mission_number() const { return mission; }  // vt+0x4c  0x00437880
  // `save` false: the world's save left to the caller (record_mission's FIX).
  CampaignGame* next_mission(bool next, bool save = true);  // vt+0x24  0x00434e00
  GameSlot* slot_at(int32_t i);      // vt+0x34  0x00435110: 0 the player's, then the lancemates
  int32_t slot_count() const;        // vt+0x38  0x00435180 (vt+0x3c 0x00437820 the same)
  // The mission start's lancemate i (player_setup): appended when it is the
  // next, else the slot there keeps its mech and loadout and takes only the
  // new one's record index (+0x14), and the new one is deleted.
  void set_lancemate(int32_t i, GameSlot* s);  // 0x004351c0
  void set_lancemate_count(int32_t n) { x9c = n; }  // 0x00435230
  bool keeps();                      // vt+0x1c, PM 0x00435d80
  void reset_lancemates(bool del);   // 0x00435240
  void copy_lancemates(const CampaignGame& from);  // 0x00434300

  uint8_t b38 = 0;                   // +0x38
  Store store3c, store50, store64, store78;  // +0x3c, +0x50, +0x64, +0x78
  std::vector<GameSlot*> lancemates; // +0x90 (+0x8c its allocator byte)
  int32_t x9c = 0;                   // +0x9c  the mission's lancemates (0x00435230; never set by the constructors)
  int32_t mission = 0;               // +0xa0
  int32_t campaign = 0;              // +0xa4  the operation, 1..4
  int32_t a8 = 3;                    // +0xa8  set to +0xac on loading
  int32_t ac = 3;                    // +0xac
  uint8_t b0 = 0;                    // +0xb0
  int32_t b4[4] = {};                // +0xb4
  int32_t c4 = 0;                    // +0xc4
  std::vector<std::string> strings;  // PM +0xc4  a CStringList, kept with the record (each: length, bytes)
};

// One of Pirate's Moon's standalone missions (0x0040d920, 0x20 bytes), from its reader's sainfo.zrd.
struct StandaloneMission {
  std::string archive;               // +0x00  zbd\c<world>\readersa<number>.zbd
  std::string name, text;            // +0x04, +0x08  db_name's and db_verbose's messages
  zrdr::Node* info = nullptr;        // +0x10  sainfo.zrd while the mission is open
  int32_t order = 0;                 // +0x14
  int32_t world = 0, number = 0;     // +0x18, +0x1c
};
std::vector<StandaloneMission*>& standalone_missions();  // 0x00618848
int32_t standalone_read_missions();  // 0x0040def0: the readers found, by `order`

// Pirate's Moon's standalone game, the second campaign (0x40e100, vtable 0x5df148, 0x5c
// bytes; pirates-moon.md). Its store is rebuilt at each mission's start from the
// mission's salvage block and the lance's mechs and loadouts.
class StandaloneGame : public Game {
 public:
  explicit StandaloneGame(Pilot* p);  // 0x0040e100
  ~StandaloneGame() override;
  void save() override;              // 0x0040e250
  void load() override;              // 0x0040e2b0
  int32_t type() const override { return kStandalone; }
  const char* game_text(int32_t slot) override;  // vt+0x08, +0x0c (0x0040e320, 0x0040e330)
  GameSlot* player_slot() override { return slot_at(0); }
  bool set_mission(int32_t i);       // 0x0040e340: within the list, true when it had to be moved
  bool start();                      // vt+0x64  0x0040e3f0: the reader, the store, the lineup
  void close();                      // vt+0x68  0x0040e5e0 (0x0040daa0)
  int32_t unlocked() const;          // 0x0040e4d0: the missions listed
  bool advance();                    // 0x0040e510: the next mission, the pilot saved; true past the last
  int32_t number() const;            // vt+0x48  0x0040e660
  int32_t world_index() const;       // vt+0x50  0x0040e310
  zrdr::Node* block(const char* key) const;  // vt+0x6c..+0x74: briefing, objectives, environment
  GameSlot* slot_at(int32_t i);      // vt+0x30  0x0040e760
  int32_t slot_count() const;        // vt+0x34  0x0040e830
  void set_lancemate(int32_t i, GameSlot* s, const Config* config);  // 0x0040e6c0
  void set_lancemate_count(int32_t n) { x48 = n; }  // 0x0040e6a0
  int32_t x38 = 0, x3c = 0;          // +0x38, +0x3c  the mission, the best reached (0x0040e510)
  StandaloneMission* x40 = nullptr;  // +0x40
  int32_t x48 = 0;                   // +0x48  the lancemates in the mission (0x0040e6a0)
  std::vector<std::vector<GameSlot*>> lineups;  // +0x50, one list a mission (0x0040dc00, 0x0040dc70)
  int32_t lineup = -1;               // +0x44  this mission's (0x0040de60)
};

// The Instant Action settings the game keeps outside the game object and the
// pilot's file carries ([0x00605820..0x0060587f]; the Action screen's).
struct ActionSetup;
ActionSetup& action_setup();

struct ActionSetup {
  int32_t g820[5] = {-1, -1, -1, -1, -1};  // 0x00605820 (PM: per pilot, its game's +0x40, -1 by 0x0040c460)
  int32_t g838[6] = {-1, -1, -1, -1, -1, -1};  // 0x00605838 (PM: -1, 0x0040c460)
  int32_t g858[6] = {};              // 0x00605858
  int32_t g874 = 0, g878 = 0, g87c = 0;  // 0x00605874, 0x00605878, 0x0060587c
};
// Instant Action (0x00436650, vtable 0x005bed50), 0x17c bytes: its record.
class ActionGame : public Game {
 public:
  explicit ActionGame(Pilot* p);     // 0x00436650
  ~ActionGame() override;            // 0x00436770
  void save() override;              // vt+0x18  0x00436b50
  void load() override;              // vt+0x1c  0x00436cc0
  int32_t type() const override { return kInstantAction; }  // 0x004993e0
  // 0x004378a0 (message 0x10c), 0x004487d0 (+0x38), 0x0040a840 (+0x3c), 0x00437970 (+0x40)
  const char* game_text(int32_t slot) override;

  // The line-up (0x0049dc80's): slot i (vt+0x34; 0 the player's, then
  // lineup[i - 1]), allies first, then enemies (the screen keeps them so).
  GameSlot* slot_at(int32_t i);      // vt+0x34  0x004368b0
  int32_t slot_count() const { return count + 1; }  // vt+0x38  0x00436a30
  bool ally(int32_t i) const;        // 0x00436a90: slot i is on the player's side (0 is)
  bool remove(int32_t i);            // 0x00436af0: lineup[i] (0-based) out, the rest moved up
  void drop_enemies();               // 0x00436ac0
  // A slot appended: its mech, side, a copy of the loadout and its paint;
  // null when the ten are taken.
  GameSlot* add(int32_t mech, uint8_t side, const Config* config, int32_t paint);  // 0x00436840
  // 0x004368e0 (+0x38), 0x00436950 (+0x3c), 0x004369c0 (+0x40): a copy of
  // `text` (the Action screen's world, the scenario's name and its description).
  void set_text(char** field, const char* text);

  char* x38 = nullptr;               // +0x38  the world's name (malloc'd by the Action screen, freed here)
  char* x3c = nullptr;               // +0x3c  its name (vt+0x0c, 0x0040a840: the objectives' header)
  char* x40 = nullptr;               // +0x40  its description
  ActionSetup setup;                 // PM +0x38..+0x84
  int32_t mode = 0;                  // +0x44  1 endless, 2 waves (the Action screen's; not set by the constructor)
  uint8_t field_bases = 0;           // +0x48  the mission's mobile HQs made (the screen's)
  // +0x4c..+0x68, the pilot screen's lines (messages 0x184..0x18a, 0xdb7).
  int32_t player_kills = 0, lancemate_kills = 0, enemy_kills = 0;
  int32_t attack_wins = 0, attack_losses = 0, defend_wins = 0, defend_losses = 0;
  int32_t waves = 0;                 // +0x68  progression waves (the best wave in wave mode)
  int32_t x6c = 1;                   // +0x6c  the wave being fought
  int32_t x70 = 0;                   // +0x70  waves won (not set by the constructor)
  int32_t x74 = -1, x78 = 0;         // +0x74, +0x78
  int32_t count = 0;                 // +0x7c  line-up slots in use
  GameSlot lineup[10];               // +0x80, 0x18 each
  uint8_t side[10] = {};             // +0x170  each slot's: 1 an ally
};

// [0x00608e2c]: the game is team play (the connection screen's, mp_connect.h).
bool multiplayer_team_play();

// The lobby's records (docs/multiplayer-port.md, piece 11), kept by the
// multiplayer game and made again from the network's players and teams at
// each refresh (0x00498710). A score line (0x10 bytes): its text for
// SCORES_LIST and the order it sorts by.
struct ScoreLine {
  std::string text;                  // +0x00
  int32_t score = 0;                 // +0x04  kills less suicides, not under 0 (0x004361e0)
  uint32_t key = 0;                  // +0x08, PM +0x10  the lines' order, largest first (0x00435b60)
  int32_t kind = 0;                  // +0x0c, PM +0x14  1 a player's, 2 a team's
  int32_t rank = 0;                  // PM +0x0c  its place, 0 first, ties sharing it (0x004373e0)
};

// A player or a team of the lobby's lists (vtable 0x005c7cc4); the lists'
// items point at them. vt+0 tells which.
struct LobbyEntry {
  virtual ~LobbyEntry() = default;
  virtual int32_t kind() const = 0;  // vt+0x00  1 a player (0x00499270), 2 a team (0x004993e0)
  uint32_t id = 0;                   // +0x04
};

// A player (0x00499030, vtable 0x005c7cc0), 0x30 bytes: its network record
// and data (the lobby's kind 1: skin and state, multiplayer-port.md 11).
struct LobbyPlayer : LobbyEntry {
  int32_t kind() const override { return 1; }
  uint32_t team = 0;                 // +0x08  0 none
  uint8_t host = 0;                  // +0x0c
  uint8_t out = 0;                   // +0x0e  PM: out of the round (0x004379b0, from the kill's scoring 0x004721e0)
  std::string name;                  // +0x10
  int32_t state = 2;                 // +0x14  1 in the mech lab, 3 ready (its data's; 2 without)
  uint8_t left = 0;                  // +0x18  gone from the game (0x00495b10's users)
  int32_t skin = 1;                  // +0x1c  1..8 (its data's)
  int32_t order = 0;                 // +0x20  its place in the lobby's count ([0x005e7920])
  int32_t kills = 0;                 // +0x24
  int32_t deaths = 0;                // +0x28  its suicides
  int32_t rounds = 0;                // PM +0x2c  rounds won (0x00436980); 0 when it leaves (0x0046f440)
  int32_t place = 0;                 // PM +0x30  its line's rank, or its team's (0x004373e0); its data's word 5
  ScoreLine* score = nullptr;        // +0x2c, PM +0x34  the game's
};

// A team (0x00499280, vtable 0x005c7cc8), 0x20 bytes; its data (8 bytes) the
// owner and the skin.
struct LobbyTeam : LobbyEntry {
  int32_t kind() const override { return 2; }
  std::string name;                  // +0x08
  int32_t count = 0;                 // +0x0c  its players, counted by the lists (0x00498710)
  int32_t skin = 0;                  // +0x10  its data's second word
  int32_t kills = 0;                 // +0x14  its players' (0x004361e0)
  uint32_t owner = 0;                // +0x18, PM +0x28  the first to join it; its data's first word
  int32_t word2 = 0;                 // PM +0x18  its data's third word (0x0049db90's last): the team's number (0x00436600; MW3 used the skin), bit 0 its side
  int32_t rounds = 0;                // PM +0x20  rounds won (0x00436980), its data's fourth word between rounds
  int32_t place = 0;                 // PM +0x24  its line's rank (0x004373e0)
  ScoreLine* score = nullptr;        // +0x1c, PM +0x2c  the game's
};

// Multiplayer (0x004353a0, vtable 0x005becf8), 0xb0 bytes.
class MultiplayerGame : public Game {
 public:
  explicit MultiplayerGame(Pilot* p);  // 0x004353a0
  ~MultiplayerGame() override;       // 0x00435500
  void save() override;              // vt+0x18  0x00435690
  void load() override;              // vt+0x1c  0x004357d0
  int32_t type() const override { return kMultiplayer; }  // 0x0041ec30
  GameSlot* slot_at(int32_t i) { return i == 0 ? &slot : nullptr; }  // vt+0x34  0x00437800
  int32_t world() const { return x94 << 8 | x90; }  // vt+0x4c (0x00437940) << 8 | vt+0x54 (0x00437910)
  // The briefing's texts: "M U L T I P L A Y E R" (vt+0x04, 0x00437920), the
  // world's name (vt+0x08, 0x00436500: a shipped one's message, an add-on's
  // mpinfo.zrd db_name, else "Unknown"), the session's name (vt+0x0c,
  // 0x00437950), the players as START listed them (vt+0x10, 0x00437930).
  const char* game_text(int32_t slot) override;
  void set_roster(const std::vector<std::string>& lines);  // 0x00436470: each line and "\n"

  // The lobby's records by player or team id (0x00495b10..0x00495d20 on
  // +0x48 and +0x54), and their lists emptied (0x00495a50, 0x00495c60,
  // 0x00495d70 on +0x38).
  LobbyPlayer* lobby_player(uint32_t id);       // 0x00495b10: null none
  uint32_t lobby_team_id(uint32_t player);      // 0x00495b60: its team, 0 none
  int32_t lobby_skin(uint32_t player);          // 0x00495bb0: 0 none
  bool in_team(uint32_t player, uint32_t team);  // 0x00495c00
  LobbyTeam* lobby_team(uint32_t id);           // 0x00495d20: null none
  LobbyTeam* team_numbered(int32_t number);     // PM 0x00436650: the team whose number (+0x18) it is
  void clear_lobby();                           // the three, as the lists' refresh does
  // The network game's lookups and scores (0x00435910..0x00435b40, 0x004361e0,
  // 0x00435b60). A player's side in play: its place in the lobby's count,
  // or in team play its team's skin (a team is its colour); 0 none.
  uint32_t net_team(uint32_t player);           // PM 0x00436600 (MW3 0x00435910)
  int32_t net_skin(uint32_t player);            // 0x00435960: its own skin, or its team's in team play
  int32_t add_kills(uint32_t player, int32_t n);   // 0x004359b0: -1 no such player
  int32_t set_kills(uint32_t player, int32_t n);   // 0x004359f0
  int32_t kills_of(uint32_t player);               // 0x00435a30
  int32_t score_of(uint32_t player);               // 0x00435a50: its team's in team play
  int32_t add_deaths(uint32_t player, int32_t n);  // PM 0x00436820: only while kills exceed deaths; 0 not counted
  bool set_out(uint32_t player, bool out);          // PM 0x004379b0: the record's +0x0e, unless respawning
  void round_reset();                                // PM 0x00437a10
  int32_t set_deaths(uint32_t player, int32_t n);  // 0x00435b00
  int32_t deaths_of(uint32_t player);              // 0x00435b40
  void rescore(bool rounds = true);                // PM 0x004374f0 (MW3's 0x004361e0)
  void rank();                                     // PM 0x004373e0
  void award_side(bool side);                      // PM 0x00436d50
  bool tally(bool award);                          // PM 0x00436980
  bool round_winner();                             // PM 0x00437970
  void next_round(uint32_t place, int32_t round);  // PM 0x00438580
  int32_t spawn_base(uint32_t player);             // PM 0x00437a40: the first "remote_N" for the player's side
  bool base_counts(int32_t base);                  // PM 0x00437920: a destroyed base `base` (1, 2) ends the round
  bool base_destroyed(int32_t base, float now);    // PM 0x00472b40's: reason 6, +0x8c, the clock sent now
  // PM's helpers for the lobby screen (0x0049a3c0, 0x0049b5e0) and the objectives (piece 4).
  std::string objectives_key();                    // vt+0x58 0x00436580: "mp<+0xb0>"
  int32_t scenario_message();                      // vt+0x08 0x00437850: the world's text id
  int32_t enemies_of(uint32_t player);             // 0x004368d0: the others not on its team
  int32_t award_team(uint32_t player);             // 0x00436930: a round to each on its team; how many
  int32_t runner_up_rounds();                      // 0x00437380: the most rounds won short of `best`
  int32_t rounds_of(uint32_t player);              // 0x004366d0: its +0x2c, 0 unlisted
  int32_t place_of(uint32_t player);               // 0x004366f0: its +0x30, 0 unlisted
  // PM 0x0049ef20's game half (the host's start; everyone's then): the settings into the game; the clock's on
  // and its seconds out ([0x0062da98], [0x0062da94]).
  void apply_lobby(const LobbySettings& s, bool* clock_on, float* clock_left);
  std::vector<uint8_t> start_message();            // PM 0x0049ef20's 0x1e: each player's id, order, team number
  void score_texts(int32_t name_width, int32_t number_width);  // PM 0x00436db0 (MW3 0x00435b60): the lines, sorted
  // PM 0x0049b730, the lobby screen's results list's lines: before any round
  // message 0x2bd; else "End of Round <played> of <total>" (the saved total
  // once `over`), the tie's and the game over's lines, then the score lines.
  std::vector<std::string> round_lines(int32_t played, bool extended, bool over, int32_t over_total);

  // +0x38..+0x5c: the lobby's (pointers in the exe's lists, owned here).
  std::vector<std::unique_ptr<ScoreLine>> score_lines;      // +0x3c (its allocator +0x38)
  std::list<std::unique_ptr<LobbyPlayer>> lobby_players;    // +0x4c, its count +0x50 (at +0x48)
  std::list<std::unique_ptr<LobbyTeam>> lobby_teams;        // +0x58, its count +0x5c (at +0x54)
  // +0x60: the rules as locked (0x0049aca0 the host's, 0x00499cf0 a
  // joiner's), with a random word, and their CRC-32 (+0x88): once for the
  // host's 51 s check, dropped as FIX (trap); kept, read by nothing.
  struct Rules {
    uint32_t flags = 0;              // +0x60  the 0x20 message's (1 respawn .. 0x20 locked)
    int16_t world = 0, kills = 0, time = 0, weight = 0;  // +0x64..+0x6a  the lists' values
    int32_t seed = 0;                // +0x6c  rand()
  } rules;
  uint32_t rules_crc = 0;            // +0x88
  int32_t pmcc = 0;                  // PM +0xcc  Total Shots on the pilot's screen, one per shot (0x00471adf)
  uint8_t radar = 1;                 // PM +0xa8  the rules' 0x100 (RADAR_CHECKBOX's, by elimination: piece 6)
  int32_t x70 = 0;                   // +0x70, PM +0x78  KILLS_LIST's: the score to win (never set by the constructor; saved)
  float x74 = 10.0f;                 // +0x74, PM +0x7c  TIME_LIST's: minutes
  int32_t x78 = 90;                  // +0x78, PM +0x80  WEIGHT_LIST's: tons
  // The network game's clock and end (PM 0x00471040, MW3 0x0046ef70); the start clears them.
  float clock_next = 0;              // PM +0x84 (MW3 +0x7c): when the host sends the clock (0x11) again
  float end_time = 0;                // PM +0x88 (MW3 +0x80): the round over then (not set by the constructor)
  int32_t end_state = 0;             // PM +0x94 (a byte): 1 the round ending (0x00472850); MW3 +0x84 0, 1, 3
  int32_t round_end = 0;             // PM +0x90: why the round ended (0x00472850's 1..6), 0 playing
  uint32_t clock_from = 0;           // PM +0xa0: the last clock's (0x11) sender, the host (0x00472850)
  uint8_t overtime_shown = 0;        // PM +0xab: the overtime line shown (0x00472850), cleared by the start
  // PM's rounds (0x00436980): the game's kind, the base destroyed and the side the vt+0x48 names.
  int32_t kind = 0;                  // PM +0x98  0, 1 deathmatch; 3, 4 a base or two (piece 4)
  int32_t base_lost = 0;             // PM +0x8c  the base destroyed, 1 or 2 (0x00472b40); cleared by the start
  uint8_t winner_set = 0;            // PM +0xaa  a winner already (0x00437970); cleared by the start
  uint8_t jets_assist = 1;           // PM +0xa7  vt+0x44 (0x004365f0): the jump jets' assist (the MW2 option's other way round)
  // PM's between rounds (0x00438580).
  int32_t realloc = 0;               // PM +0x70  REALLOC_LIST's ("MSG_REALLOC_SALVAGE"): the rewards' scale, x2
  int32_t rounds_total = 0;          // PM +0x74  the rounds in the match (the lobby's 0x0049a3c0 extends it on a tie)
  float bonus_tons = 0;              // PM +0xd0  the places' rewards summed; read through vt+0x60 (0x00436020)
  Store round_store{0};              // PM +0xd4  what the players draw from between rounds
  uint8_t b8c = 1, b8d = 0, b8e = 0, b8f = 1;  // +0x8c..+0x8f; PM +0xa4..+0xa6 and +0xa9: respawn, ammo, outside, stock
  // The world, PM's scenario list's value: its folder (0 c1, PM +0xac, vt+0x50) and number (PM +0xb0, vt+0x48:
  // readermp<n>.zbd, "mp<n>" and the side whose base is base 1).
  int32_t x90 = 0, x94 = 1;          // +0x90, +0x94
  int32_t kills = 0, deaths = 0;     // +0x98, +0x9c  the pilot screen's line (0x27e)
  int32_t best = 0, best_count = 0;  // PM +0xbc, +0xc0 (MW3 +0xa0, +0xa4): the most rounds won and how many have it (0x00436db0)
  uint8_t ba8 = 0;                   // +0xa8, PM +0xc4  MOBILE_CHECKBOX's
  std::string roster;                // +0xac  (0x00436470)
};

// Training (0x00436e30, vtable 0x005beda8), 0x8c bytes.
class TrainingGame : public Game {
 public:
  explicit TrainingGame(Pilot* p);   // 0x00436e30
  ~TrainingGame() override;          // 0x00436f10
  void save() override;              // vt+0x18  0x00436fe0
  void load() override;              // vt+0x1c  0x00437020
  int32_t type() const override { return kTraining; }  // 0x00473a20
  // 0x00437980 (message 0x10b), 0x004487d0 (+0x38), 0x00437990 / 0x004379b0
  // (the mission's name and description, vt+0x4c 0x00437970 the mission)
  const char* game_text(int32_t slot) override { return text_of(slot, mission); }
  static const char* text_of(int32_t slot, int32_t mission);
  // A training mission's name and description (0x00437060, 0x004370d0: the
  // message, i within 1..4).
  static int32_t name_id(int32_t i);
  static int32_t text_id(int32_t i);

  int32_t x38 = 0;                   // +0x38
  uint8_t passed[4] = {};            // +0x3c  each training mission's PASS (mission n at +0x3b + n)
  int32_t mission = 1;               // +0x40  the one chosen, 1..4
  // +0x44: the mission's lessons (mech_objective.h), vt+0x40 0x00437140 reads
  // them, vt+0x44 0x004379d0 frees them.
  std::unique_ptr<struct TrainingLessons> lessons;
};

// The pilot (0x00477b20, vtable 0x005c37a0), 0x84 bytes; its file is
// pilots\<name>.mw3: the 12-byte header, then the games (pilot.md).
class Pilot {
 public:
  explicit Pilot(const char* name);  // 0x00477b20
  ~Pilot();                          // 0x00477e10: saved first while dirty
  Pilot(const Pilot&) = delete;
  Pilot& operator=(const Pilot&) = delete;
  bool read();                       // vt+0x00  0x004790d0
  bool write();                      // vt+0x04  0x00479390
  bool open();                       // 0x00478ce0: the file, its header checked or written
  bool write_header();               // 0x004794f0
  bool save();                       // the inline open / header / write / close of 0x004788b0 and others
  void close();
  std::string file_path() const;     // 0x00478c80: pilots\<name>.mw3
  bool rename(const char* name);     // 0x00477fe0
  bool delete_files();               // 0x004781e0: the campaigns' saved games, then its own
  void select_game(int32_t type);    // 0x00478250
  bool select_campaign(int32_t i);   // 0x00478ed0
  bool record_mission(bool next);    // 0x00478f10: true when the campaign is over
  Store* store();                    // 0x00479430: the current game's
  CampaignGame* campaign();          // 0x00479440: the current game when a campaign
  StandaloneGame* standalone_game() { return current && current->type() == kStandalone ? standalone : nullptr; }  // PM 0x0047b770
  ActionGame* action();              // 0x00479460
  MultiplayerGame* multiplayer();    // 0x00479480
  TrainingGame* training_game();     // 0x004794a0
  int32_t campaign_count() const { return static_cast<int32_t>(campaigns.size()); }

  int32_t magic = 1;                 // +0x04
  int32_t version;                   // +0x08  0x00423cf0 (15) + 0x20
  int32_t x0c = 0;                   // +0x0c
  std::FILE* file = nullptr;         // +0x10  (INVALID_HANDLE_VALUE when closed)
  Handle mech = 0;                   // +0x14  the pilot's mech in a mission
  char name[0x40] = {};              // +0x18
  uint8_t dirty = 1;                 // +0x58  saved by the destructor
  uint8_t b59 = 0;                   // +0x59
  std::vector<Game*> campaigns;      // +0x60  the new one first, then the saved (+0x5c its allocator byte)
  Game* current = nullptr;           // +0x6c
  ActionGame* action_game = nullptr; // +0x70
  MultiplayerGame* multi = nullptr;  // +0x74
  TrainingGame* training = nullptr;  // +0x78
  LabGame* lab = nullptr;            // PM +0x80
  StandaloneGame* standalone = nullptr;  // PM +0x7c
  int32_t x80 = 0;                   // +0x80  the mech's +0xd4 +0x1c, kept across a respawn
};

// The pilots ([0x0060860c], their count [0x00608610]) and the current one.
std::list<Pilot*>& pilots();         // 0x004779f0 makes the list
Pilot*& current_pilot();             // [0x0060861c]
Pilot* find_pilot(const char* name);  // 0x00478be0
bool same_name(const char* a, const char* b);  // pilot names that share a file
Pilot* load_pilot(const char* name, bool reload);  // 0x00478700
Pilot* create_pilot(const char* name);  // 0x004788b0: or the one already listed
bool delete_pilot(const char* name);  // 0x00477f50: and its files
void load_pilots();                  // 0x00478560: every pilots\*.mw3
void delete_other_pilots();          // 0x004785f0: all but the current one
Pilot* debug_start_campaign(int32_t campaign, int32_t mission);  // 0x00478a40, "DefaultPlayer"
// Mech3.exe 1.2 @ 0x00484870: the next mission, past the operation's last
// the next operation's first; true when the operation reaches 5 (the end).
// Pirate's Moon has no operations: the "operation" here is the mission's world + 1 (c1..c4).
bool campaign_advance(int32_t& operation, int32_t& mission);
constexpr int32_t kCampaignLastMission = 10;  // 0x00435db0
int32_t campaign_world(int32_t mission);      // 0x00435de0: 0..3, -1 out of range
bool campaign_fresh_world(int32_t mission);   // 0x00435e40: entered fresh, not carried on from the one before

}  // namespace mech3
