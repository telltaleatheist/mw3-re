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
