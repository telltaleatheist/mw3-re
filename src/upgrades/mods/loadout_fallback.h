#pragma once
// Ours (playtest W20): a mech whose loadouts do not carry the name the game asks for. The campaign's start and a
// mission script's gift ask for a mech by its loadout's name ("Primary", message 0xf20); a mod that renames the
// primary ("Primary: CN9-D", Freako's PM 1.2 mod) made the original's exact match (0x00424e90) find nothing, and the
// mech was never given: Pirate's Moon's first mission had no Centurion and no Owens, only the Puma.
#include <cstdint>

namespace mech3 {

struct Config;
struct ConfigList;

// The loadout to give for `name` when none is called that: the first whose name starts with it, else, when the
// primary was asked for (`primary`), the mech's first (the dfn lists its primary first). Said once per mech and
// name in the log; null when there is none to give.
const Config* loadout_fallback(const ConfigList& list, const char* name, bool primary, const char* mech_name);

}  // namespace mech3
