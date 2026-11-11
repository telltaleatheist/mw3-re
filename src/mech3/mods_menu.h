#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "mech3/menu_states.h"
#include "mech3/menu_widgets.h"
#include "mech3/mods_tables.h"
#include "mech3/mods_widgets.h"
#include "upgrades/mods/mods_tick_list.h"

// Ours (Owen, 2026-11-08; docs/modding.md, The MODS screen): the main menu's
// MODS button and its screen, MODSDIALOG (our record edits,
// patch12::apply_ours) on the pilot screen's look. Two tables: CUSTOM
// CAMPAIGNS (each campaign= of every mod) and MODS (every other mod, on or
// off, in winning order, with its conflicts). Mods by campaign, step 2 (until
// step 3's CAMPAIGNS list): a plain run mounts no mods; ON/OFF, UP and DOWN
// write the game's own campaign entry's ticks, ENTER on a custom campaign
// starts the program again into its entry and ENTER on the MODS table into
// the game's own campaign with its ticks (mods_menu_play). EDITOR is a stub for
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
std::string& mods_menu_install();                 // the install (a whole-install mod is cut against it)

// IMPORT's and EXPORT's platform side (the program sets it; unset, the tabs
// say so): the system's picker for an archive or a folder, and its save box
// for EXPORT's zip (the file name offered), each answer handed back through
// mods_menu_picked from whatever thread it comes on; the mods folder opened
// in the system's file browser; which game, when a mod's files cannot tell
// (1 MechWarrior 3, 2 Pirate's Moon, 0 neither: not added).
struct ModsMenuPlatform {
  std::function<void(bool folder)> pick;
  std::function<void(const std::string& file_name)> save;
  std::function<bool(const std::string& dir)> open_folder;
  std::function<int(const std::string& name)> ask_game;
};
ModsMenuPlatform& mods_menu_platform();
// EDIT's (editor.md, The way in from MODS): the program's start of a mod's
// mission in the editor (the mod's folder, the world, its m<n>; false with
// `why` when it cannot), and the flag that opens MODS again when the main
// menu comes back (the editor's exit sets it).
using ModsEditStart = std::function<bool(const std::string& mod, const std::string& world, int32_t number, std::string* why)>;
// ENTER's (docs/mods-campaigns-plan.md, step 2): the program's start again
// into a campaign entry (campaign_entries.h), on its pilot screen; false with
// `why` when it cannot (unset: said). On true the game is closing.
using ModsPlay = std::function<bool(const std::string& entry, std::string* why)>;
ModsPlay& mods_menu_play();
// TEST's on a plain run (step 4a): the program's start again into the entry and straight into that mission
// (--entry ID --entry-mission WORLD N). The campaign MODS opens on (--mods-entry ID: the one just left).
using ModsTestRestart = std::function<bool(const std::string& entry, const std::string& world, int32_t number, std::string* why)>;
ModsTestRestart& mods_menu_test_restart();
std::string& mods_menu_open_entry();
ModsEditStart& mods_menu_edit_start();
bool& mods_menu_reopen();
// A mod's missions for EDIT: its campaigns' lines when it has any (mounted:
// mod_campaigns), else its worlds' reader archives (zbd\<world>\readerm<N>.zbd).
struct ModsEditRow {
  std::string world;
  int32_t number = 0;
};
std::vector<ModsEditRow> mods_edit_rows(const ModInfo& m);
// TEST's (ours, docs/modding.md, Quick testing): the program's start of a
// mission to play with the mods on (EDIT's start without the editor; MODS
// again when the main menu comes back), and its missions: the mod's own
// (mods_edit_rows), then every campaign mission of the install's worlds
// (zbd\<world>\readerm<N>.zbd, the game's world order), each once.
ModsEditStart& mods_menu_test_start();
std::vector<ModsEditRow> mods_test_rows(const ModInfo& m, const std::string& install);
void mods_menu_picked(const std::string& path);  // "" cancelled; the screen takes it on its next update

class ModsScreen;

