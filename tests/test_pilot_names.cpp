// FIX (bug) for pilot names (docs/fixes.md): kept apart from test_fixes.cpp,
// whose headers declare a different Pilot.
#include <cstring>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zutil/zfix.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/pilot.h"

// Pilot names share a file without case (mech3/pilot.cpp): with the fix
// "BOB" finds bob and a rename onto a taken name is refused; the exe made a
// second pilot over bob.mw3 and renamed onto it (0x00478be0, 0x00477fe0).
TEST(fix_pilot_names_without_case) {
  zutl::mounts().read.clear();
  zutl::set_write_dir(std::string(MW3_TEST_TMP) + "/test_pilot_names");
  for (const bool fix : {true, false}) {
    zfix::set_all(fix);
    mech3::Pilot* bob = mech3::create_pilot("bob");
    mech3::Pilot* alice = mech3::create_pilot("alice");
    EXPECT(bob && alice && bob != alice);
    EXPECT((mech3::create_pilot("BOB") == bob) == fix);
    EXPECT(alice->rename("Bob") != fix);
    EXPECT((std::strcmp(alice->name, "alice") == 0) == fix);
    EXPECT(bob->rename("Bob"));  // its own name in other capitals
    EXPECT(std::strcmp(bob->name, "Bob") == 0);
    while (!mech3::pilots().empty()) mech3::delete_pilot(mech3::pilots().front()->name);
  }
  zfix::set_all(true);
}

namespace {
// The pilot module's calls out: the "Replace existing game?" answer, the
// worlds saved (zSave, 0x005a8a70) in order.
struct SaveHooks : mech3::PilotHooks {
  int32_t answer = 1;
  std::vector<std::string> saved;
  int32_t asked = 0;
  uint32_t unported(const char*, std::initializer_list<zgame::Arg>) override { return 0; }
  int32_t message_box(int32_t, int32_t, int32_t) override {
    ++asked;
    return answer;
  }
  void save_world(const char* path, int32_t) override { saved.push_back(path ? path : ""); }
};
}  // namespace

// FIX (bug) for the campaign's saved games (docs/fixes.md): the exe wrote the
// next mission's world before asking "Replace existing game?" (0x00478f10,
// zSave in 0x00434e00), so No kept the old entry over a file already holding
// the new world. With the fix the world is written after the answer, not on
// No; without it, as the exe: written first, whatever the answer.
TEST(fix_campaign_save_after_the_question) {
  zutl::mounts().read.clear();
  zutl::set_write_dir(std::string(MW3_TEST_TMP) + "/test_campaign_save");
  SaveHooks h;
  mech3::set_pilot_hooks(&h);
  for (const bool fix : {true, false}) {
    zfix::set_all(fix);
    for (const int32_t answer : {1, 2}) {
      mech3::Pilot* p = mech3::create_pilot("Saver");
      EXPECT(p && p->select_campaign(0));
      h.saved.clear();
      h.asked = 0;
      p->record_mission(true);  // Op 1 Mission 2, new: filed, its world saved
      EXPECT(h.asked == 0 && h.saved.size() == 1);
      EXPECT(p->select_campaign(0));
      h.answer = answer;
      h.saved.clear();
      p->record_mission(true);  // Mission 2 again: the question
      EXPECT(h.asked == 1);
      // Yes: saved either way; No: the exe saved anyway, the fix does not.
      EXPECT(h.saved.size() == (answer == 1 || !fix ? 1u : 0u));
      while (!mech3::pilots().empty()) mech3::delete_pilot(mech3::pilots().front()->name);
    }
  }
  zfix::set_all(true);
  mech3::set_pilot_hooks(nullptr);
}

// The mech lab's SORT_CHASSISNAME (0x004496c0, docs/fixes.md): with the fix
// by the names shown, without by the mechs' numbers as the exe.
#include "mech3/lab_screen.h"
namespace {
struct NameEngine : mech3::Engine {
  uint32_t unported(const char*, std::initializer_list<mech3::Arg>) override { return 0; }
  const char* message(int32_t id) override { return id == 0xd82 + 3 ? "Avatar" : id == 0xd82 + 1 ? "Zeus" : "X"; }
};
}  // namespace
TEST(fix_lab_sort_by_name) {
  NameEngine e;
  for (const bool fix : {true, false}) {
    zfix::set_all(fix);
    EXPECT(mech3::chassis_before(e, 0, 3, 1) == fix);   // Avatar (3) before Zeus (1) by name
    EXPECT(mech3::chassis_before(e, 0, 1, 3) == !fix);  // the exe: 1 before 3
    EXPECT(!mech3::chassis_before(e, 2, 1, 3));         // no other order
  }
  zfix::set_all(true);
}

