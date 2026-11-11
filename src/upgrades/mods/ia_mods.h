#pragma once
#include <string>
#include <vector>

// Ours (docs/mods-campaigns-plan.md, Instant Action): the rules of Instant
// Action's MODS apart from its screen (ia_mods_menu.h), for the tests.
namespace mech3 {

constexpr const char* kIaEntry = "ia";
// The line about the mods: `ticked` of them as the tick list shows them now,
// or (-1) what this run has mounted: "MODS: off" on a plain run (whatever
// ticks are remembered), "MODS: 2 on" inside the entry.
std::string ia_mods_line(const std::string& write_dir, int ticked = -1);
// The entry's ticked mods that are gone or unusable (campaign_entries.h, entry_missing_mods).
std::vector<std::string> ia_mods_missing(const std::string& write_dir);
// A plain run never goes into the modded Instant Action by itself (Owen, 2026-11-11: "mods should be disabled
// when you open the game, and you should enable them if you want to play modded"): the ticks of the last
// session are remembered as ticks on its MODS screen, and ACCEPT there starts the game again with them.
// The pilot a start was given (--entry-pilot): the main menu's state picks it
// once its pilots are loaded and opens Instant Action past the pilot screen
// (the app's "back from a game" flags, 0x0048a750); taken once.
// A pilot the entry lacks is brought from the main menu first, quietly (entry_pilots.h).
std::string& ia_start_pilot();
// What ACCEPT does with the ticks as they were and as they are now: none ticked, back (plain) or the plain
// game again (inside the entry); some ticked, into the entry, unless inside it with nothing changed.
enum class IaModsStep { kBack, kIntoEntry, kPlain };
IaModsStep ia_mods_step(const std::vector<std::string>& before, const std::vector<std::string>& now, bool inside);

}  // namespace mech3
