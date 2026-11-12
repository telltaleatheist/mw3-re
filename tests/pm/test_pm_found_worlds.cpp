// A world Pirate's Moon does not have, found at IMPORT (docs/modding.md, A world the game does not have; on
// Pirate's Moon): PM's c1 copied as ironcoast with c1.gs from PM's interp.zbd, its DB the new name, in the
// test's own folder. PM's folder table (0x00486df0) holds MW3's nine names, so the same names are the game's;
// its worlds are its own version (41), which the mod check reads with PM's reader, so the world's gamez.zbd is
// kept (MW3's header read refused it, and IMPORT left it out). A MechWarrior 3 world (version 27) is not one PM
// reads: left out, said.
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zgame/zgame_table.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/mod_install.h"
#include "mech3/mods.h"
#include "pm/pm_game.h"
#include "upgrades/mods/found_worlds.h"
#include "upgrades/mods/import_folder.h"

namespace {
namespace fs = std::filesystem;

std::string text(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
std::string fresh(const std::string& name) {
  const std::string d = std::string(MW3_TEST_TMP) + "/" + name;
  std::error_code ec;
  fs::remove_all(d, ec);
  fs::create_directories(d, ec);
  return d;
}
// <src> laid out as a zStudio export lays a world: zbd/ironcoast/ from `world_dir`, ironcoast.gs from PM's c1.gs.
bool make_export(const std::string& world_dir, const std::string& src) {
  std::error_code ec;
  fs::create_directories(src + "/zbd/ironcoast", ec);
  for (const auto& f : fs::directory_iterator(world_dir, ec)) fs::copy_file(f.path(), src + "/zbd/ironcoast/" + f.path().filename().string(), ec);
  std::string script;
  for (std::vector<std::string> line : mech3::interp_archive_script(std::string(MW3PM_GAME_DIR) + "/zbd/interp.zbd", "c1.gs")) {
    if (line.size() == 3 && line[0] == "set" && line[1] == "DB" && line[2] == "c1") line[2] = "ironcoast";
    for (size_t i = 0; i < line.size(); ++i) script += (i ? " " : "") + line[i];
    script += "\r\n";
  }
  std::ofstream(src + "/ironcoast.gs", std::ios::binary) << script;
  return script.find("set DB ironcoast\r\n") != std::string::npos;
}
}  // namespace

TEST(pm_found_worlds_import_keeps_a_pm_world) {
  const std::string install = MW3PM_GAME_DIR;
  std::error_code ec;
  if (!fs::exists(install + "/zbd/c1/gamez.zbd", ec)) return;
  zgame::set_game(pm::table());  // PM's archives and worlds, as mw3pm at its start
  const zutl::Mounts keep = zutl::mounts();
  zutl::mounts() = zutl::Mounts{};
  zutl::mounts().confined = true;
  EXPECT(mech3::mods_game() == "mw3pm");
  // PM's folder names are MW3's nine: each the game's, in any case; none of them a world found.
  for (const char* n : {"c1", "c2", "c3", "c4", "C4B", "t1", "lab", "dev1", "dev2"}) EXPECT(mech3::game_world_name(n));
  EXPECT(!mech3::game_world_name("ironcoast"));
  const std::string dir = fresh("pm_found_worlds"), wd = dir + "/wd", src = dir + "/export/Iron Coast";
  fs::create_directories(wd, ec);
  EXPECT(make_export(install + "/zbd/c1", src));
  const mech3::ModAddDone d = mech3::import_folder_run(wd, src, install, nullptr, nullptr);
  const std::string mod = wd + "/mods/Iron Coast", txt = text(mod + "/mod.txt");
  std::printf("  %s (%d warnings, %d left out)\n", d.message.c_str(), d.warnings, d.left_out);
  EXPECT(d.ok && d.warnings == 0 && d.left_out == 0);
  EXPECT(fs::is_regular_file(mod + "/zbd/ironcoast/gamez.zbd") && fs::is_regular_file(mod + "/zbd/ironcoast/readersa1.zbd"));
  EXPECT(txt.find("for=mw3pm\n") != std::string::npos && txt.find("\nworld=ironcoast\n") != std::string::npos &&
         txt.find("\nia_world=ironcoast ironcoast\n") != std::string::npos && txt.find("\ncampaign=ironcoast.txt\n") != std::string::npos);
  EXPECT(text(mod + "/ironcoast.txt").find("mission=ironcoast 1\nmission=ironcoast 2\nmission=ironcoast 5\n") != std::string::npos);
  // INFO's sentences: PM's c1 has three missions, four Instant Action maps and two multiplayer maps.
  const std::vector<mech3::FoundWorld> found = mech3::found_worlds(mod);
  EXPECT(found.size() == 1);
  if (found.size() == 1) {
    const std::vector<std::string> said = mech3::found_world_sentences(found[0]);
    for (const std::string& s : said) std::printf("  INFO: %s\n", s.c_str());
    EXPECT(!said.empty() && said[0] == "The world ironcoast has 3 missions, 4 Instant Action maps and 2 multiplayer maps.");
  }
  // MechWarrior 3's c1 as the world: its gamez.zbd (version 27) is not a world PM reads; left out, said.
  const std::string mw3_world = std::string(MW3_GAME_DIR) + "/zbd/c1";
  if (fs::exists(mw3_world + "/gamez.zbd", ec)) {
    const std::string wd2 = dir + "/wd2", src2 = dir + "/export2/Iron Coast";
    fs::create_directories(wd2, ec);
    EXPECT(make_export(mw3_world, src2));
    int errors = 0;
    bool said = false;
    for (const std::string& l : mech3::mod_check(src2, install, &errors))
      said |= l.find("zbd/ironcoast/gamez.zbd: not a world the game can read") != std::string::npos;
    EXPECT(said);
  }
  zutl::mounts() = keep;
  fs::remove_all(dir, ec);
}
