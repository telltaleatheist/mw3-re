#pragma once
#include <string>
#include <vector>

#include "mech3/mods.h"

// Ours (docs/mods-campaigns-plan.md, Owen 2026-11-10): mods by campaign. The
// MODS screen's CAMPAIGNS are entries: the game's own campaign(s), Instant
// Action, each campaign a mod brings and each copy the player saved. Each
// entry has a folder of ours, <write dir>/campaigns/<id>/ (our marker in it),
// holding its ticked asset mods (mods.txt, mod= lines in the order they win),
// its pilots and their saves (pilots/, the original's layout) and its saved
// loadouts (configs/, the mech lab's and Instant Action's). Mods mount only for an entry (--entry);
// nothing here removes a folder but campaign_entry_remove, on the player's
// confirm, and only one with our marker.
namespace mech3 {

constexpr const char* kEntriesFolder = "campaigns";        // under the write dir
constexpr const char* kEntryTicksFile = "mods.txt";        // in an entry's folder
constexpr const char* kEntriesMovedKey = "mods_moved";     // mw3.cfg's, once the migration ran

struct CampaignEntry {
  enum Kind { kOfficial, kInstantAction, kImported, kCopy } kind = kOfficial;
  std::string id;        // its folder under campaigns\: "campaign", "pirate", "ia", "<mod>" or "<mod>~<campaign id>"
  std::string name;      // shown
  std::string mod;       // the mod it is (imported, copy); "" for the game's own and Instant Action
  std::string campaign;  // the mod's campaign= id; "" when the mod plays the game's own campaign over its files
  std::string base;      // the entry it is built on: a copy's base=, a mod over the game's own files "campaign"
  std::string error;     // why it cannot be played (its campaign file does not read), "" when it can
  bool missing = false;  // its folder is there, its mod is not: listed so its saves stay in reach
};

// The game's own entries, in the list's order. Pirate's Moon's: "campaign",
// "pirate" (the Pirate Missions) and "ia" (main's: "mw3" and "ia").
std::vector<CampaignEntry> official_entries();

// A mod's kind: kind=campaign or kind=assets when mod.txt says; else a
// campaign when it has campaign= or world= lines or files of the game's own
// worlds (zbd\c1\.. zbd\t1\), an asset mod otherwise.
bool mod_is_campaign(const ModInfo& m);

// Every entry: the game's own, then the mods' (by folder; a campaign mod's
// each campaign=, or the mod itself over the game's campaign), then the
// folders under campaigns\ with our marker whose mod is gone (`missing`).
std::vector<CampaignEntry> campaign_entries(const std::string& write_dir);
const CampaignEntry* find_entry(const std::vector<CampaignEntry>& all, const std::string& id);

// <write dir>/campaigns/<id>, and the game's pilots folder in it (the
// original's separators, as set_pilots_folder takes it).
std::string entry_dir(const std::string& write_dir, const std::string& id);
std::string entry_pilots_folder(const std::string& id);
// Its saved loadouts' folder the same way (set_configs_folder): its own, empty at first.
std::string entry_configs_folder(const std::string& id);
// Its folder made with our marker, pilots\games and configs (one already there kept
// when it has the marker or is empty; one holding other files refused, `why`).
bool entry_ensure(const std::string& write_dir, const CampaignEntry& e, std::string* why);

// Its ticked asset mods (mods.txt), in the order they win; written beside it
// and renamed over it (false: the old file left as it was).
std::vector<std::string> entry_ticks(const std::string& write_dir, const std::string& id);
bool entry_save_ticks(const std::string& write_dir, const CampaignEntry& e, const std::vector<std::string>& ticks);

// What the entry mounts, in the order it wins: its own mod and its bases'
// (a copy over what it was saved from), the mods those need (needs=, found
// by folder or name), then its ticks. `locked` (when given): the ones the
// player cannot untick, the first part.
std::vector<std::string> entry_mount_list(const std::string& write_dir, const CampaignEntry& e,
                                          std::vector<std::string>* locked = nullptr);

// The entry's mods that are not in the mods folder, or cannot be used: its
// ticks, and the mods it is built on. An entry with any does not start until
// they are unticked or imported again (its pilots are kept).
std::vector<std::string> entry_missing_mods(const std::string& write_dir, const CampaignEntry& e);

// The first start of this build (once: mw3.cfg's mods_moved=1 after it):
// mw3.cfg's mod= lines become the ticks of the game's own campaigns (both; its asset mods)
// and each enabled campaign mod's entries get the same; the pilots of
// custom campaigns under pilots\mods\<mod>\<id>\ (our marker in them) move
// to their entry. mw3.cfg keeps every line byte for byte. The log's lines.
std::vector<std::string> entries_migrate(const std::string& write_dir);

}  // namespace mech3
