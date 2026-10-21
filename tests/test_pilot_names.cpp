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
    zfix::options().bugs = fix;
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
  zfix::options().bugs = true;
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
    zfix::options().bugs = fix;
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
  zfix::options().bugs = true;
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
    zfix::options().bugs = fix;
    EXPECT(mech3::chassis_before(e, 0, 3, 1) == fix);   // Avatar (3) before Zeus (1) by name
    EXPECT(mech3::chassis_before(e, 0, 1, 3) == !fix);  // the exe: 1 before 3
    EXPECT(!mech3::chassis_before(e, 2, 1, 3));         // no other order
  }
  zfix::options().bugs = true;
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
  EXPECT(mech3::lab_export_config(c));
  const std::string out = zutl::write_path((std::string("export\\") + mech3::mech_name(3) + "_Mine.m3c").c_str());
  std::FILE* f = std::fopen(out.c_str(), "rb");
  EXPECT(f != nullptr);
  std::string bytes;
  for (int ch; f && (ch = std::fgetc(f)) != EOF;) bytes += static_cast<char>(ch);
  if (f) std::fclose(f);
  std::FILE* g = zutl::open("import\\mine.m3c", "wb");
  EXPECT(g != nullptr);
  if (g) {
    std::fwrite(bytes.data(), 1, bytes.size(), g);
    std::fclose(g);
  }
  mech3::Config* in = mech3::lab_import_config();
  EXPECT(in && in->mech == 3 && in->name && std::strcmp(in->name, "Mine") == 0);
  delete in;
  EXPECT(mech3::lab_import_config() == nullptr);  // read once: renamed .done
  EXPECT(zutl::exists("import\\mine.m3c.done"));
}
