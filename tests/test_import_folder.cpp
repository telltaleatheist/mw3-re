// IMPORT from a modded game folder (docs/modding.md; upgrades/mods/import_folder.h):
// installs built here in the test's folders, nothing of a player's touched.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "mech3/mod_install.h"
#include "mech3/mods.h"
#include "upgrades/mods/import_folder.h"

namespace {
namespace fs = std::filesystem;

void put(const fs::path& p, const std::string& text) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  std::ofstream(p, std::ios::binary) << text;
}
std::string tmp(const char* name) {
  const std::string d = std::string(MW3_TEST_TMP) + "/" + name;
  std::error_code ec;
  fs::remove_all(d, ec);
  fs::create_directories(d, ec);
  return d;
}
// Every file under a folder with its bytes: a folder is unchanged when this is.
std::map<std::string, std::string> snapshot(const std::string& dir) {
  std::map<std::string, std::string> m;
  std::error_code ec;
  for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_regular_file(ec)) {
      std::ifstream f(it->path(), std::ios::binary);
      std::stringstream ss;
      ss << f.rdbuf();
      m[fs::relative(it->path(), dir, ec).generic_string()] = ss.str();
    }
  return m;
}
std::string names(const std::string& dir) {
  std::string s;
  for (const auto& [rel, data] : snapshot(dir)) s += rel + "|";
  return s;
}
bool nothing_staged(const std::string& wd) {
  std::error_code ec;
  return !fs::exists(fs::path(wd) / "mod-import", ec) && !fs::exists(fs::path(wd) / "mod-staging", ec);
}
// The player's install, and the same with a mod's installer run over it.
void build(const std::string& inst, const std::string& modded) {
  for (const std::string& d : {inst, modded}) {
    put(d + "/Mech3.exe", "the game");
    put(d + "/Ir50_32.dll", "a dll");
    put(d + "/zbd/reader.txt", "the reader");
    put(d + "/zbd/c1/gamez.txt", "abc");
    put(d + "/zbd/c4b/m5.txt", "a mission only MechWarrior 3 has");
    put(d + "/video/intro.avi", "a movie");
  }
  put(modded + "/Mech3.exe", "a patched game");            // a program
  put(modded + "/DDraw.dll", "dgVoodoo");                  // its dll
  put(modded + "/dgVoodoo.conf", "settings");              // and its settings
  put(modded + "/unins000.dat", "the installer's");        // the installer's own
  put(modded + "/zbd/reader.txt", "the mod's reader");     // differs
  put(modded + "/zbd/c1/gamez.txt", "abd");                // the same size, other bytes
  put(modded + "/zbd/c1/New.txt", "new");                  // new
  put(modded + "/pilots/Someone.txt", "a pilot");          // the player's own kind
  put(modded + "/saves/game.sav", "a save");
  put(modded + "/Readme.txt", "about the mod");            // loose at the top
  put(modded + "/Extras/art.txt", "not a game folder");    // a folder the install does not have
}
}  // namespace

// A whole modded install: only the files that differ or are new under the
// install's own folders, as a new mod; both installs left exactly as they were.
TEST(import_folder_takes_what_differs_from_the_install) {
  const std::string wd = tmp("import_folder_wd"), inst = tmp("import_folder_inst"), modded = tmp("import_folder_src") + "/Big Guns";
  build(inst, modded);
  const auto inst_before = snapshot(inst), modded_before = snapshot(modded);
  EXPECT(mech3::import_folder_is_install(modded) && mech3::import_folder_is_install(inst) && !mech3::import_folder_is_install(wd));
  int asked = 0;
  std::string report;
  const mech3::ModAddDone d = mech3::import_folder_run(wd, modded, inst, [&](const std::string&) { return ++asked, 1; }, nullptr, &report);
  std::printf("  %s\n  %s\n", d.message.c_str(), report.c_str());
  EXPECT(d.ok && d.folder == "Big Guns" && d.name == "Big Guns" && asked == 0);  // the game from the install's own files
  const std::string at = wd + "/mods/" + d.folder;
  std::printf("  the mod: %s\n", names(at).c_str());
  EXPECT(names(at) == std::string("mod.txt|") + mech3::kModAddedMarker + "|zbd/c1/New.txt|zbd/c1/gamez.txt|zbd/reader.txt|");
  const auto got = snapshot(at);
  auto has = [&](const char* rel, const char* text) { return got.count(rel) && got.at(rel).find(text) != std::string::npos; };
  EXPECT(has("zbd/reader.txt", "the mod's reader") && has("zbd/c1/gamez.txt", "abd"));
  EXPECT(has("mod.txt", "name=Big Guns\n") && has("mod.txt", "for=mw3\n"));
  EXPECT(report == "Took 3 files that differ from your install; left 2 the same; the program and its dlls are not part of a mod "
                   "here (5); pilots, saves and settings stay yours (2); 2 files outside the game's folders left.");
  EXPECT(snapshot(inst) == inst_before && snapshot(modded) == modded_before);  // read only
  EXPECT(nothing_staged(wd));
  // The counts, and the staging folder's own life.
  mech3::ImportFolder f = mech3::import_folder_prepare(wd, modded, inst);
  EXPECT(f.install && f.error.empty() && f.taken == 3 && f.same == 2 && f.programs == 5 && f.own == 2 && f.other == 2);
  EXPECT(f.game == mech3::ModGame::kMw3 && fs::path(f.picked).filename() == "Big Guns" &&
         fs::exists(fs::path(f.staging) / mech3::kModStagingMarker));
  EXPECT(names(f.picked) == "zbd/c1/New.txt|zbd/c1/gamez.txt|zbd/reader.txt|");
  // A folder of the player's in mod-import (no marker) is never removed.
  put(wd + "/mod-import/theirs/keep.txt", "keep me");
  mech3::import_folder_discard(f);
  EXPECT(f.staging.empty() && fs::exists(wd + "/mod-import/theirs/keep.txt"));
  // A second import of the same folder: a second mod, the first untouched.
  const mech3::ModAddDone d2 = mech3::import_folder_run(wd, modded, inst, nullptr, nullptr);
  EXPECT(d2.ok && d2.folder == "Big Guns (2)" && names(at) == std::string("mod.txt|") + mech3::kModAddedMarker +
                                                                 "|zbd/c1/New.txt|zbd/c1/gamez.txt|zbd/reader.txt|");
  EXPECT(fs::exists(wd + "/mod-import/theirs/keep.txt"));
}

