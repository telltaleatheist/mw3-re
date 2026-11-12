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
#include "pm_side.h"
#include "message_dll.h"
#include "gamez/zutil/zutl_msgdll.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/mod_install.h"
#include "upgrades/mods/mod_report.h"
#include "mech3/mods.h"
#include "upgrades/mods/import_folder.h"
#include "upgrades/mods/import_texts.h"

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
struct KeepMounts {
  zutl::Mounts saved = zutl::mounts();
  ~KeepMounts() { zutl::mounts() = saved; }
};
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
    put(d + "/" + TEST_ONLY_OURS, "a file only this game has");
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
  EXPECT(has("mod.txt", "name=Big Guns\n") && has("mod.txt", TEST_IS_PM ? "for=mw3pm\n" : "for=mw3\n"));
  // The marker names the modded folder it came from, not our staging copy.
  EXPECT(has(mech3::kModAddedMarker, ("Added by the game from " + modded + ".").c_str()) && !has(mech3::kModAddedMarker, "mod-import"));
  EXPECT(report == "Took 3 files that differ from your install; the program and its dlls are not part of a mod here." &&
         report.size() <= 2 * 58);  // the screen's two lines
  EXPECT(snapshot(inst) == inst_before && snapshot(modded) == modded_before);  // read only
  EXPECT(nothing_staged(wd));
  // The counts, and the staging folder's own life.
  mech3::ImportFolder f = mech3::import_folder_prepare(wd, modded, inst);
  EXPECT(mech3::import_folder_report(f) ==
         "Took 3 files that differ from your install; left 2 the same; the program and its dlls are not part of a mod "
         "here (5); pilots, saves and settings stay yours (2); 2 files outside the game's folders left.");
  EXPECT(f.version.empty() && f.install_version.empty() && f.warning.empty());  // made-up data: no version to tell
  EXPECT(f.install && f.error.empty() && f.taken == 3 && f.same == 2 && f.programs == 5 && f.own == 2 && f.other == 2);
  EXPECT(f.game == (TEST_IS_PM ? mech3::ModGame::kPm : mech3::ModGame::kMw3) && fs::path(f.picked).filename() == "Big Guns" &&
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
// IMPORT's one file box takes a folder mod by a file inside it: the mod.txt, a file beside it, one under zbd
// (three folders up at most); an archive, a loadout, a folder or a lone program is the pick itself.
TEST(import_pick_root_is_the_mods_folder) {
  namespace fs = std::filesystem;
  const std::string dir = std::string(MW3_TEST_TMP) + "/pick root";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir + "/mod/zbd/c1", ec);
  fs::create_directories(dir + "/laid/zbd", ec);
  fs::create_directories(dir + "/loose/a/b/c/d", ec);
  for (const char* f : {"/mod/mod.txt", "/mod/readme.txt", "/mod/zbd/c1/readerm1.zbd", "/laid/Mech3.exe", "/laid/zbd/reader.zbd", "/mod/pack.zip",
                        "/mod/Thor.m3c", "/loose/setup.exe", "/loose/a/b/c/d/deep.txt"})
    std::ofstream(dir + f) << "x";
  const auto root = [&](const char* f) { return fs::path(mech3::import_pick_root(dir + f)).lexically_normal().generic_string(); };
  const std::string mod = fs::path(dir + "/mod").lexically_normal().generic_string(), laid = fs::path(dir + "/laid").lexically_normal().generic_string();
  EXPECT(root("/mod/mod.txt") == mod && root("/mod/readme.txt") == mod && root("/mod/zbd/c1/readerm1.zbd") == mod);
  EXPECT(root("/laid/Mech3.exe") == laid && root("/laid/zbd/reader.zbd") == laid);  // a game folder a mod was laid over
  EXPECT(root("/mod/pack.zip") == fs::path(dir + "/mod/pack.zip").lexically_normal().generic_string());
  EXPECT(root("/mod/Thor.m3c") == fs::path(dir + "/mod/Thor.m3c").lexically_normal().generic_string());
  EXPECT(root("/loose/setup.exe") == fs::path(dir + "/loose/setup.exe").lexically_normal().generic_string());  // a lone installer
  EXPECT(root("/loose/a/b/c/d/deep.txt") == fs::path(dir + "/loose/a/b/c/d/deep.txt").lexically_normal().generic_string());
  EXPECT(root("/mod") == mod && mech3::import_pick_root(dir + "/none.txt") == dir + "/none.txt");
  fs::remove_all(dir, ec);
}

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
  // The other game's install against this one's.
  const std::string pm = tmp("import_refuse_pm") + "/Other";
  put(pm + "/Mech3.exe", "pm"), put(pm + "/" + TEST_ONLY_THEIRS, "the other's"), put(pm + "/zbd/reader.txt", "the other's reader");
  d = mech3::import_folder_run(wd, pm, inst, nullptr, nullptr);
  std::printf("  other game: %s\n", d.message.c_str());
  EXPECT(!d.ok && d.message == std::string("Not added: that folder is a ") + TEST_THEIR_NAME + " install, and yours is " + TEST_OUR_NAME + ".");
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
  put(plain + "/zbd/c1/thing.txt", "a mod's file");  // a mod's folder: no exe
  std::string report = "x";
  mech3::ModAddDone d = mech3::import_folder_run(wd, plain, inst, [](const std::string&) { return TEST_OUR_ANSWER; }, nullptr, &report);
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
#ifndef MW3PM_GAME_DIR  // Pirate's Moon's: pm_import_folder_real_install_env (tests/pm/test_pm_mods_screen.cpp)
TEST(import_folder_real_install_env) {
  const char* folder = std::getenv("MW3_IMPORT_FOLDER");
  if (!folder || !*folder) return;
  // MW3_IMPORT_WD=<a scratch write folder>: the mod is added there and kept (several runs: several mods in one
  // write folder, to play them together); without it, the test's own folder, gone when the tests end.
  const char* keep = std::getenv("MW3_IMPORT_WD");
  const std::string wd = keep && *keep ? std::string(keep) : tmp("import_real_wd");
  KeepMounts mounts;
  zutl::mounts().confined = true;  // as the program has it when IMPORT runs
  std::string report;
  const mech3::ModAddDone d = mech3::import_folder_run(wd, folder, MW3_GAME_DIR, nullptr, nullptr, &report);
  std::printf("  %s\n  %s\n  in %s/mods/%s\n", d.message.c_str(), report.c_str(), wd.c_str(), d.folder.c_str());
  EXPECT(d.ok && nothing_staged(wd));
}
#endif

