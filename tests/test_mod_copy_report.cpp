// Mods by campaign: REPORT on a copy (upgrades/mods/mod_report, mod_copy_changes): what a campaign saved
// from another changes from it, over the base= chain and then the game's own files. Built folders.
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.h"
#include "upgrades/mods/mod_report.h"

namespace {
namespace fs = std::filesystem;
fs::path tmp(const char* name) {
  const std::string d = std::string(MW3_TEST_TMP) + "/" + name;
  std::error_code ec;
  fs::remove_all(d, ec);
  fs::create_directories(d, ec);
  return d;
}
void put(const fs::path& p, const std::string& text) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  std::ofstream(p, std::ios::binary) << text;
}
using V = std::vector<std::string>;
std::string joined(const V& v) {
  std::string s;
  for (const std::string& l : v) s += l + "\n";
  return s;
}
const char* kNight = "name=Long Night\nop=Op 1\nmission=c1 1\n";
// camp (a campaign mod with a weapon and a mech) <- copy1 (saved from camp~night) <- copy2 (saved from copy1's), an
// install with two files, and a copy of the game's own campaign.
struct Layout {
  fs::path wd, install;
};
Layout layout(const char* name) {
  Layout l;
  l.wd = tmp(name), l.install = l.wd / "install";
  put(l.install / "zbd/c1/anim.zbd", "the game's anim");
  put(l.install / "zbd/reader.zbd", "the game's reader");
  put(l.wd / "mods/camp/mod.txt", "name=Camp\ncampaign=night.txt\nweapon=GAUSS2 ammo=yes tech=sphere\nmech=Thor B tons=70\n");
  put(l.wd / "mods/camp/night.txt", kNight);
  put(l.wd / "mods/camp/zbd/w1/gamez.zbd", "camp's world");
  put(l.wd / "mods/camp/zbd/w1/anim.zbd", "camp's anim");
  put(l.wd / "mods/copy1/mod.txt",
      "name=My Night\nbase=camp~night\ncampaign=night.txt\nweapon=GAUSS2 ammo=yes tech=clan\nweapon=PPC9 tech=sphere\nmech=Thor B tons=70\n");
  put(l.wd / "mods/copy1/night.txt", kNight);                         // the same as camp's
  put(l.wd / "mods/copy1/zbd/w1/GameZ.zbd", "my world");             // camp's, changed (another letter case)
  put(l.wd / "mods/copy1/zbd/c1/anim.zbd", "my c1 anim");            // the game's own
  put(l.wd / "mods/copy1/zbd/w1/nets/extra.zrd", "a new net");       // nobody's
  put(l.wd / "mods/copy1/mw3-mod-added.txt", "ours");                // a marker: not a file of the mod
  put(l.wd / "mods/copy2/mod.txt", "name=My Night 2\nbase=copy1~night\ncampaign=night.txt\n");
  put(l.wd / "mods/copy2/night.txt", "name=Long Night\nop=Op 1\nmission=c1 2\n");  // copy1's, changed
  put(l.wd / "mods/copy2/zbd/w1/anim.zbd", "camp's anim");           // two down, the same
  put(l.wd / "mods/copy2/zbd/w1/gamez.zbd", "my world again");       // copy1's, changed
  put(l.wd / "mods/own/mod.txt", "name=My MechWarrior\nbase=mw3\n");
  put(l.wd / "mods/own/zbd/reader.zbd", "my reader");
  put(l.wd / "mods/own/zbd/c9/new.zbd", "new");
  put(l.wd / "mods/plain/mod.txt", "name=Plain\nweapon=LBX tech=clan\n");
  return l;
}
}  // namespace

