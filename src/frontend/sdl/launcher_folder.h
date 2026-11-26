#pragma once
// The launcher's own folders (launcher split b6, 2026-11-26; docs/launcher-split.md): its write dir at the start and
// MY DATA FOLDER, on the setup kit's rekit/data_folder directly, as data_folder_start.h and mech3/data_folder do for
// the games. Pirate's Moon's old nested folder (mech3::data_folder::adopt_nested) is mw3pm's own: it adopts it at its
// every start, its status included, before the launcher reads where its data is.
#include <string>

#include "rekit/data_folder.h"

namespace sdlfe::own {

// MechWarrior 3's data folder as the launcher tells the kit (its settings file mw3.cfg; the launcher's own
// narrow_paths_utf8 and path_from_config): the same record mech3::data_folder gives it, so a move reads the same.
const rekit::data_folder::Game& data_game();

// The usual place, BattleTech/<game>; MW3_USUAL_FOLDER names another (checks). An earlier MicroProse/<game> is moved
// across once in a package (rekit::data_folder::family_move), never in a development build.
std::string usual_folder(const char* game = "MechWarrior 3");

// `dir` set to the folder to use, through the pointer. One that cannot be used is said and asked about again (a
// drive to connect), never replaced by a fresh folder: false when the player quits, or at once when `ask` is false.
bool data_folder_at_start(std::string* dir, bool ask, bool* pointer = nullptr);

}  // namespace sdlfe::own
