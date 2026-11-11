#pragma once
#include <functional>
#include <string>
#include <vector>

#include "mech3/mod_install.h"

// Ours (Owen, 2026-11-10; docs/modding.md, IMPORT from a modded game folder):
// a mod that ships as an installer writes over a game folder, so what the
// player has of it is a whole install. Given such a folder (the game's exe
// and zbd\ in it), the files that differ from the player's own install, or
// are new, under the folders the install has (zbd\ and the like) are copied
// into a staging folder of ours and added as a mod by mod_install's add
// (its checks, mod.txt, marker and name). Both installs are only read: never
// the exe, dlls, dgVoodoo's files, pilots, configs or saves, and neither
// folder is ever changed. Any other folder goes to mod_install's add as it is.
namespace mech3 {

// The game's exe and a zbd folder in it, in any letter case.
bool import_folder_is_install(const std::string& folder);
// A folder's data version by the 28 files the 1.2 patch touches (data_version.h):
// "1.0" or "1.2" when every one of them that is either's is that one's (a mod's
// own changed files aside), else "".
std::string import_folder_version(const std::string& folder);

struct ImportFolder {
  bool install = false;      // `folder` is a whole install (else nothing below is set)
  std::string staging;       // ours: write dir/mod-import/import-<n> (the staging marker in it)
  std::string picked;        // the folder inside it holding what was taken, named as `folder`
  ModGame game = ModGame::kUnknown;  // the modded folder's, from its files
  int taken = 0;             // differ from the player's install, or new
  int same = 0;              // the same as the player's: left
  int programs = 0;          // the program, dlls, dgVoodoo's files: left
  int own = 0;               // the player's own kind (pilots, configs, saves, settings, logs): left
  int other = 0;             // loose at the top, or in a folder the install does not have: left
  // The data's version on each side ("1.0", "1.2", "" when it cannot be told),
  // by the data files neither a mod nor the patch's other version changed; and
  // what is said when they differ (the versions' differences are taken too).
  std::string version, install_version, warning;
  std::vector<std::string> linked;  // linked folders in the modded folder (a symlink, a Windows junction): left out, said
  std::string error;         // why it was refused ("" when not; the staging then gone)
};
// What would be taken, copied into a new staging folder. The modded folder
// and `install` are read only.
ImportFolder import_folder_prepare(const std::string& write_dir, const std::string& folder, const std::string& install,
                                   const ModAddLimits& limits = {});
// The staging folder removed (only with our marker in it).
void import_folder_discard(ImportFolder& f);
// The line that says what was taken and what was left (the log's), and the
// short one for the MODS screen's two lines of 58 letters.
std::string import_folder_report(const ImportFolder& f);
std::string import_folder_report_short(const ImportFolder& f);

// IMPORT's pick whole: a whole install prepared as above and added by
// mod_add_run (the game its files say, else asked), any other pick handed to
// mod_add_run unchanged. `report`, when given, gets the short line ("" for
// any other pick); the long one goes to the log ("mw3: import: ...").
ModAddDone import_folder_run(const std::string& write_dir, const std::string& source, const std::string& install,
                             const std::function<int(const std::string& name)>& ask, std::vector<std::string>* enabled,
                             std::string* report = nullptr);

}  // namespace mech3
