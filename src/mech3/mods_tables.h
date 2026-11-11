#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "mech3/mods.h"

// Ours (docs/modding.md, The MODS screen): the MODS screen's two tables and
// its changes to the order, apart from the screen (mods_menu.h) for the tests.
namespace mech3 {

struct ModsCampaignRow {
  std::string mod, file;    // the mod's folder, campaign= as given
  std::string name;         // the campaign's name=, else its file
  std::string author, version;  // the mod's
  bool mounted = false;     // its mod is on in this run (mod_campaigns has it)
  std::string error;        // why it cannot be played, "" when it can
  // Mods by campaign (mods_entry_rows): the entry it is (campaign_entries.h),
  // and for a copy what it was saved from ("A copy of <campaign>").
  std::string entry, copy_of;
};

struct ModsTables {
  std::vector<ModsCampaignRow> campaigns;  // CAMPAIGNS' rows (mods_entry_rows); MODS' are the tick list's
};

// Mods by campaign (docs/mods-campaigns-plan.md): CAMPAIGNS' rows, every
// campaign entry but Instant Action's: the game's own first, each imported
// campaign, each copy, then those whose mod is gone. `error` says why one
// cannot be played now: its campaign file, its mod gone, a ticked or
// built-on mod missing (named).
std::vector<ModsCampaignRow> mods_entry_rows(const std::string& write_dir);

// A MODS row as shown: "[x] Name" (on) or "[ ] Name", " !" after it when it
// has a conflict or cannot be used.
std::string mods_row_text(const ModsListEntry& m);
// The line under the table for a MODS row: why it cannot be used, what it
// shares with which earlier mod (that one wins), or "" when nothing.
std::string mods_row_note(const ModsListEntry& m, const std::vector<ModsListEntry>& all);
std::string mods_row_note_shared(const ModsListEntry& m, const std::vector<ModsListEntry>& all);  // its part after the needs
// A CUSTOM CAMPAIGNS row as shown: its name, " !" after it when its mod is
// off in this run or it cannot be played (the note line says which).
std::string mods_campaign_row_text(const ModsCampaignRow& c);
// A campaign row's details: "<author>, version <v>" and why it cannot be played.
std::string mods_campaign_details(const ModsCampaignRow& c);

// ON/OFF: a mod off is turned on at the end of the order, one on is taken
// out. UP and DOWN: an enabled mod moved one place in the order (earlier
// wins a shared file); false when it cannot move (off, or at the end).
void mods_toggle(std::vector<std::string>& enabled, const std::string& folder);
bool mods_move(std::vector<std::string>& enabled, const std::string& folder, int32_t by);

}  // namespace mech3
