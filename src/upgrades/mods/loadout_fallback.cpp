#include "upgrades/mods/loadout_fallback.h"

#include <cstdio>
#include <cstring>
#include <set>
#include <string>

#include "mech3/mech_config.h"
#include "upgrades/mods/mod_warning.h"

namespace mech3 {

const Config* loadout_fallback(const ConfigList& list, const char* name, bool primary, const char* mech_name) {
  if (!name) return nullptr;
  const Config* first = nullptr;
  const Config* prefixed = nullptr;
  const size_t n = std::strlen(name);
  for (const Config* c : list.configs) {
    if (!c || !c->name) continue;
    if (!first) first = c;
    if (!prefixed && n && std::strncmp(c->name, name, n) == 0) prefixed = c;
  }
  const Config* got = prefixed ? prefixed : primary ? first : nullptr;
  static std::set<std::string> said;
  const std::string mech = mech_name ? mech_name : "?";
  if (got && said.insert(mech + "\n" + name).second) {
    ModWarning w;
    w.wrong = std::string("no loadout is called \"") + name + "\", the name the game gives this mech by";
    w.did = std::string("gave its loadout \"") + got->name + "\" instead";
    w.original = "does not give the mech at all (a campaign's start or a mission's gift leaves it out)";
    w.fix = std::string("name its primary loadout MSG_CONFIG_PRIMARY, as the game's own mechs do");
    std::printf("mw3: %s\n", mod_warning_line("the mech " + mech + "'s loadouts", w).c_str());
  }
  return got;
}

}  // namespace mech3
