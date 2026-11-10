#pragma once
#include <string>
#include <vector>

// Ours (docs/mods-campaigns-plan.md, Deleting and resetting; Owen
// 2026-11-10): DELETE and RESET on the MODS screen. One remover takes a
// folder directly under this write dir's mods\ or campaigns\ and the marker
// it must carry; without the marker, through a link, or anywhere else, it
// refuses. What goes is decided by the row chosen and said in the confirm,
// its counts read from the folder at that moment:
// - a copy of ours (the editor's marker): its mod folder and its campaign
//   folder (pilots, saved games, ticks);
// - a mod the game imported (mod_install's marker): its mod folder only. An
//   asset mod's ticks are left (campaigns that had it show "!"); an imported
//   campaign's pilots and saved games are kept;
// - what is left of a campaign whose mod is gone: its campaign folder;
// - the game's own campaign: RESET, its ticks, pilots, saved games and saved
//   mechs cleared, the folder made again as on a first start.
// Never: the game's own files, a mod folder without a marker of ours, a
// campaign another is built on, the campaign the run is inside.
namespace mech3 {

// The one remover. `under` is "mods" or "campaigns"; `name` one plain folder
// name; `marker` the file the folder must hold, itself a plain file. Links
// inside are removed as links, never followed. False with `why`, nothing
// removed, when: the name is not plain, the folder is missing or is a link,
// the marker is missing, or (campaigns) it holds a folder the game does not
// make there (entry_ensure's own refusal).
bool remove_marked_folder(const std::string& write_dir, const char* under, const std::string& name, const char* marker, std::string* why);

struct DeletePlan {
  enum Kind { kNone, kCopy, kImportedMod, kImportedCampaign, kLeftover, kReset } kind = kNone;
  std::string label;     // the tab's: "DELETE", "RESET"; "" when there is nothing to offer
  std::string question;  // the confirm's words
  std::string refused;   // why it is not offered or cannot be done, for the note
  std::string mod;       // the mod folder that goes ("" none)
  const char* mod_marker = nullptr;
  std::string entry;     // the campaign folder that goes or is reset ("" none)
};
// For the campaign chosen at the left, and for the mod chosen at the right.
// `inside` is the entry the run is in ("" a plain run).
DeletePlan delete_plan_for_entry(const std::string& write_dir, const std::string& entry, const std::string& inside = "");
DeletePlan delete_plan_for_mod(const std::string& write_dir, const std::string& folder);
// Done as planned (the confirm was answered yes): false with `why`; what was removed before a failure stays removed and is said.
bool delete_run(const std::string& write_dir, const DeletePlan& plan, std::string* why);

// The counts the confirms say: an entry's pilots (pilots\*.mw3) and saved games (pilots\games\*.mw3).
void entry_counts(const std::string& write_dir, const std::string& entry, int* pilots, int* saves);

}  // namespace mech3
