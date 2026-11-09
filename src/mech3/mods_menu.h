#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "mech3/menu_states.h"
#include "mech3/menu_widgets.h"
#include "mech3/mods.h"

// Ours (Owen, 2026-11-08; docs/modding.md, The MODS screen): the main menu's
// MODS button and its screen, MODSDIALOG (our record edits,
// patch12::apply_ours) on the pilot screen's look. Two tables: CUSTOM
// CAMPAIGNS (each campaign= of every mod) and MODS (every other mod, on or
// off, in mw3.cfg's order, with its conflicts); ON/OFF, UP and DOWN write
// mw3.cfg's mod= lines and take hold at the next start. EDITOR is a stub for
// the Battlesport editor. Bound only where the program turns it on
// (mods_menu_page): the probes run the original's main menu.
namespace mech3 {

// ---- the tables (no screen: the tests build them) ----

struct ModsCampaignRow {
  std::string mod, file;    // the mod's folder, campaign= as given
  std::string name;         // the campaign's name=, else its file
  std::string author, version;  // the mod's
  bool mounted = false;     // its mod is on in this run (mod_campaigns has it)
  std::string error;        // why it cannot be played, "" when it can
};

struct ModsTables {
  std::vector<ModsCampaignRow> campaigns;
  std::vector<ModsListEntry> mods;  // mods_list's, without the mods that are only campaigns
};

// The tables from the write dir's mods\ and `enabled` (mw3.cfg's order);
// `mounted` the mod folders on in this run. A mod with a campaign= is a
// campaign row each and not in MODS unless it also holds other files
// (mech=, weapon=, ia_world=, or any file besides mod.txt and its campaign files).
ModsTables mods_tables(const std::string& write_dir, const std::vector<std::string>& enabled,
                       const std::vector<std::string>& mounted);

// A MODS row as shown: "[x] Name" (on) or "[ ] Name", " !" after it when it
// has a conflict or cannot be used.
std::string mods_row_text(const ModsListEntry& m);
// The line under the table for a MODS row: why it cannot be used, what it
// shares with which earlier mod (that one wins), or "" when nothing.
std::string mods_row_note(const ModsListEntry& m, const std::vector<ModsListEntry>& all);
// A campaign row's details: "<author>, version <v>" and why it cannot be played.
std::string mods_campaign_details(const ModsCampaignRow& c);

// ON/OFF: a mod off is turned on at the end of the order, one on is taken
// out. UP and DOWN: an enabled mod moved one place in the order (earlier
// wins a shared file); false when it cannot move (off, or at the end).
void mods_toggle(std::vector<std::string>& enabled, const std::string& folder);
bool mods_move(std::vector<std::string>& enabled, const std::string& folder, int32_t by);

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