// Another version of the data than the player's is said, and still imports:
// the real 1.0 install as the modded folder against the 1.2 one (when both
// are there).
#ifndef MW3PM_GAME_DIR  // MechWarrior 3's two versions
TEST(import_folder_says_another_version) {
  const std::string v12 = MW3_GAME_DIR, v10 = std::string(MW3_GAME_DIR) + "/../mw3-1.0";
  std::error_code ec;
  if (!fs::exists(v12 + "/zbd/reader.zbd", ec) || !fs::exists(v10 + "/zbd/reader.zbd", ec)) {
    std::printf("  (skipped: needs the 1.2 and the 1.0 installs)\n");
    return;
  }
  EXPECT(mech3::import_folder_version(v12) == "1.2" && mech3::import_folder_version(v10) == "1.0");
  const std::string wd = tmp("import_version_wd");
  mech3::ImportFolder f = mech3::import_folder_prepare(wd, v10, v12);
  std::printf("  %s\n  short: %s\n", mech3::import_folder_report(f).c_str(), mech3::import_folder_report_short(f).c_str());
  EXPECT(f.error.empty() && f.taken > 0 && f.version == "1.0" && f.install_version == "1.2");
  EXPECT(f.warning == "This folder is version 1.0 and your game is 1.2, so the differences between the versions are taken too: "
                      "for a clean mod, install it onto a copy of the same version.");
  EXPECT(mech3::import_folder_report(f).rfind(f.warning, 0) == 0);  // said first
  EXPECT(mech3::import_folder_report_short(f).find("version 1.0 and your game 1.2") != std::string::npos &&
         mech3::import_folder_report_short(f).size() <= 2 * 58);
  mech3::import_folder_discard(f);
  // The same version on both sides: nothing said.
  mech3::ImportFolder same = mech3::import_folder_prepare(wd, v12, v12 + "/../mw3-1.2/.");
  EXPECT(same.warning.empty());
  mech3::import_folder_discard(same);
  EXPECT(nothing_staged(wd));
}
#endif

