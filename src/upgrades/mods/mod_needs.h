#pragma once
#include <string>
#include <vector>

#include "mech3/mods.h"

// Ours (docs/modding.md, Requirements in mod.txt; modder step 7): what a mod
// says it needs (needs=, conflicts=, game_version=) against the other mods
// and this program, as warnings. The MODS screens say them when a mod is
// turned on and on its row; they never stop a mod being turned on.
namespace mech3 {

// This program's version (MW3_VERSION), set by the program at its start;
// "" leaves game_version= unchecked (the tests, the probes).
std::string& mods_program_version();

// The warnings for `m` were it on with `enabled` (folder names, in order;
// `m` among them or not): each needs= mod missing, off or older than asked,
// each conflicting mod on (its own conflicts= or theirs naming it), and a
// game_version= newer than this program. `all` is every mod found.
std::vector<std::string> mod_needs_warnings(const ModInfo& m, const std::vector<ModInfo>& all,
                                            const std::vector<std::string>& enabled);

}  // namespace mech3
