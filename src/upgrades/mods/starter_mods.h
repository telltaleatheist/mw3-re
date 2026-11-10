#pragma once
#include <string>
#include <vector>

// Ours (docs/modding.md, Starter mods; modder step 5): one small example mod
// of each kind, made from the player's own install so nothing of the game's
// is in the repo or the download: the recipe is here, the game's file read
// from the install and the edit applied, written as text the readers take.
// Each has a guide, docs/starter-mods/<kind>.md, and an about_this_mod.txt in it.
//   mech         Thor B: a Thor that turns and speeds up faster (dfn_thor_b.zrd, a mech= row)
//   weapon       ER PPC B: an ER PPC that hits harder and runs hotter (weapons.zrd, a weapon= row)
//   translation  the main menu's buttons in German (strings.txt)
//   mission      a one-mission campaign on the game's own c1 mission 1 (campaign=)
// Only ever a new folder: a target that exists (anything there) is refused
// and named, nothing in it touched; no replace, no delete.
namespace mech3 {

const std::vector<std::string>& starter_kinds();
std::string starter_folder(const std::string& kind);  // "starter_<kind>"

struct StarterMade {
  bool ok = false;
  std::string path;     // the mod's folder, made or refused
  std::string message;  // what was made, or why not
};
// `kind` into `mods_dir`\starter_<kind>, from `install`'s files, for the
// running game (mods_game: "mw3" or "mw3pm").
StarterMade starter_make(const std::string& kind, const std::string& install, const std::string& mods_dir);

}  // namespace mech3