// FIX (A) for the mech lab's Ctrl+X / Ctrl+I (docs/fixes.md): a loadout
// written to export\, put in import\, read back once, its checksum checked.
#include <cstdio>
#include "mech3/mech_config.h"
namespace mech3 {
const char* mech_name(int32_t mech);
}
TEST(fix_lab_loadout_export_import) {
  zutl::mounts().read.clear();
  const std::string dir = std::string(MW3_TEST_TMP) + "/test_lab_files";
  zutl::set_write_dir(dir);
  mech3::Config c(3, "Mine");
  EXPECT(mech3::lab_export_config(c, "Mine file"));
  EXPECT(zutl::exists("configs\\Mine file.m3c"));
  mech3::Config* in = mech3::lab_import_config("Mine file");
  EXPECT(in && in->mech == 3 && in->name && std::strcmp(in->name, "Mine") == 0);
  delete in;
  EXPECT(mech3::lab_import_config("No file") == nullptr);
}

// IMPORT of a mech loadout (upgrades/mods/import_loadout.h): one player's exported .m3c, picked on another's
// MODS screen, lands in that player's import folder under a free name (never over one), and the lab's Ctrl+I
// reads it once; a folder of loadouts the same; a file that is no loadout is refused; no mod is made and the
// picked files are only read. With the program's confinement on.
#include <filesystem>
#include <fstream>
#include <iterator>
#include "mech3/mod_install.h"
TEST(import_takes_a_mech_loadout_for_the_lab) {
  namespace fs = std::filesystem;
  const std::string root = std::string(MW3_TEST_TMP) + "/import_loadout", theirs = root + "/theirs", wd = root + "/mine";
  std::error_code ec;
  fs::remove_all(root, ec);
  const zutl::Mounts saved = zutl::mounts();
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(theirs);  // the player who made it
  mech3::Config c(3, "Mine"), c2(5, "Other");
  EXPECT(mech3::lab_export_config(c) && mech3::lab_export_config(c2));
  const std::string one = theirs + "/export/" + mech3::mech_name(3) + "_Mine.m3c";
  const auto bytes = [](const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  };
  const std::string one_before = bytes(one);
  EXPECT(!one_before.empty() && mech3::loadout_name(one) && mech3::loadout_reads(one, nullptr));
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd);  // the player who was given it
  zutl::mounts().confined = true;
  const mech3::ModAddDone a = mech3::mod_add_run(wd, one, "", nullptr, nullptr);
  std::printf("  %s\n", a.message.c_str());
  EXPECT(!a.ok && a.loadouts == 1 && a.message == "Added 1 mech loadout: in a mech lab, Ctrl+I takes it.");
  const std::string name = std::string(mech3::mech_name(3)) + "_Mine";
  EXPECT(bytes(wd + "/import/" + name + ".m3c") == one_before && bytes(one) == one_before);
  EXPECT(!fs::exists(wd + "/mods", ec) && !fs::exists(wd + "/mod-staging", ec));  // no mod, nothing staged
  // Again: a second file, the first as it was. The whole export folder: both, under free names.
  const mech3::ModAddDone b = mech3::mod_add_run(wd, one, "", nullptr, nullptr);
  EXPECT(b.loadouts == 1 && bytes(wd + "/import/" + name + " (2).m3c") == one_before && bytes(wd + "/import/" + name + ".m3c") == one_before);
  const mech3::ModAddDone all = mech3::mod_add_run(wd, theirs + "/export", "", nullptr, nullptr);
  std::printf("  %s\n", all.message.c_str());
  EXPECT(!all.ok && all.loadouts == 2 && all.message == "Added 2 mech loadouts: in a mech lab, Ctrl+I takes the next one.");
  EXPECT(fs::exists(wd + "/import/" + name + " (3).m3c", ec) && !fs::exists(wd + "/mod-staging", ec));
  // No loadout: refused, nothing made.
  { std::ofstream(root + "/bad.m3c") << "xx"; }
  std::string big(8, '\xff');
  { std::ofstream(root + "/odd.m3c", std::ios::binary) << big << std::string(100, 'a'); }
  const mech3::ModAddDone bad = mech3::mod_add_run(wd, root + "/bad.m3c", "", nullptr, nullptr), odd = mech3::mod_add_run(wd, root + "/odd.m3c", "", nullptr, nullptr);
  std::printf("  %s\n  %s\n", bad.message.c_str(), odd.message.c_str());
  EXPECT(!bad.ok && bad.loadouts == 0 && bad.message.find("Not added: bad.m3c: it does not read as a mech loadout") == 0 && !fs::exists(wd + "/import/bad.m3c", ec));
  EXPECT(odd.loadouts == 0 && !fs::exists(wd + "/import/odd.m3c", ec));
  // The lab's own import takes them, each once, as they were made.
  zutl::mounts().confined = false;
  int got = 0, mine = 0, other = 0;
  while (mech3::Config* in = mech3::lab_import_config()) {
    ++got;
    if (in->mech == 3 && in->name && std::strcmp(in->name, "Mine") == 0) ++mine;
    if (in->mech == 5 && in->name && std::strcmp(in->name, "Other") == 0) ++other;
    delete in;
  }
  EXPECT(got == 4 && mine == 3 && other == 1);
  // One taken before (its .done there): the same file imported again gets a free name, and is read again.
  const mech3::ModAddDone again = mech3::mod_add_run(wd, one, "", nullptr, nullptr);
  EXPECT(again.loadouts == 1 && fs::exists(wd + "/import/" + name + " (4).m3c", ec) && fs::exists(wd + "/import/" + name + ".m3c.done", ec));
  mech3::Config* in = mech3::lab_import_config();
  EXPECT(in && in->mech == 3);
  delete in;
  zutl::mounts() = saved;
  fs::remove_all(root, ec);
}

