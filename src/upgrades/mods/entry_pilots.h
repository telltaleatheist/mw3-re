#pragma once
#include <string>
#include <vector>

// Ours (docs/mods-campaigns-plan.md, Pilots; Owen 2026-11-10): inside a
// campaign entry the pilot screen lists the entry's own pilots and, after
// them, the main menu's pilots the entry does not have (a name the entry has,
// in any letter case, is the entry's). Picking one of those brings it across
// at that moment, quietly (entry_pilot_copy.h): its file and the saved mechs,
// and its saved games only when the entry plays the game's own campaign
// (campaign_entries.h, entry_plays_own_campaign), so in a different campaign
// the pilot starts at that campaign's first mission (pilot.md: a campaign
// record without its saved game is dropped at load). The main menu's pilots
// are only ever listed by name and copied as bytes: never loaded, opened for
// writing, renamed or deleted from inside an entry.
namespace mech3 {

class Pilot;

// The run is inside an entry (entry_run.h) with a write dir to list from.
bool entry_pilots_on();
// The main menu's pilot names the entry lacks, sorted; empty outside an entry.
std::vector<std::string> entry_main_pilots();
bool entry_is_main_pilot(const std::string& name);  // one of those, in any letter case
// Brought across and loaded as the entry's own; null with `why` (nothing is left half-copied).
Pilot* entry_bring_pilot(const std::string& name, std::string* why);

}  // namespace mech3