#if !defined(_WIN32)
// A linked folder inside the modded game folder (a symlink; on Windows a
// junction counts the same, marked_remove.h): never walked into, left out,
// and said in the report; what it points at is neither read as the mod's nor
// changed.
TEST(import_folder_leaves_a_linked_folder_out_and_says_so) {
  const std::string wd = tmp("import_link_wd"), inst = tmp("import_link_inst"), modded = tmp("import_link_src") + "/Mod",
                    elsewhere = tmp("import_link_else");
  build(inst, modded);
  put(elsewhere + "/huge.txt", "somewhere else on the disk");
  std::error_code ec;
  fs::create_directory_symlink(elsewhere, modded + "/zbd/linked", ec);
  EXPECT(!ec);
  const auto before = snapshot(elsewhere);
  mech3::ImportFolder f = mech3::import_folder_prepare(wd, modded, inst);
  const std::string report = mech3::import_folder_report(f);
  std::printf("  %s\n", report.c_str());
  EXPECT(f.error.empty() && f.taken == 3 && (f.linked == std::vector<std::string>{"zbd/linked"}));
  EXPECT(report.find("a linked folder was left out: zbd/linked") != std::string::npos);
  EXPECT(names(f.picked).find("huge.txt") == std::string::npos && snapshot(elsewhere) == before);
  mech3::import_folder_discard(f);
  EXPECT(nothing_staged(wd) || !fs::exists(f.staging, ec));
  // A mod's own folder with a link in it is refused whole, as before.
  const mech3::ModAddDone d = mech3::mod_add_run(wd, modded + "/zbd", inst, [](const std::string&) { return 1; }, nullptr);
  std::printf("  a mod's folder with a link: %s\n", d.message.c_str());
  EXPECT(!d.ok && d.message.find("it holds a link (linked)") != std::string::npos && snapshot(elsewhere) == before);
}
#endif

// One multiplayer map's reader archive with an entry no reader could hold (a list of a billion; a hand-edited
// file) does not stop the rest of a mod being added: IMPORT says so and leaves that one file out. A map whose
// entry only says an item more than it holds (the original reads on into the next entry) is added as it is.
TEST(import_folder_leaves_out_an_unreadable_reader_archive_and_adds_the_rest) {
  const TestGame game;  // the archives are made and checked as this game's
  const std::string wd = tmp("import_unread_wd"), inst = tmp("import_unread_inst"), modded = tmp("import_unread_src") + "/Mod";
  build(inst, modded);
  auto le = [](std::vector<uint8_t>& v, int32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back(static_cast<uint8_t>(x >> (8 * i)));
  };
  auto archive = [&](const std::string& path, const std::vector<std::pair<const char*, std::vector<uint8_t>>>& entries) {
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    zutl_zar::Archive a;
    zutl_zar::init(a);
    EXPECT(zutl_zar::create(a, path.c_str()));
    for (const auto& [name, bytes] : entries) zutl_zar::add(a, name, bytes.data(), static_cast<uint32_t>(bytes.size()));
    EXPECT(zutl_zar::close(a));
    zutl_zar::destroy(a);
  };
  std::vector<uint8_t> good, huge, past;
  le(good, 4), le(good, 2), le(good, 1), le(good, 7);                    // (7)
  le(huge, 4), le(huge, 1000000000), le(huge, 1), le(huge, 7);           // a list of a billion
  le(past, 4), le(past, 3), le(past, 1), le(past, 5);                    // one item more than it holds
  archive(modded + "/zbd/c1/readermp90.zbd", {{"aiv.zrd", huge}, {"mpinfo.zrd", good}});
  archive(modded + "/zbd/c1/readermp91.zbd", {{"aiv.zrd", past}, {"mpinfo.zrd", good}});
  const auto before = snapshot(modded);
  std::vector<std::string> enabled;
  const mech3::ModAddDone d = mech3::import_folder_run(wd, modded, inst, [](const std::string&) { return TEST_OUR_ANSWER; }, &enabled);
  std::printf("  %s\n", d.message.c_str());
  EXPECT(d.ok && d.warnings == 2 && d.left_out == 1 && d.message.rfind("Added " + d.name + ": 2 warnings: INFO shows them. ", 0) == 0 &&
         d.message.size() > ("Added " + d.name + ": 2 warnings: INFO shows them. ").size());  // then what to do next, as without warnings
  std::printf("  %s\n", d.message.c_str());
  const std::string mod = wd + "/mods/" + d.folder;
  std::error_code ec;
  EXPECT(!fs::exists(mod + "/zbd/c1/readermp90.zbd", ec) && fs::is_regular_file(mod + "/zbd/c1/readermp91.zbd", ec) &&
         fs::is_regular_file(mod + "/zbd/reader.txt", ec));
  EXPECT(snapshot(modded) == before && nothing_staged(wd));  // the player's folder as it was
  // The check on the added mod: nothing refused, nothing unreadable left in it.
  int errors = -1;
  std::string text;
  for (const std::string& l : mech3::mod_check(mod, inst, &errors)) text += l + "\n";
  // and what IMPORT left out is still said, from its record in the mod's folder, with the entry read short.
  EXPECT(errors == 0 && text.find("0 errors, 2 warnings") != std::string::npos);
  EXPECT(text.find("warning: zbd/c1/readermp90.zbd: aiv.zrd: a list count of 1000000000, 0 bytes into the entry, that no file could hold. The rebuild: "
                   "nothing of the entry can be read, so IMPORT left this file out; the game's own, if it has one, is used.") != std::string::npos);
  EXPECT(text.find("warning: zbd/c1/readermp91.zbd: aiv.zrd: ") != std::string::npos);
  EXPECT(fs::is_regular_file(mod + "/" + mech3::kModLeftOutFile, ec) && text.find(mech3::kModLeftOutFile) == std::string::npos);
}

