#pragma once
#include <cstdint>

// Ours (docs/modding.md, Quick testing): what TEST reads again at its start,
// so a modder edits, QUITs to MODS and TESTs again without a restart. The
// weapons need nothing: weapons.zrd is read at every mission load (zwep's
// init). The mechs' definitions are cached for the whole run (0x004846a0,
// read once per mech), so they are let go here (their saved loadouts
// written first, as at the game's exit) and read on first use. Only between
// missions: nothing holds a definition across one (docs/modding.md).
namespace mech3 {

void mod_test_reload();
int32_t& mod_test_reloads();  // how many times (the tests')

}  // namespace mech3
