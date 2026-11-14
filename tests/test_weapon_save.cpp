// zWeapon's save section (0x00583750 registering 0x00583830 / 0x00583850; Pirate's Moon's 0x005a5170 with
// 0x005a5260 / 0x005a5290): "Weapons" with its one entry "WeaponData", the ownerless hits' count, registered only
// with the save flag (0x00583740) on. The shipped game turns the flag off before the start (0x004802b1).
#include <cstring>
#include <filesystem>
#include <string>

#include "check.h"
#include "gamez/zsave/zsave.h"
#include "gamez/zweapon/zweapon.h"

namespace {
bool registered() {
  zsave::Manager* m = zsave::manager();
  if (!m) return false;
  for (const zsave::Registrant& r : m->list)
    if (r.name && std::strcmp(r.name, "Weapons") == 0) return r.priority == 1000;
  return false;
}
void unregister() {
  if (zsave::Manager* m = zsave::manager())
    m->list.remove_if([](const zsave::Registrant& r) { return r.name && std::strcmp(r.name, "Weapons") == 0; });
}
}  // namespace

TEST(weapons_save_section_only_with_the_flag) {
  const bool had = zsave::manager() != nullptr;
  if (!had) zsave::init();
  unregister();
  zwep::HitState& h = zwep::hit_state();
  const zwep::HitState kept = h;
  zwep::set_save(0);
  zwep::startup();
  EXPECT(!registered());  // as the game: off before the start
  zwep::set_save(1);
  zwep::startup();
  EXPECT(registered());
  EXPECT(h.hits == 0);  // the start clears it
  h.hits = 7;
  const std::string dir = std::string(MW3_TEST_TMP_ROOT) + "/weapon_save";
  std::filesystem::create_directories(dir);
  const std::string path = dir + "/w.sav";
  EXPECT(zsave::save(path.c_str(), 2));
  h.hits = 0;
  EXPECT(zsave::load(path.c_str(), 2));
  EXPECT(h.hits == 7);
  unregister();
  zwep::set_save(0);
  h = kept;
  if (!had) zsave::shutdown();
}