// --- A mod's Mech3Msg.dll: its texts into strings.txt (upgrades/mods/import_texts.h) ---------------------
namespace {
// The mod's strings.txt as the game takes it at a start, over the player's own table.
std::map<int32_t, std::string> texts_with(const std::string& wd, const std::string& folder, const std::string& inst,
                                          std::map<std::string, int32_t>* names = nullptr) {
  KeepMounts keep;
  zutl::MessageDll mine;
  zutl::read_message_dll(inst + "/Mech3Msg.dll", mine);
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mods_mount(wd, {folder}, {});
  mech3::mods_apply_strings(mine.text, mine.ids, names);
  return mine.text;
}
const std::map<int32_t, std::string> kMine = {{100, "CAMPAIGN"}, {101, "INSTANT ACTION"}, {102, "Line one\r\nLine two"}, {200, "Caf\xe9"}};
const std::map<int32_t, std::string> kTheirs = {{100, "KAMPAGNE"}, {101, "INSTANT ACTION"}, {102, "Line one\r\nZwei \\ drei"},
                                                {200, "Caf\xe9"}, {201, "Stra\xdf" "e"}, {300, "A new briefing"}};
}  // namespace

// The made dll reads as the game's does, and the lines for what differs: by number, UTF-8, \n and \; a name
// the player's table lacks, or has under another number, as NAME@id with its text.
TEST(import_texts_lines_for_what_differs_and_new_names) {
  const std::string dir = tmp("import_texts_lines");
  put(dir + "/a.dll", message_dll(kTheirs));
  zutl::MessageDll theirs, mine;
  EXPECT(zutl::read_message_dll(dir + "/a.dll", theirs) && theirs.text == kTheirs);
  mine.text = kMine;
  int taken = -1, names = -1;
  const std::string lines = mech3::import_texts_lines(theirs, mine, &taken, &names);
  std::printf("%s", lines.c_str());
  EXPECT(lines == "0x64=KAMPAGNE\n0x66=Line one\\nZwei \\\\ drei\n0xc9=Stra\xc3\x9f" "e\n0x12c=A new briefing\n" && taken == 4 && names == 0);
  EXPECT(mech3::import_texts_lines(mine, mine, &taken, &names).empty() && taken == 0);
  // Line ends alike are the same text.
  zutl::MessageDll unix_ends = mine;
  unix_ends.text[102] = "Line one\nLine two";
  EXPECT(mech3::import_texts_lines(unix_ends, mine, &taken, &names).empty());
  // Names: one new, one moved to another number, one the same.
  mine.ids = {{"MSG_MOVED", 150}, {"MSG_SAME", 100}};
  theirs.ids = {{"MSG_NEW", 300}, {"MSG_MOVED", 101}, {"MSG_SAME", 100}};
  const std::string named = mech3::import_texts_lines(theirs, mine, &taken, &names);
  std::printf("%s", named.c_str());
  EXPECT(named == "0x64=KAMPAGNE\nMSG_MOVED@0x65=INSTANT ACTION\n0x66=Line one\\nZwei \\\\ drei\n0xc9=Stra\xc3\x9f" "e\n"
                  "MSG_NEW@0x12c=A new briefing\n" && taken == 5 && names == 2);
}