class ModsScreen : public MenuScreen {
 public:
  // Six buttons (Owen, 2026-11-11): PLAY (kEnter), MISSIONS, IMPORT (kAdd), INFO, the delete tab, BACK. kToggle
  // has no button: a click on a row's box ticks it. kExport and kOpenFolder are INFO's view's.
  enum Id : int32_t { kEnter = 1, kBack, kToggle, kUp, kDown, kMissions, kCampaigns, kMods, kAdd, kOpenFolder, kExport, kInfo, kDelete };
  ModsScreen(Engine& e, Handle state);
  void update(float dt) override;    // a picked mod taken in
  ~ModsScreen() override;
  void on_button(int32_t id, int32_t a, int32_t b) override;
  void refill();                     // both tables again from the write dir
  void add_mod(const std::string& path);  // IMPORT's pick: added (mod_install.h), shown, said
  void export_mod(const std::string& zip);  // EXPORT's answer: the chosen mod packed (upgrades/mods/mod_export.h), said
  void tell(const std::string& text);     // the two lines under MODS, the text wrapped
  void heading(const char* words);        // the left box's: CAMPAIGNS, MISSIONS, INFO
  std::string shown_heading;
  void show_details();               // the selected row's lines
  void enter_campaign();             // PLAY
  void choose_entry();               // the campaign chosen at the left: MODS filled with its ticks
  const ModsCampaignRow* chosen_row() const;
  void missions();                   // MISSIONS: the chosen campaign's missions in the left box, or back to the tables
  void edit_start();                 // PLAY in MISSIONS: the chosen mission played (the game started again into it)
  void report();                     // INFO (ours, upgrades/mods/mod_report.h): the chosen mod's or campaign's sentences, or back
  void toggle(int32_t tick_id);      // a tick flipped or moved (the box's click, UP, DOWN): the campaign's ticks saved
  void show_buttons();               // the tabs the view has, the others hidden (each update too)
  std::string hover_line(int32_t id) const;  // what a press of that button does, and to what
  void hover();                      // the line of the button under the mouse; off the buttons the last result back

  ModsSlideButton enter, back;       // ENTER, BACK
  ShellButton missions_btn;          // MISSIONS (UP and DOWN are the tick list's)
  ShellButton add_btn, export_btn, folder_btn, info_btn;  // IMPORT, INFO; in INFO's view EXPORT, OPEN FOLDER
  // The delete tab (upgrades/mods/mods_delete.h), one place, three records: the one that says what the press
  // acts on is shown, none when there is nothing of ours to remove: DELETE MOD (a mod, or an imported
  // campaign, whose mod is what goes), DELETE COPY (a copy of ours with its pilots), DELETE (what is left of a
  // campaign whose mod is gone), RESET (the game's own campaign). "DELETE CAMPAIGN" does not fit a tab.
  ShellButton delete_btn, delete_copy_btn, delete_rest_btn, reset_btn;
  void delete_row();                 // the chosen row deleted or reset, after the game's confirm
  void show_delete_tab();
  ModsList campaign_list;
  ModTickList ticks;                 // MODS: the chosen campaign's (upgrades/mods/mods_tick_list.h)
  ModsList& mod_list;                // its list
  std::vector<std::string>& enabled; // its ticks, in winning order
  ztwod::Image lines{0}, lines2{0};
  zimg::Texture* own_lines = nullptr;  // LINES2 with the CAMPAIGNS box (ours, released with the screen)
  ztwod::Text detail_label, detail, detail2, note, restart;  // DETAILS: the row's name, its author and version
  ModsTables tables;
  std::string entry;                 // the campaign chosen at the left (campaign_entries.h): MODS shows its ticks
  int32_t focus = kCampaigns;        // the table the details are of
  std::string adding;                // a pick said ("Adding ...") and added on the next update
  std::string export_folder;         // EXPORT's mod, while the save box is up
  std::string exporting;             // its zip, said ("Exporting ...") and written on the next update
  bool editing = false;              // EDIT's (or TEST's) list in CUSTOM CAMPAIGNS' box
  bool testing = false;              // the list is TEST's
  std::string edit_mod;              // its mod's folder
  std::vector<ModsEditRow> edit_rows;
  int tab_shown = 0;                 // the delete tab: 0 none, 1 DELETE MOD, 2 RESET, 3 DELETE COPY, 4 DELETE (show_delete_tab)
  bool tick_clicked = false;         // a box's click flipped a tick on this frame already (a click tells the screen twice)
  int32_t hovered = 0;               // the button the hover line is of (0 none)
  std::string hover1, hover2;        // the hover line as said, and under it what stood there before
  std::string under1, under2;
  bool delete_asked = false;         // DELETE or RESET asked on this click already (delete_row)
  bool reporting = false;            // REPORT's sentences in CUSTOM CAMPAIGNS' box
  std::string report_file;           // the report up, as saved in <write dir>/mod-reports ("" not saved)
  std::string report_mod;            // its mod's folder ("" the game's own campaign: nothing to export)
  // Inside an entry started for TEST (upgrades/mods/entry_run.h, kTest): the screen is that entry's test list
  // only ("TEST in <campaign>"): ENTER plays the chosen mission in place, BACK leaves the entry.
  bool test_mode = false;
  void open_test_mode();
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
