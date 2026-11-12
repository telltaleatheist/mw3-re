// Playtest W20 (upgrades/mods/loadout_fallback.h): a mod's mech whose primary loadout is renamed ("Primary: CN9-D",
// Freako's PM 1.2 mod) is still given at a campaign's start. The original's exact match finds no "Primary", and
// Pirate's Moon's first mission had neither the Centurion nor the Owens.
#include <string>

#include "check.h"
#include "gamez/zutil/zfix.h"
#include "mech3/mech_config.h"
#include "mech3/mech_reader.h"
#include "mech3/pilot.h"
#include "upgrades/mods/loadout_fallback.h"

namespace {
// mech `m`'s definition as a mod's: its loadouts named `names`, in the slot the game reads it from.
struct FakeDfn {
  int32_t mech;
  mech3::MechDfn* kept;
  mech3::MechDfn* dfn;
  FakeDfn(int32_t m, std::initializer_list<const char*> names) : mech(m), kept(mech3::mech_dfn_slot(m)) {
    dfn = new mech3::MechDfn(m, nullptr);
    dfn->configs = new mech3::ConfigList(m, static_cast<int32_t>(names.size()));
    for (const char* n : names) dfn->configs->configs.push_back(new mech3::Config(m, n));
    mech3::mech_dfn_slot(m) = dfn;
  }
  ~FakeDfn() {
    mech3::mech_dfn_slot(mech) = kept;
    delete dfn->configs;
    dfn->configs = nullptr;  // nothing saved on the way out
    delete dfn;
  }
};
}  // namespace

TEST(loadout_fallback_gives_a_renamed_primary) {
  {
    mech3::ConfigList l(3, 3);
    for (const char* n : {"Alternate A", "Primary: CN9-D", "Exotic"}) l.configs.push_back(new mech3::Config(3, n));
    const mech3::Config* c = mech3::loadout_fallback(l, "Primary", true, "centurion");
    EXPECT(c && std::string(c->name) == "Primary: CN9-D");  // the one whose name starts with it
    c = mech3::loadout_fallback(l, "Primary", false, "centurion");
    EXPECT(c && std::string(c->name) == "Primary: CN9-D");
    c = mech3::loadout_fallback(l, "Brawler", true, "centurion");
    EXPECT(c && std::string(c->name) == "Alternate A");  // the primary asked for: the first
    EXPECT(!mech3::loadout_fallback(l, "Brawler", false, "centurion"));  // any other name: none, as the original
  }
  // Through the campaign start's hook: the mech with a renamed primary is given; with the fixes off (the
  // original's path) it is not.
  const int32_t mech = 3;
  FakeDfn fake(mech, {"Primary: CN9-D", "Alternate A"});
  mech3::PilotHooks h;
  const std::string primary = h.message(0xf20);
  const std::string asked = primary.empty() ? "Primary" : primary;
  mech3::Store given(0);
  EXPECT(h.store_add_mech(given, mech, asked.c_str()) == 1 && given.has_mech(mech));
  const bool keep = zfix::options().fixed;
  zfix::options().fixed = false;
  mech3::Store original(0);
  EXPECT(h.store_add_mech(original, mech, asked.c_str()) == 0 && !original.has_mech(mech));
  zfix::options().fixed = keep;
}
