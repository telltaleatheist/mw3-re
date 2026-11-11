#pragma once
#include <string>
#include <vector>

// Ours (docs/mods-campaigns-plan.md, Instant Action): the rules of Instant
// Action's MODS apart from its screen (ia_mods_menu.h), for the tests.
namespace mech3 {

constexpr const char* kIaEntry = "ia";
// The line about the mods: `ticked` of them as the tick list shows them now,
// or (-1) what this run has mounted: "MODS: off", "MODS: 2 on"; on a plain
// run whose ticks could not be played, "MODS: 1 missing".
std::string ia_mods_line(const std::string& write_dir, int ticked = -1);
// The entry's ticked mods that are gone or unusable (campaign_entries.h, entry_missing_mods).
std::vector<std::string> ia_mods_missing(const std::string& write_dir);
// INSTANT ACTION on the main menu of a plain run goes straight into the
// modded one: mods are ticked and none is missing.
bool ia_straight_in(const std::string& write_dir);
// The pilot a start was given (--entry-pilot): the main menu's state picks it
// once its pilots are loaded and opens Instant Action past the pilot screen
// (the app's "back from a game" flags, 0x0048a750); taken once.
// A pilot the entry lacks is brought from the main menu first, quietly (entry_pilots.h).
std::string& ia_start_pilot();
// What ACCEPT does with the ticks as they were and as they are now.
enum class IaModsStep { kBack, kIntoEntry, kPlain };
IaModsStep ia_mods_step(const std::vector<std::string>& before, const std::vector<std::string>& now, bool inside);

}  // namespace mech3