TEST(mod_copy_report_says_what_a_copy_changes_from_its_base) {
  const Layout l = layout("copy_report");
  const mech3::CopyChanges c = mech3::mod_copy_changes(l.wd.string(), l.install.string(), "copy1");
  EXPECT(c.is_copy && c.base == "camp~night" && c.base_name == "Long Night" && c.missing.empty());
  EXPECT((c.under == V{"camp"}));
  EXPECT((c.replaced == V{"zbd/w1/GameZ.zbd"}));       // camp's file, other bytes, any letter case
  EXPECT((c.replaced_game == V{"zbd/c1/anim.zbd"}));   // only the game has it
  EXPECT((c.added == V{"zbd/w1/nets/extra.zrd"}));
  EXPECT((c.same == V{"night.txt"}));                  // mod.txt and our marker are not its files
  EXPECT((c.rows_changed == V{"weapon GAUSS2"}));      // tech=sphere to tech=clan; the mech and the campaign as they were
  EXPECT((c.rows_added == V{"weapon PPC9"}));
  const std::string said = joined(mech3::mod_copy_sentences(c));
  std::printf("%s", said.c_str());
  EXPECT(said ==
         "It is a copy saved from the campaign \"Long Night\".\n"
         "It replaces 1 file of that campaign: zbd/w1/GameZ.zbd.\n"
         "It replaces 1 of the game's own files: zbd/c1/anim.zbd.\n"
         "It adds 1 new file: zbd/w1/nets/extra.zrd.\n"
         "It changes the weapon GAUSS2.\n"
         "It adds the weapon PPC9.\n"
         "1 of its files is the same as that campaign's, so it changes nothing.\n");
  // A copy of a copy: the nearest mod under it that has the file is the one compared.
  const mech3::CopyChanges d = mech3::mod_copy_changes(l.wd.string(), l.install.string(), "copy2");
  EXPECT(d.is_copy && d.base == "copy1~night" && (d.under == V{"copy1", "camp"}));
  EXPECT((d.replaced == V{"night.txt", "zbd/w1/gamez.zbd"}) && (d.same == V{"zbd/w1/anim.zbd"}));
  EXPECT(d.replaced_game.empty() && d.added.empty() && d.rows_changed.empty() && d.rows_added.empty());
  // A copy of the game's own campaign: nothing under it but the game.
  const mech3::CopyChanges o = mech3::mod_copy_changes(l.wd.string(), l.install.string(), "own");
  EXPECT(o.is_copy && o.base == "mw3" && o.base_name == "MechWarrior 3" && o.under.empty() && o.missing.empty());
  EXPECT((o.replaced_game == V{"zbd/reader.zbd"}) && (o.added == V{"zbd/c9/new.zbd"}) && o.replaced.empty());
  // With no install given the game's files are not looked at: such a file counts as added.
  const mech3::CopyChanges n = mech3::mod_copy_changes(l.wd.string(), "", "own");
  EXPECT(n.replaced_game.empty() && (n.added == V{"zbd/c9/new.zbd", "zbd/reader.zbd"}));
  // Not a copy, or no such mod: nothing said.
  EXPECT(!mech3::mod_copy_changes(l.wd.string(), l.install.string(), "plain").is_copy);
  EXPECT(mech3::mod_copy_report(l.wd.string(), l.install.string(), "plain").empty());
  EXPECT(mech3::mod_copy_report(l.wd.string(), l.install.string(), "nosuch").empty());
}

TEST(mod_copy_report_says_when_its_base_is_gone_or_nothing_changed) {
  const Layout l = layout("copy_report_gone");
  std::error_code ec;
  // Nothing changed: the same files and rows as its base.
  put(l.wd / "mods/twin/mod.txt", "name=Twin\nbase=camp~night\ncampaign=night.txt\nweapon=GAUSS2 ammo=yes tech=sphere\n");
  put(l.wd / "mods/twin/night.txt", kNight);
  const std::string twin = joined(mech3::mod_copy_report(l.wd.string(), l.install.string(), "twin"));
  EXPECT(twin == "It is a copy saved from the campaign \"Long Night\".\n"
                 "1 of its files is the same as that campaign's, so it changes nothing.\n"
                 "It changes nothing from the campaign it was saved from.\n");
  // The base's mod removed from the mods folder (the test's own folder): said, and what is left is compared with the game.
  fs::remove_all(l.wd / "mods/camp", ec);
  const mech3::CopyChanges c = mech3::mod_copy_changes(l.wd.string(), l.install.string(), "copy1");
  EXPECT(c.is_copy && c.missing == "camp~night" && c.under.empty());
  const std::string said = joined(mech3::mod_copy_sentences(c));
  std::printf("%s", said.c_str());
  EXPECT(said.find("The campaign it was saved from is not in the mods folder, so what it changed there cannot be told.") != std::string::npos);
  EXPECT(said.find("It changes nothing") == std::string::npos);
  // What nothing above the gap has may have been that campaign's: neither "adds" nor the game's files are claimed.
  EXPECT(said == "It is a copy saved from the campaign camp~night.\n"
                 "The campaign it was saved from is not in the mods folder, so what it changed there cannot be told.\n");
  EXPECT((c.added.size() == 3) && (c.replaced_game == V{"zbd/c1/anim.zbd"}));  // the lists are still filled, for a caller that wants them
  // Two down: copy2 still has copy1 under it, and says the one below that is gone.
  const mech3::CopyChanges d = mech3::mod_copy_changes(l.wd.string(), l.install.string(), "copy2");
  EXPECT((d.under == V{"copy1"}) && d.missing == "camp~night");
  const std::string two = joined(mech3::mod_copy_sentences(d));
  EXPECT(two.find("The campaign \"camp~night\" under it is not in the mods folder") != std::string::npos);
  EXPECT(two.find("It replaces 2 files of that campaign: night.txt and zbd/w1/gamez.zbd.") != std::string::npos && two.find("It adds") == std::string::npos);
  // A chain that comes round to itself ends.
  put(l.wd / "mods/loop/mod.txt", "name=Loop\nbase=loop\n");
  EXPECT(mech3::mod_copy_changes(l.wd.string(), l.install.string(), "loop").under.empty());
}