// A modded game folder whose Mech3Msg.dll differs: its texts taken into the mod's strings.txt, the dll not in the
// mod, both installs as they were; the game then shows them. With the program's own confinement on.
TEST(import_folder_takes_the_texts_of_a_modded_mech3msg_dll) {
  const std::string wd = tmp("import_texts_wd"), inst = tmp("import_texts_inst"), modded = tmp("import_texts_src") + "/Big Words";
  build(inst, modded);
  put(inst + "/Mech3Msg.dll", message_dll(kMine));
  put(modded + "/Mech3Msg.dll", message_dll(kTheirs));
  const auto inst_before = snapshot(inst), modded_before = snapshot(modded);
  KeepMounts keep;
  zutl::mounts().confined = true;
  std::string report;
  const mech3::ModAddDone d = mech3::import_folder_run(wd, modded, inst, nullptr, nullptr, &report);
  std::printf("  %s\n  %s\n", d.message.c_str(), report.c_str());
  const std::string at = wd + "/mods/" + d.folder;
  std::printf("  the mod: %s\n", names(at).c_str());
  EXPECT(d.ok && d.texts.found && d.texts.read && d.texts.taken == 4 && d.texts.names == 0 && d.warnings == 0);
  EXPECT(names(at) == std::string("mod.txt|") + mech3::kModAddedMarker + "|strings.txt|zbd/c1/New.txt|zbd/c1/gamez.txt|zbd/reader.txt|");
  EXPECT(snapshot(at).at("strings.txt").find("\n0x64=KAMPAGNE\n0x66=Line one\\nZwei \\\\ drei\n0xc9=Stra\xc3\x9f" "e\n0x12c=A new briefing\n") !=
         std::string::npos);
  EXPECT(snapshot(inst) == inst_before && snapshot(modded) == modded_before && nothing_staged(wd));
  // The long line says the count; the files' count does not include the dll.
  mech3::ImportFolder f = mech3::import_folder_prepare(wd, modded, inst);
  f.texts = d.texts;
  std::printf("  %s\n", mech3::import_folder_report(f).c_str());
  EXPECT(f.taken == 3 && f.programs == 5 && f.msg_dll &&
         mech3::import_folder_report(f).find("; took 4 texts from its Mech3Msg.dll.") != std::string::npos);
  mech3::import_folder_discard(f);
  // As the game takes them at a start.
  const std::map<int32_t, std::string> text = texts_with(wd, d.folder, inst);
  EXPECT(text.at(100) == "KAMPAGNE" && text.at(101) == "INSTANT ACTION" && text.at(102) == "Line one\r\nZwei \\ drei" &&
         text.at(200) == "Caf\xe9" && text.at(201) == "Stra\xdf" "e" && text.at(300) == "A new briefing");
  // The check and the report take the mod as it is: no warning for its strings.txt.
  int errors = 0;
  mech3::ModFindings facts;
  mech3::mod_check(at, inst, &errors, &facts);
  EXPECT(errors == 0 && facts.notes.empty());
}

