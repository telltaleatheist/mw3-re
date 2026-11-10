#pragma once
#include <string>

#include "mech3/mods.h"

// Ours (docs/modding.md, The save's mods): a saved game records the mods it
// was made with, and the load says when they differ from the ones running.
// A zSave registrant "Mods" writes one entry, "Mods/identity", only when a
// gameplay mod is on (mods_identity), so a save made without mods is the
// original's. The original's load passes over an owner it has not
// registered (0x005a8e40, save.md section 1), so it reads ours as before.
namespace mech3 {

// The registrant, added after zSave's init (GameEngine::save_init).
void save_mods_register();
// Before a saved game is read (PilotHooks::load_world): what the next load
// gives back starts as "made without mods".
void save_mods_begin_load();
// The entry's text and its reading (the tests' too).
std::string save_mods_text(const ModsIdentity& id);
ModsIdentity save_mods_parse(const std::string& text);
// After the mission load's "Saved Game Loaded": a line for the loading
// screen when the save's mods differ from the running ones, "" when they
// match or no saved game was read since the last call (it said once).
std::string save_mods_note();

}  // namespace mech3
