#pragma once
#include <string>
#include <vector>

#include "mech3/mods.h"

// Ours (Owen, 2026-11-09; docs/modding.md, IMPORT and EXPORT): EXPORT packs
// a mod folder into one .zip that IMPORT (mod_install.h, mod_add_prepare)
// takes back as the same mod: its files and mod.txt under one folder named
// as the mod, and a checksums file (sha256sum's lines, one per file) that
// IMPORT checks. Left out: the game's own markers, the player's own kind of
// file (zutl::mod_may_serve), *.bak, *.saving and the systems' folder notes
// (.DS_Store, Thumbs.db, desktop.ini). Refused whole, as IMPORT would refuse
// the zip: a link, a name Windows cannot hold, two names differing only in
// case, more than ModAddLimits allows.
namespace mech3 {

constexpr const char* kModChecksums = "mw3-checksums.txt";  // at the mod's root, in the zip

struct ModExportList {
  std::vector<std::string> files;     // '/'-separated, from the mod's root, sorted
  std::vector<std::string> left_out;  // the same, of the files not taken
  std::string error;                  // why it cannot be exported, "" when it can
};
// What an export of `mod_dir` takes.
ModExportList mod_export_list(const std::string& mod_dir);

// "<name> <version>.zip" (the folder's name without a name), made a name
// every system can hold.
std::string mod_export_file_name(const ModInfo& info);

struct ModExportResult {
  bool ok = false;
  int files = 0, left_out = 0;
  std::string message;  // one line for the screen
};
// `mod_dir` into `zip_path`, written beside it as <zip_path>.saving and
// renamed over it when whole (the system's save box asked about replacing).
// Never into the mod's own folder.
ModExportResult mod_export(const std::string& mod_dir, const std::string& zip_path);

// IMPORT's check of a mod's checksums file at `root`, when it has one: every
// file listed there with its sum, nothing else beside it (our files apart).
// "" when it matches (the file is then removed: the mod folder keeps no list
// that goes stale) or there is none (`had` false); else why not.
std::string mod_checksums_verify(const std::string& root, bool* had);

}  // namespace mech3
