#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "mech3/mods.h"

// Ours (Owen, 2026-11-08; docs/modding.md, ADD MOD): a mod added from a .zip
// (or any archive the build's libarchive reads: .7z, .rar) or a folder into
// the write dir's mods\<name>\. Unpacked first into a staging folder of ours
// (mod-staging\<n>\, with our marker), every entry checked (nothing outside
// it: no absolute path, drive, "..", link), the size and the count capped,
// the mod's root found whatever the layout, a whole install cut to the files
// that differ from the player's, mod.txt written when it has none, the mod
// checked as --check-mod does, then moved into mods\ under a name no folder
// has. Nothing in mods\ is ever replaced or removed; the staging folder is
// removed only with our marker in it.
namespace mech3 {

constexpr const char* kModAddedMarker = "mw3-mod-added.txt";   // in a mod folder the game made
constexpr const char* kModStagingMarker = "mw3-mod-staging.txt";  // in a staging folder

// Our own files in a mod folder (mod.txt, the marker): not the mod's data.
bool mod_own_file(const std::string& rel);

struct ModAddLimits {
  int64_t max_bytes = int64_t{4} << 30;  // the files unpacked, together
  int64_t max_files = 20000;
};

enum class ModGame { kUnknown, kMw3, kPm };

struct ModAdd {
  std::string source;        // what the player picked
  std::string staging;       // ours: write dir/mod-staging/<n> (the marker in it)
  std::string root;          // where the mod's files start (inside staging/files)
  std::string name;          // its name: mod.txt's, else the archive's or folder's
  bool has_mod_txt = false;
  bool full_install = false; // a whole install (Mech3.exe in it), cut to what differs
  int same_dropped = 0;      // files the same as the install's, left out
  int other_dropped = 0;     // programs and the player's own kind of file, left out
  ModGame game = ModGame::kUnknown;  // from its files (mod.txt's for= when it has one)
  std::string error;         // why it was refused, "" when it was not (staging then gone)
};

// A path inside an archive made safe: '/' separated, relative, no "..",
// no drive or ':' (alternate streams), no control characters; "." and empty
// parts dropped. False for anything that would land outside the folder.
bool mod_entry_path_ok(const std::string& path, std::string* clean);
// A folder name for mods\ from a name: letters, digits, space, '-', '_', '.',
// '(' and ')' kept, others '_'; trimmed; "mod" when nothing is left.
std::string mod_folder_name(const std::string& name);
// Which game the files are for, by the names only one game ships (MW3's
// c4b and t1 worlds and its own mission readers, Pirate's Moon's readersa
// and readeria4): kUnknown when neither or both.
ModGame mod_game_of(const std::vector<std::string>& rel_paths);

// Unpacks or copies `source` into a new staging folder, finds the root and
// cuts a whole install to the files that differ from `install`'s. On a
// refusal `error` says why and the staging folder is already gone.
ModAdd mod_add_prepare(const std::string& write_dir, const std::string& source, const std::string& install,
                       const ModAddLimits& limits = {});

struct ModAddResult {
  bool ok = false;
  std::string folder;             // its folder under mods\ (what mod= names)
  ModInfo info;                   // its mod.txt as read back
  int check_errors = 0;
  std::vector<std::string> report;  // mod_check's lines
  std::string message;            // one line for the screen
};
// mod.txt written when it has none (for= from `game`), the mod checked
// (refused when the game would refuse anything in it), moved into mods\
// under its name or "<name> (2)" and on, our marker in it. The staging
// folder is removed either way.
ModAddResult mod_add_finish(const std::string& write_dir, ModAdd& add, ModGame game, const std::string& install);
// The staging folder removed (only with our marker in it).
void mod_add_discard(ModAdd& add);

}  // namespace mech3