// Only the texts differ: a mod of strings.txt alone. The same dll as the player's: a program as before, and
// with nothing else changed there is nothing to add.
TEST(import_folder_of_texts_alone_and_of_the_players_own_dll) {
  const std::string wd = tmp("import_texts_alone_wd"), inst = tmp("import_texts_alone_inst"), src = tmp("import_texts_alone_src");
  const std::string words = src + "/Only Words", same = src + "/Same";
  build(inst, same);
  put(inst + "/Mech3Msg.dll", message_dll(kMine));
  std::error_code ec;
  fs::copy(inst, words, fs::copy_options::recursive, ec);
  put(words + "/Mech3Msg.dll", message_dll(kTheirs));
  std::string report;
  const mech3::ModAddDone d = mech3::import_folder_run(wd, words, inst, nullptr, nullptr, &report);
  std::printf("  %s\n  %s\n", d.message.c_str(), report.c_str());
  EXPECT(d.ok && d.texts.taken == 4 && names(wd + "/mods/" + d.folder) == std::string("mod.txt|") + mech3::kModAddedMarker + "|strings.txt|");
  EXPECT(texts_with(wd, d.folder, inst).at(300) == "A new briefing");
  // The player's own dll in the modded folder: counted with the programs, as before this.
  put(same + "/Mech3Msg.dll", message_dll(kMine));
  mech3::ImportFolder f = mech3::import_folder_prepare(wd, same, inst);
  EXPECT(f.error.empty() && !f.msg_dll && f.taken == 3 && f.programs == 6);
  mech3::import_folder_discard(f);
  // Another dll with the very same texts, and nothing else changed: nothing to add, said.
  put(words + "/Mech3Msg.dll", message_dll(kMine) + "padding");
  const mech3::ModAddDone equal = mech3::import_folder_run(wd, words, inst, nullptr, nullptr, &report);
  std::printf("  %s\n", equal.message.c_str());
  EXPECT(!equal.ok && equal.message.find("texts are the same as yours and it holds nothing else") != std::string::npos && nothing_staged(wd));
  fs::copy_file(inst + "/Mech3Msg.dll", words + "/Mech3Msg.dll", fs::copy_options::overwrite_existing, ec);
  const mech3::ModAddDone none = mech3::import_folder_run(wd, words, inst, nullptr, nullptr, &report);
  EXPECT(!none.ok && none.message.find("nothing to add") != std::string::npos && nothing_staged(wd));
}

// A dll that is no message table: a warning in the mods' one form, kept with the mod for REPORT, and the mod
// added all the same. A zip-shaped mod (no program) with a strings.txt of its own keeps its lines, which win.
TEST(import_texts_unreadable_dll_warns_and_own_strings_win) {
  const std::string wd = tmp("import_texts_bad_wd"), inst = tmp("import_texts_bad_inst"), modded = tmp("import_texts_bad_src") + "/Bad Words";
  build(inst, modded);
  put(inst + "/Mech3Msg.dll", message_dll(kMine));
  put(modded + "/Mech3Msg.dll", "not a dll at all");
  std::string report;
  const mech3::ModAddDone d = mech3::import_folder_run(wd, modded, inst, nullptr, nullptr, &report);
  std::printf("  %s\n", d.message.c_str());
  const std::string at = wd + "/mods/" + d.folder;
  EXPECT(d.ok && d.texts.found && !d.texts.read && d.warnings == 1 && d.message.find("1 warning: INFO shows it.") != std::string::npos);
  EXPECT(names(at) == std::string("mod.txt|") + mech3::kModAddedMarker + "|" + mech3::kModLeftOutFile + "|zbd/c1/New.txt|zbd/c1/gamez.txt|zbd/reader.txt|");
  int errors = 0;
  mech3::ModFindings facts;
  mech3::mod_check(at, inst, &errors, &facts);
  std::string said;
  for (const mech3::ModNote& n : facts.notes) said += mech3::mod_warning_line(n.where, n.w) + "\n";
  std::printf("  %s", said.c_str());
  EXPECT(errors == 0 && facts.notes.size() == 1 && said.find("Mech3Msg.dll: it does not read as the game's table of texts. The rebuild: IMPORT left this file out") == 0);
  // A folder of files to lay over the game (no program in it), its own strings.txt beside the dll.
  const std::string loose = tmp("import_texts_loose_src") + "/Loose Words";
  put(loose + "/zbd/c1/New.txt", "new");
  put(loose + "/Mech3Msg.dll", message_dll(kTheirs));
  put(loose + "/strings.txt", "\xef\xbb\xbf" "100=Mine\n");
  const auto loose_before = snapshot(loose);
  const mech3::ModAddDone l = mech3::mod_add_run(wd, loose, inst, [](const std::string&) { return TEST_OUR_ANSWER; }, nullptr);  // which game: this one
  std::printf("  %s\n", l.message.c_str());
  EXPECT(l.ok && l.texts.taken == 4 && snapshot(loose) == loose_before);
  EXPECT(names(wd + "/mods/" + l.folder) == std::string("mod.txt|") + mech3::kModAddedMarker + "|strings.txt|zbd/c1/New.txt|");
  const std::map<int32_t, std::string> text = texts_with(wd, l.folder, inst);
  EXPECT(text.at(100) == "Mine" && text.at(300) == "A new briefing");
}

