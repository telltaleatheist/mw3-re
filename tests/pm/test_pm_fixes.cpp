// Pirate's Moon's Small fixes list against its gates, and the two mech-file
// typos it shares with MechWarrior 3 (main's: tests/test_fixes.cpp).
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zgame/zgame_table.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zfix.h"
#include "mech3/mech_config.h"
#include "mech3/mech_names.h"
#include "mech3/mech_reader.h"
#include "pm/pm_game.h"
#include "upgrades/settings/upgrades.h"

// The page's list shows exactly what the switch turns off (the lead's rule,
// 2026-11-10), on Pirate's Moon by its own table: each line has its gates, by
// file, and every `options().small_fixes` in src is some line's or is one of
// the gates below that only MechWarrior 3's data meets (its c2 mission 3, its
// c4 and c4b records), which the page does not list here.
TEST(pm_small_fixes_list_matches_its_gates) {
  struct Gate { const char* file; int count; };
  struct Line { const char* text; std::vector<Gate> gates; };
  const std::vector<Line> table = {
      {"A turret's charge or glow goes out when it dies.", {{"mech3/turret.cpp", 1}}},
      {"Tripod turrets aim when facing straight up or down.", {{"mech3/turret.cpp", 1}}},
      {"A vehicle file with pitch and no roll keeps its pitch.", {{"mech3/veh_controls.cpp", 1}}},
      {"TAG's strike lands where the target is, not was.", {{"mech3/weapon.cpp", 1}}},
      {"A held beam stays on a moving mech after release.", {{"mech3/weapon.cpp", 1}}},
      {"A mech re-placed where it stands keeps to its ground.", {{"mech3/mech_place.cpp", 1}}},
      // Pirate's Moon's own copy of the file and the one it shares with MechWarrior 3's build.
      {"A mission's last animation can be waited for.", {{"pm/gamez/zeffect/zeff_anim_init.cpp", 1}, {"gamez/zeffect/zeff_anim_init.cpp", 1}}},
      {"An animation whose object is missing waits its turn.", {{"pm/gamez/zeffect/zeff_anim_init.cpp", 1}, {"gamez/zeffect/zeff_anim_init.cpp", 1}}},
      {"The Sunder Alternate B loads all 360 LRM 20 missiles.", {{"mech3/mech_reader.cpp", 1}}},
      {"The Strider has its recoil slide (a number typed as text).", {{"mech3/mech_reader.cpp", 1}}},
  };
  const std::vector<Gate> mw3_data_only = {{"mech3/mech_objective.cpp", 1}, {"mech3/zrdraiv.cpp", 2}};
  const std::vector<const char*>& list = upgrades::small_fixes_list();
  EXPECT(list.size() == table.size());
  for (size_t i = 0; i < list.size() && i < table.size(); ++i) {
    if (std::strcmp(list[i], table[i].text) != 0) std::printf("  line %zu: \"%s\" has no entry in the table\n", i, list[i]);
    EXPECT(std::strcmp(list[i], table[i].text) == 0);
    EXPECT(!table[i].gates.empty());
  }
  std::map<std::string, int> want, have;
  for (const Line& l : table)
    for (const Gate& g : l.gates) want[g.file] += g.count;
  for (const Gate& g : mw3_data_only) want[g.file] += g.count;
  namespace fs = std::filesystem;
  const fs::path src = fs::path(MW3_SOURCE_DIR) / "src";
  std::error_code ec;
  for (fs::recursive_directory_iterator it(src, ec), end; !ec && it != end; it.increment(ec)) {
    const std::string ext = it->path().extension().string();
    if (!it->is_regular_file(ec) || (ext != ".cpp" && ext != ".h")) continue;
    const std::string rel = it->path().lexically_relative(src).generic_string();
    if (rel.rfind("upgrades/settings/", 0) == 0 || rel == "gamez/zutil/zfix.h") continue;  // the switch itself
    std::ifstream f(it->path());
    int n = 0;
    for (std::string line; std::getline(f, line);)
      for (size_t at = 0; (at = line.find("options().small_fixes", at)) != std::string::npos; ++at) ++n;
    if (n) have[rel] = n;
  }
  EXPECT(!have.empty());
  for (const auto& [file, n] : have)
    if (!want.count(file) || want[file] != n) std::printf("  %s: %d gates in the code, %d in the table\n", file.c_str(), n, want.count(file) ? want[file] : 0);
  for (const auto& [file, n] : want)
    if (!have.count(file)) std::printf("  %s: %d gates in the table, none in the code\n", file.c_str(), n);
  EXPECT(have == want);
}

// Pirate's Moon ships the same two typos in dfn_sunder.zrd and dfn_strider.zrd
// as MechWarrior 3 (fixes.md): with Small fixes each reads as meant, without
// as the exe reads it.
TEST(pm_mech_data_typos_read_as_meant) {
  const std::string path = std::string(MW3PM_GAME_DIR) + "/zbd/reader.zbd";
  if (!std::filesystem::exists(path)) {
    std::printf("  (skipped: no %s)\n", path.c_str());
    return;
  }
  const zfix::Options saved = zfix::options();
  mech3::set_load_saved(nullptr), mech3::set_save_all(nullptr);  // no player's loadouts read or written
  zgame::set_game(pm::table());  // PM's archives (version 2)
  zrdr::init();
  EXPECT(zrdr::open_archive(path.c_str(), true));
  const int32_t lrm20 = mech3::weapon_index("LRM20");
  for (const bool on : {true, false}) {
    zfix::set_all(true);
    zfix::options().small_fixes = on;
    int missiles = 0;
    {
      mech3::MechDfn d(mech3::mech_index("sunder"), zrdr::load("dfn_sunder.zrd"));
      EXPECT(mech3::read_dfn(d) && d.configs);
      for (mech3::Config* c : d.configs ? d.configs->configs : std::vector<mech3::Config*>{})
        if (c->name && std::strstr(c->name, "ALTERNATE_B"))
          for (const mech3::ConfigItem* it : c->items)
            if (it->kind() == 2 && it->id() == lrm20) missiles += it->count;
    }
    float slide = -1;
    {
      mech3::MechDfn d(mech3::mech_index("strider"), zrdr::load("dfn_strider.zrd"));
      EXPECT(mech3::read_dfn(d));
      slide = d.get<float>(0x184);
    }
    std::printf("  small fixes %d: Sunder Alternate B's LRM 20 missiles %d; the Strider's recoil slide %g\n", on, missiles, slide);
    EXPECT(missiles == (on ? 360 : 120));
    EXPECT(slide == (on ? 1.5f : 0.0f));
  }
  zrdr::close_archives(true);
  zgame::set_game(zgame::mw3_table());
  mech3::set_load_saved([](mech3::ConfigList* c) { c->load_saved(); });
  mech3::set_save_all([](mech3::ConfigList* c) { c->save_all(); });
  zfix::options() = saved;
}