// Refused, and said: the player's own install, one the same as it, the other
// game's, a folder that holds the write dir. Nothing is made for any.
TEST(import_folder_refusals) {
  const std::string wd = tmp("import_refuse_wd"), inst = tmp("import_refuse_inst"), modded = tmp("import_refuse_src") + "/Mod";
  build(inst, modded);
  std::string report = "x";
  mech3::ModAddDone d = mech3::import_folder_run(wd, inst, inst, nullptr, nullptr, &report);
  std::printf("  own: %s\n", d.message.c_str());
  EXPECT(!d.ok && d.message == "Not added: that folder is your own install: pick the folder the mod was installed over." && report.empty());
  // The same files as the install's (programs and a pilot aside): nothing to add.
  const std::string same = tmp("import_refuse_same") + "/Copy";
  for (const auto& [rel, data] : snapshot(inst)) put(same + "/" + rel, data);
  put(same + "/Mech3.exe", "another exe"), put(same + "/pilots/p.txt", "a pilot");
  d = mech3::import_folder_run(wd, same, inst, nullptr, nullptr);
  std::printf("  same: %s\n", d.message.c_str());
  EXPECT(!d.ok && d.message.find("nothing to add") != std::string::npos);
  // Pirate's Moon's install against MechWarrior 3's.
  const std::string pm = tmp("import_refuse_pm") + "/Pirates";
  put(pm + "/Mech3.exe", "pm"), put(pm + "/zbd/c1/readersa1.txt", "pm's"), put(pm + "/zbd/reader.txt", "pm's reader");
  d = mech3::import_folder_run(wd, pm, inst, nullptr, nullptr);
  std::printf("  other game: %s\n", d.message.c_str());
  EXPECT(!d.ok && d.message == "Not added: that folder is a Pirate's Moon install, and yours is MechWarrior 3.");
  // A modded install with the write dir inside it (settings beside the game).
  const std::string outer = tmp("import_refuse_outer") + "/Game";
  build(tmp("import_refuse_outer_inst"), outer);
  d = mech3::import_folder_run(outer + "/settings", outer, inst, nullptr, nullptr);
  EXPECT(!d.ok && d.message.find("the game's own settings") != std::string::npos);
  // No install to compare with.
  d = mech3::import_folder_run(wd, modded, "", nullptr, nullptr);
  EXPECT(!d.ok && d.message.find("no install of yours") != std::string::npos);
  std::error_code ec;
  EXPECT(!fs::exists(wd + "/mods", ec) && nothing_staged(wd));
}

// Any other pick goes to mod_install's add as it is; a link in a modded
// install is not followed.
TEST(import_folder_other_picks_and_links) {
  const std::string wd = tmp("import_other_wd"), inst = tmp("import_other_inst"), modded = tmp("import_other_src") + "/Linked";
  build(inst, modded);
  const std::string plain = tmp("import_other_plain") + "/Plain Mod";
  put(plain + "/zbd/c4b/thing.txt", "a mod's file");  // a mod's folder: no exe
  std::string report = "x";
  mech3::ModAddDone d = mech3::import_folder_run(wd, plain, inst, nullptr, nullptr, &report);
  EXPECT(d.ok && d.folder == "Plain Mod" && report.empty() && !mech3::import_folder_is_install(plain));
#if !defined(_WIN32)
  const std::string outside = tmp("import_other_outside");
  put(outside + "/secret.txt", "not the mod's");
  std::error_code ec;
  fs::create_directory_symlink(outside, modded + "/zbd/linked", ec);
  fs::create_symlink(outside + "/secret.txt", modded + "/zbd/link.txt", ec);
  EXPECT(!ec);
  mech3::ImportFolder f = mech3::import_folder_prepare(wd, modded, inst);
  EXPECT(f.error.empty() && f.taken == 3 && f.other == 4);  // the two links counted with what is left
  EXPECT(names(f.picked) == "zbd/c1/New.txt|zbd/c1/gamez.txt|zbd/reader.txt|");
  mech3::import_folder_discard(f);
  EXPECT(fs::exists(outside + "/secret.txt"));
#endif
}

// A real modded install, when one is named: MW3_IMPORT_FOLDER=<folder> against
// the test's install (read only; the mod lands in the test's own write dir).
TEST(import_folder_real_install_env) {
  const char* folder = std::getenv("MW3_IMPORT_FOLDER");
  if (!folder || !*folder) return;
  const std::string wd = tmp("import_real_wd");
  std::string report;
  const mech3::ModAddDone d = mech3::import_folder_run(wd, folder, MW3_GAME_DIR, nullptr, nullptr, &report);
  std::printf("  %s\n  %s\n", d.message.c_str(), report.c_str());
  EXPECT(d.ok && nothing_staged(wd));
}
