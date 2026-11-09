#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "mech3/menu_states.h"
#include "mech3/menu_widgets.h"
#include "mech3/mods_tables.h"

// Ours (Owen, 2026-11-08; docs/modding.md, The MODS screen): the main menu's
// MODS button and its screen, MODSDIALOG (our record edits,
// patch12::apply_ours) on the pilot screen's look. Two tables: CUSTOM
// CAMPAIGNS (each campaign= of every mod) and MODS (every other mod, on or
// off, in mw3.cfg's order, with its conflicts); ON/OFF, UP and DOWN write
// mw3.cfg's mod= lines and take hold at the next start. EDITOR is a stub for
// the Battlesport editor. Bound only where the program turns it on
// (mods_menu_page): the probes run the original's main menu.
namespace mech3 {

// ---- the screen ----

constexpr uint32_t kModsScreenCtor = 0xf0000003;  // ours: not the exe's addresses
constexpr uint32_t kModsStateCtor = 0xf0000004;
bool& mods_menu_page();       // the program turns the MODS button on
Handle& mods_menu_state();    // its menu object, made beside the main menu's
std::string& mods_menu_write_dir();               // where mods\ and mw3.cfg are
std::vector<std::string>& mods_menu_mounted();    // the mods on in this run

class ModsScreen;

class ModsList : public FileList {
 public:
  ModsList(Engine& e, int32_t id) : FileList(e, id) {}
  void select(int32_t index) override;  // and tells the screen (its id, the index)
};

class ModsScreen : public MenuScreen {
 public:
  enum Id : int32_t { kEnter = 1, kBack, kToggle, kUp, kDown, kEditor, kCampaigns, kMods };
  ModsScreen(Engine& e, Handle state);
  void on_button(int32_t id, int32_t a, int32_t b) override;
  void refill();                     // both tables again from the write dir
  void show_details();               // the selected row's lines
  void enter_campaign();             // ENTER

  SlideButton enter, back;           // ENTER, BACK
  ShellButton toggle, up, down, editor;  // ON/OFF, UP, DOWN, EDITOR
  ModsList campaign_list, mod_list;
  ztwod::Image lines{0}, lines2{0};
  ztwod::Text detail_label, detail, detail2, note, restart;  // DETAILS: the row's name, its author and version
  ModsTables tables;
  std::vector<std::string> enabled;  // mw3.cfg's, as the tabs change it
  int32_t focus = kCampaigns;        // the table the details are of
};

// "Mods State" (ours): the screen made on entering, let go on leaving, as
// the credits'.
class ModsState : public MenuState {
 public:
  explicit ModsState(Handle addr) : MenuState("Mods State", addr) {}
  bool enter() override;
  void leave() override;
};

}  // namespace mech3
