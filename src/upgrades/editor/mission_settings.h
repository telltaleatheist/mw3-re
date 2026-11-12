#pragma once
#include <cstdint>
#include <string>

// Ours (Owen, 2026-11-11; docs/editor.md, Mission settings): the editor's
// MISSION SETTINGS row (the main menu's one blank row), a form for the
// mission that is open. One setting for now: how often a kill's salvage is
// offered in this mission (upgrades/mods/salvage_chance.h), 0 to 100, empty
// for none of its own. OK writes mod.txt's mission_salvage= line for the
// mission into the player's copy of the campaign (save_as.h: the first save
// asks the copy's name; the old mod.txt kept as mod.txt.bak), which the next
// start reads. The game's files, and a mod the editor did not make, are
// never written.
namespace mech3 {

struct EditorForm;

void mission_settings_open(int32_t y);  // the MISSION SETTINGS row
EditorForm& mission_settings_form();
const std::string& mission_settings_last();  // the line OK wrote last ("mission_salvage=c1 2 75", "removed"), "" nothing

// mod.txt's text with the mission's line put in the place of its old one (the world in any case), else after
// the last mission_salvage= line, else at the end; `chance` -1 takes the mission's line out. Every other line
// as it was, its line ends kept.
std::string mod_txt_with_mission_salvage(const std::string& text, const std::string& world, int32_t number, int32_t chance);
// The mission's own value in a mod.txt's text, -1 when it has no line that reads.
int32_t mod_txt_mission_salvage(const std::string& text, const std::string& world, int32_t number);

}  // namespace mech3