#if defined(_WIN32)
#include <cstdlib>
#include <filesystem>
#include <fstream>
// Names on Windows (docs/program.md): the manifest's UTF-8 in the narrow calls,
// and what an earlier build left in the system's code page (Windows-1252 here).
TEST(windows_names_are_utf8_and_old_ones_put_right) {
  namespace fs = std::filesystem;
  EXPECT(zutl::narrow_paths_utf8());
  const fs::path dir = fs::path(MW3_TEST_TMP) / L"test_win_names \u00e9 \u0416 it's";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir / "pilots", ec);
  EXPECT(!ec);
  // A narrow name reaches the disk as the letters it spells.
  const std::string narrow = dir.string() + "\\caf\xc3\xa9 \xd0\x96.txt";
  std::FILE* f = std::fopen(narrow.c_str(), "wb");
  EXPECT(f != nullptr);
  if (f) std::fclose(f);
  EXPECT(fs::exists(dir / L"caf\u00e9 \u0416.txt", ec));

  EXPECT(zutl::path_from_config("D:\\Caf\xe9") == "D:\\Caf\xc3\xa9");
  EXPECT(zutl::path_from_config("D:\\Caf\xc3\xa9") == "D:\\Caf\xc3\xa9");
  EXPECT(zutl::path_from_config("D:\\Cafe") == "D:\\Cafe");

  std::ofstream(dir / "pilots" / L"Jos\u00c3\u00a9.mw3") << "old";      // "José", as an earlier build named it
  std::ofstream(dir / "pilots" / L"Zo\u00eb.mw3") << "plain";           // a real name stays
  std::ofstream(dir / "pilots" / L"R\u00c3\u00a9mi.mw3") << "old";
  std::ofstream(dir / "pilots" / L"R\u00e9mi.mw3") << "new";            // never over a file
  zutl::repair_old_names(dir.string());
  EXPECT(fs::exists(dir / "pilots" / L"Jos\u00e9.mw3", ec));
  EXPECT(!fs::exists(dir / "pilots" / L"Jos\u00c3\u00a9.mw3", ec));
  EXPECT(fs::exists(dir / "pilots" / L"Zo\u00eb.mw3", ec));
  EXPECT(fs::exists(dir / "pilots" / L"R\u00c3\u00a9mi.mw3", ec));
  std::ifstream kept(dir / "pilots" / L"R\u00e9mi.mw3");
  std::string line;
  std::getline(kept, line);
  EXPECT(line == "new");
  // A junction inside pilots is left, and what it leads to is not ours.
  const fs::path outside = dir / "outside", second = dir.string() + "b";
  fs::create_directories(outside, ec);
  fs::create_directories(second / "pilots", ec);
  std::ofstream(outside / L"Jos\u00c3\u00a9.mw3") << "not ours";
  const std::string make = "mklink /J \"" + (second / "pilots" / "link").string() + "\" \"" + outside.string() + "\" >nul";
  EXPECT(std::system(make.c_str()) == 0);
  zutl::repair_old_names(second.string());
  EXPECT(fs::exists(outside / L"Jos\u00c3\u00a9.mw3", ec));
  EXPECT(!fs::exists(outside / L"Jos\u00e9.mw3", ec));
  fs::remove(second / "pilots" / "link", ec);  // the junction itself, not what it leads to
  EXPECT(fs::exists(outside / L"Jos\u00c3\u00a9.mw3", ec));
  fs::remove_all(second, ec);
  // Once: a name of that shape made afterwards is the player's.
  std::ofstream(dir / "pilots" / L"Ana\u00c3\u00a9.mw3") << "later";
  zutl::repair_old_names(dir.string());
  EXPECT(fs::exists(dir / "pilots" / L"Ana\u00c3\u00a9.mw3", ec));
  fs::remove_all(dir, ec);
}
#endif
