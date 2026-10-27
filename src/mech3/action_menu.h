#pragma once
#include <cstdint>
#include <vector>

#include "mech3/menu_widgets.h"

// The Action screen (docs/menus.md, The Action screen): Instant Action's
// setup after the pilot screen, ACTIONDIALOG of the main menu's dialog.zrd.
// The world, the scenario, the commander's mech, the mobile field base, three
// lancemates, and six enemies with their loadouts or a wave; ACCEPT fills the
// pilot's ActionGame and goes to the briefing. Original file unknown:
// 0x00488bc0..0x0048aac0, between mech3\mech_waynet.cpp and mech3\mission.cpp.
namespace mech3 {

class ActionScreen;
struct Config;
struct ConfigList;

// Instant Action's scenarios (0x005e6fb0, 16 bytes each; 0x005e7010 of them,
// the first "None"): the ia<n> mission, the mode (ActionGame +0x44: 0 battle,
// 1 waves, 2 progression, 3 attack, 4 defend), the name and the description.
struct ActionScenario {
  int32_t mission, mode, name, desc;
};
extern const ActionScenario kActionScenarios[6];
constexpr int32_t kActionScenarioCount = 6;  // 0x005e7010

// A cycle of the screen with the LABEL's two fonts (+0x84 0x00489320): `font`
// for a choice, `inactive_font` for none. DATABASE_LIST (vtable 0x005c5870),
// SCENARIO_DESC and ENEMY_TITLE (0x005c57c8), 0x42c bytes.
class ActionCycle : public CycleButton {
 public:
  explicit ActionCycle(Engine& e) : CycleButton(e, -1) {}
  bool read_control(zrdr::Node* node, ztwod::Dialog* dialog) override;  // +0x84  0x00489320

  Handle font = 0;                   // +0x424
  Handle inactive_font = 0;          // +0x428
};

// SCENARIO_LIST (vtable 0x005c5720): stepping keeps the old scenario's line-up
// in [0x00605820..], steps SCENARIO_DESC with it and fills the enemies anew.
class ScenarioList : public ActionCycle {
 public:
  explicit ScenarioList(Engine& e) : ActionCycle(e) {}
  void step(int32_t by) override;    // +0xa4  0x00489390
};

// A line-up cycle (+0x84 0x00489580): the fonts and its OFFSET, the next one's
// place from it (0x434 bytes).
class LineupCycle : public ActionCycle {
 public:
  explicit LineupCycle(Engine& e) : ActionCycle(e) {}
  bool read_control(zrdr::Node* node, ztwod::Dialog* dialog) override;  // +0x84  0x00489580

  int32_t offset_x = 0, offset_y = 0;  // +0x42c, +0x430
};

// ALLY_LIST (vtable 0x005c5918, 0x00489620), five of them: the commander's
// mech, the mobile field base, three lancemates; a choice of none in the
// inactive font.
class AllyList : public LineupCycle {
 public:
  explicit AllyList(Engine& e) : LineupCycle(e) {}
  void select(int32_t i) override;   // +0xa0  0x00489660
};

// ENEMY_CONFIG_LIST (vtable 0x005c5a68): an enemy's loadouts. FIX (A): a
// choice's value is its index in `configs` (the exe keeps the Config's address,
// which does not fit a 64-bit value).
class ConfigCycle : public LineupCycle {
 public:
  explicit ConfigCycle(Engine& e) : LineupCycle(e) {}
  const Config* config_at(int32_t i);  // the choice's loadout
  std::vector<const Config*> configs;
};

// ENEMY_LIST (vtable 0x005c59c0, 0x00489b90), six of them, each with its
// loadouts beside it (0x868 bytes the pair): choosing a mech lists its
// loadouts, or the wave's one.
class EnemyList : public LineupCycle {
 public:
  explicit EnemyList(Engine& e) : LineupCycle(e) {}
  void select(int32_t i) override;   // +0xa0  0x00489c10
  ConfigCycle* configs = nullptr;    // +0x434, the pair's second
};

// ENEMY_WAVE (vtable 0x005c5678): stepping shows that wave's enemies.
class WaveList : public LineupCycle {
 public:
  explicit WaveList(Engine& e) : LineupCycle(e) {}
  void step(int32_t by) override;    // +0xa4  0x00489710
  void fill(int32_t best);           // 0x004897b0: "Wave 1" up to the best + 1 (20 at most)
};

// The screen (vtable 0x005c5518), 0x17730 bytes, made with the Action State.
class ActionScreen : public MenuScreen {
 public:
  ActionScreen(Engine& e, Handle state);  // 0x00488bc0
  ~ActionScreen() override = default;     // 0x00489ee0 (deleting 0x00489b70)
  void on_button(int32_t id, int32_t a, int32_t b) override;  // +0x14  0x0048a680

  void reset();                      // 0x0048aac0: on entering, from the pilot's game and [0x00605820..]
  void accept();                     // 0x0048a140: the ActionGame filled, the pilot saved, the briefing
  void show_wave(int32_t wave);      // 0x0048a810: the enemies of the wave (0-based), fixed
  void show_custom();                // 0x0048a930: every mech for each enemy, chosen
  void keep_lineup();                // inline in 0x00489390 and 0x0048a140: the choices into [0x00605820..]

  ActionCycle worlds;                // +0x111ec  DATABASE_LIST (c1..c4, 0xe33..0xe36)
  ScenarioList scenarios;            // +0x11618  SCENARIO_LIST
  ActionCycle descriptions;          // +0x11a44  SCENARIO_DESC
  ShellButton ally_title;            // +0x11e70  ALLY_TITLE
  AllyList allies[5];                // +0x11fc8, 0x434 apart
  ActionCycle enemy_title;           // +0x134cc  ENEMY_TITLE
  EnemyList enemies[6];              // +0x138f8, 0x868 apart (each with its loadouts)
  ConfigCycle enemy_configs[6];      // +0x13d2c, 0x868 apart
  WaveList waves;                    // +0x16b68  ENEMY_WAVE
  int32_t x16f94 = 0, x16f98 = 0;    // +0x16f94, +0x16f98  (set by nothing else)
  uint8_t fixed = 0;                 // +0x16f9c  1: the enemies chosen; 0: a wave's
  int32_t wave = 0;                  // +0x16fa0  the wave shown (0-based)
  int32_t filling = 0;               // +0x16fa4  the enemy being filled (0x0048a810's loop)
  SlideButton cancel_btn;            // +0x16fa8  CANCEL (3)
  SlideButton accept_btn;            // +0x17194  ACCEPT (4)
  Marquee title;                     // +0x17380  (never bound)
  ztwod::Image lines{0};             // +0x17440  LINES
  ztwod::Image lines1{0};            // +0x174fc  LINES1
  ztwod::Image lines2{0};            // +0x175b8  LINES2
  ztwod::Image lines3{0};            // +0x17674  LINES3
};

}  // namespace mech3
