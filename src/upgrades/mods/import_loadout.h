#pragma once
#include <string>
#include <vector>

// Ours (docs/modding.md, IMPORT: a mech loadout): a mech variant passed
// around as the lab's exported loadout (export\<mech>_<name>.m3c, Ctrl+X) is
// not a mod. IMPORT copies such a file into the write dir's import\ folder,
// where the lab's own import (Ctrl+I, lab_import_config) reads the next one
// once, in whichever campaign's lab the player is. A file there is never
// replaced; the picked file is only read.
namespace mech3 {

// A file named *.m3c (any case).
bool loadout_name(const std::string& path);
// Whether its start reads as a loadout's (the two names' lengths and room for the rest); `why` when not.
bool loadout_reads(const std::string& path, std::string* why);

struct ImportLoadouts {
  std::vector<std::string> added;    // the names they have in the import folder
  std::vector<std::string> refused;  // "<file>: <why>"
  std::string message;               // one line for the screen
};
// Each of `files` (host paths) copied into <write_dir>/import under its own name, or "<stem> (2).m3c" and on
// while one of that name (or its .done, a loadout taken before) is there; its .rows beside it (mod_rows_file.h)
// copied with it.
ImportLoadouts import_loadouts(const std::string& write_dir, const std::vector<std::string>& files);

}  // namespace mech3