// NAME@id in strings.txt: the text set and the name known from then on; the check reads the line without a table.
TEST(mods_strings_name_at_id_gives_a_message_its_name) {
  const std::string wd = tmp("import_texts_names_wd");
  put(wd + "/mods/n/mod.txt", "name=Names\n");
  put(wd + "/mods/n/strings.txt", "MSG_NEW@0x12c=A new briefing\nMSG_MOVED@101=Moved\nMSG_NEW=by its new name: not yet known on this line\n@5=x\nA@b=y\n");
  KeepMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mods_mount(wd, {"n"}, {});
  std::map<int32_t, std::string> text = {{101, "START"}};
  std::map<std::string, int32_t> ids = {{"MSG_MOVED", 150}};
  std::string log;
  for (const std::string& l : mech3::mods_apply_strings(text, ids, &ids)) log += l + "\n";
  std::printf("%s", log.c_str());
  EXPECT(text[300] == "A new briefing" && text[101] == "Moved" && ids["MSG_NEW"] == 300 && ids["MSG_MOVED"] == 101);
  EXPECT(text[5] == "x" && ids.size() == 2);  // no name before the @: the number alone
  EXPECT(log.find("line 3: no id= here") != std::string::npos && log.find("line 5: no id= here") != std::string::npos);
  // Without a names table (the check's): the texts all the same.
  std::map<int32_t, std::string> again;
  mech3::mods_apply_strings(again, {});
  EXPECT(again[300] == "A new briefing" && again[101] == "Moved");
}

// The game's own two dlls (1.0's and 1.2's, when both installs are there): 1.2's taken as a mod's over a 1.0
// player gives, through strings.txt, exactly 1.2's texts and names; a dll against itself gives nothing.
TEST(import_texts_real_dlls_round_trip_through_strings_txt) {
  const std::string v12 = std::string(MW3_GAME_DIR) + "/Mech3Msg.dll", v10 = std::string(MW3_GAME_DIR) + "/../mw3-1.0/Mech3Msg.dll";
  std::error_code ec;
  if (!fs::exists(v12, ec) || !fs::exists(v10, ec)) return;
  zutl::MessageDll theirs, mine;
  EXPECT(zutl::read_message_dll(v12, theirs) && zutl::read_message_dll(v10, mine));
  int taken = 0, names = 0;
  EXPECT(mech3::import_texts_lines(theirs, theirs, &taken, &names).empty() && taken == 0 && names == 0);
  const std::string wd = tmp("import_texts_real_wd");
  put(wd + "/mods/v12/mod.txt", "name=1.2's texts\n");
  put(wd + "/mods/v12/strings.txt", mech3::import_texts_lines(theirs, mine, &taken, &names));
  std::printf("  1.2 over 1.0: %d lines, %d with a name of their own; %zu and %zu messages\n", taken, names, theirs.text.size(), mine.text.size());
  EXPECT(taken > 0);
  KeepMounts keep;
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);
  mech3::mods_mount(wd, {"v12"}, {});
  std::string log;
  for (const std::string& l : mech3::mods_apply_strings(mine.text, mine.ids, &mine.ids)) log += l + "\n";
  EXPECT(log.find("left out") == std::string::npos);
  auto plain = [](std::string t) {
    t.erase(std::remove(t.begin(), t.end(), '\r'), t.end());
    return t.substr(0, 0x1ff);
  };
  int differ = 0, missing_names = 0;
  for (const auto& [id, text] : theirs.text)
    if (!mine.text.count(id) || plain(mine.text.at(id)) != plain(text)) {
      if (++differ <= 3) std::printf("    0x%x differs\n", static_cast<unsigned>(id));
    }
  for (const auto& [name, id] : theirs.ids)
    if (!mine.ids.count(name) || mine.ids.at(name) != id) ++missing_names;
  EXPECT(differ == 0 && missing_names == 0);
}
