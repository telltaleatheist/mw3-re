#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "mech3/mods.h"
#include "upgrades/mods/campaign_entries.h"

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

// Mods by campaign (docs/mods-campaigns-plan.md, A saved game belongs to its
// entry): a game saved inside a MODS campaign entry records the entry, the
// entries it is built on and its ticks, as a second entry "Mods/entry", in
// place of the mod list; the load's note then says when the game was saved
// in another campaign, or when this campaign's ticks or its own files
// changed since. An entry that mounts nothing of gameplay (the game's own
// with no gameplay tick) writes nothing: the original's save.
struct SaveEntry {
  std::string id, name;            // the entry ("" none: a plain start) and its name as shown
  std::vector<std::string> bases;  // the entries it is built on, nearest first
  uint64_t ticks_hash = 0;         // mods_gameplay_hash of its ticks, in their order (0: none that change gameplay)
  std::vector<std::string> ticks;  // those ticks' names as shown (gameplay ones only)
  uint64_t mounted_hash = 0;       // the same of everything it mounts: its own mod, its bases', what they need, the ticks
};
// The running entry. The program sets it once the entry's mods are mounted
// (the one call: save_entry_set(write_dir, entry)); a plain start leaves it empty.
SaveEntry& save_entry();
void save_entry_set(const std::string& write_dir, const CampaignEntry& e);
void save_entry_clear();
// The entry's text in the save and its reading (the tests' too); a line it does not know is passed over.
std::string save_entry_text(const SaveEntry& e);
SaveEntry save_entry_parse(const std::string& text);

}  // namespace mech3
