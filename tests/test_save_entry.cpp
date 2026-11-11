// Mods by campaign: a saved game belongs to its entry (upgrades/mods/save_mods, docs/mods-campaigns-plan.md).
// A game saved inside a campaign entry holds "Mods/entry" (the entry, what it is built on, its ticks) in place
// of the mod list; the load says when it was saved in another campaign or the entry's ticks or files changed.
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zsave/zsave.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/mods.h"
#include "upgrades/mods/campaign_entries.h"
#include "upgrades/mods/save_mods.h"

namespace {
namespace fs = std::filesystem;
std::string tmp(const char* name) {
  const std::string d = std::string(MW3_TEST_TMP) + "/" + name;
  std::error_code ec;
  fs::remove_all(d, ec);
  fs::create_directories(d, ec);
  return d;
}
void put(const std::string& p, const std::string& text) {
  std::error_code ec;
  fs::create_directories(fs::path(p).parent_path(), ec);
  std::ofstream(p, std::ios::binary) << text;
}
bool has(const std::string& path, const char* entry) {
  zutl_zar::Archive a;
  zutl_zar::init(a);
  const bool ok = zutl_zar::open(a, path.c_str()) && zutl_zar::find(a, entry) >= 0;
  zutl_zar::close(a);
  zutl_zar::destroy(a);
  return ok;
}
// Asset mods that change gameplay (weps, guns) and one that does not (tex); a campaign mod and a copy of it.
void mods(const std::string& wd) {
  put(wd + "/mods/weps/mod.txt", "name=Freako Weapons\n");
  put(wd + "/mods/weps/zbd/c1/gamez.zbd", "weps");
  put(wd + "/mods/guns/mod.txt", "name=Big Guns\n");
  put(wd + "/mods/guns/zbd/c2/gamez.zbd", "guns");
  put(wd + "/mods/tex/mod.txt", "name=Textures\nkind=assets\n");
  put(wd + "/mods/tex/zbd/rmechtex.zbd", "pixels");
  put(wd + "/mods/camp/mod.txt", "name=Camp\ncampaign=night.txt\n");
  put(wd + "/mods/camp/night.txt", "name=Long Night\nop=Op 1\nmission=c1 1\n");
  put(wd + "/mods/camp/zbd/w1/gamez.zbd", "camp's world");
  put(wd + "/mods/copy/mod.txt", "name=My Night\nbase=camp~night\ncampaign=night.txt\n");
  put(wd + "/mods/copy/night.txt", "name=My Night\nop=Op 1\nmission=c1 1\n");
  put(wd + "/mods/copy/zbd/w1/gamez.zbd", "my world");
}
// The running entry and the mods' identity put back after (other tests read them).
struct Keep {
  mech3::SaveEntry entry = mech3::save_entry();
  mech3::ModsIdentity id = mech3::mods_identity();
  bool had = zsave::manager() != nullptr;
  Keep() {
    if (!had) zsave::init();
    mech3::save_mods_register();
  }
  ~Keep() {
    if (!had) zsave::shutdown();
    mech3::save_entry() = entry, mech3::mods_identity() = id;
  }
};
}  // namespace

TEST(save_entry_identity_from_the_entry_its_bases_and_ticks) {
  const std::string wd = tmp("save_entry_identity");
  mods(wd);
  Keep keep;
  const std::vector<mech3::CampaignEntry> all = mech3::campaign_entries(wd);
  const mech3::CampaignEntry* own = mech3::find_entry(all, "mw3");
  const mech3::CampaignEntry* copy = mech3::find_entry(all, "copy~night");
  EXPECT(own && copy);
  if (!own || !copy) return;
  // The game's own with no tick: nothing of gameplay mounted.
  mech3::save_entry_set(wd, *own);
  EXPECT(mech3::save_entry().id == "mw3" && mech3::save_entry().ticks_hash == 0 && mech3::save_entry().mounted_hash == 0);
  // Ticks: the gameplay ones named, in order; the looks-only one changes nothing.
  EXPECT(mech3::entry_save_ticks(wd, *own, {"weps", "tex", "guns"}));
  mech3::save_entry_set(wd, *own);
  const mech3::SaveEntry a = mech3::save_entry();
  EXPECT(a.name == "MechWarrior 3" && a.bases.empty() && a.ticks_hash != 0 && a.mounted_hash == a.ticks_hash);
  EXPECT((a.ticks == std::vector<std::string>{"Freako Weapons", "Big Guns"}));
  EXPECT(mech3::entry_save_ticks(wd, *own, {"weps", "guns"}));
  mech3::save_entry_set(wd, *own);
  EXPECT(mech3::save_entry().ticks_hash == a.ticks_hash);  // tex off: the same
  EXPECT(mech3::entry_save_ticks(wd, *own, {"weps"}));
  mech3::save_entry_set(wd, *own);
  EXPECT(mech3::save_entry().ticks_hash != a.ticks_hash && mech3::save_entry().ticks_hash != 0);
  // A copy: what it is built on, nearest first; its own files count in what it mounts, not in its ticks.
  mech3::save_entry_set(wd, *copy);
  const mech3::SaveEntry c = mech3::save_entry();
  EXPECT(c.id == "copy~night" && (c.bases == std::vector<std::string>{"camp~night"}) && c.ticks_hash == 0 && c.ticks.empty() && c.mounted_hash != 0);
  // The text read back; a line it does not know passed over; none read as none.
  const mech3::SaveEntry r = mech3::save_entry_parse(mech3::save_entry_text(a) + "later 1\r\n");
  EXPECT(r.id == a.id && r.name == a.name && r.bases == a.bases && r.ticks == a.ticks && r.ticks_hash == a.ticks_hash && r.mounted_hash == a.mounted_hash);
  const mech3::SaveEntry rc = mech3::save_entry_parse(mech3::save_entry_text(c));
  EXPECT(rc.bases == c.bases && rc.mounted_hash == c.mounted_hash && rc.ticks_hash == 0);
  EXPECT(mech3::save_entry_parse("junk\n").id.empty());
  mech3::save_entry_clear();
  EXPECT(mech3::save_entry().id.empty() && mech3::save_entry().mounted_hash == 0);
}

TEST(save_entry_saved_game_says_when_its_campaign_or_ticks_changed) {
  const std::string wd = tmp("save_entry_note");
  mods(wd);
  Keep keep;
  mech3::mods_identity() = mech3::ModsIdentity{};
  const std::vector<mech3::CampaignEntry> all = mech3::campaign_entries(wd);
  const mech3::CampaignEntry* own = mech3::find_entry(all, "mw3");
  const mech3::CampaignEntry* camp = mech3::find_entry(all, "camp~night");
  const mech3::CampaignEntry* copy = mech3::find_entry(all, "copy~night");
  EXPECT(own && camp && copy);
  if (!own || !camp || !copy) return;
  // The running entry with these ticks ("": a plain start).
  const auto in = [&](const mech3::CampaignEntry* e, const std::vector<std::string>& ticks) {
    if (!e) return mech3::save_entry_clear();
    EXPECT(mech3::entry_save_ticks(wd, *e, ticks));
    mech3::save_entry_set(wd, *e);
  };
  const auto saved = [&](const mech3::CampaignEntry* e, const std::vector<std::string>& ticks, const char* name) {
    in(e, ticks);
    const std::string path = wd + "/" + name;
    EXPECT(zsave::save(path.c_str(), 2));
    return path;
  };
  const auto loaded = [&](const std::string& path, const mech3::CampaignEntry* e, const std::vector<std::string>& ticks) {
    in(e, ticks);
    mech3::save_mods_begin_load();
    EXPECT(zsave::load(path.c_str(), 2));
    return mech3::save_mods_note();
  };
  const std::string plain = saved(nullptr, {}, "plain.mw3"), bare = saved(own, {}, "bare.mw3"), looks = saved(own, {"tex"}, "looks.mw3");
  const std::string weps = saved(own, {"weps"}, "weps.mw3"), night = saved(camp, {}, "night.mw3");
  // Nothing of gameplay: the original's file, in an entry or not. With it: the entry, and no mod list.
  EXPECT(!has(plain, "Mods/entry") && !has(bare, "Mods/entry") && !has(looks, "Mods/entry"));
  EXPECT(!has(plain, "Mods/identity") && !has(weps, "Mods/identity"));
  EXPECT(has(weps, "Mods/entry") && has(night, "Mods/entry"));
  // The same entry and ticks: nothing said; a looks-only tick added or dropped: nothing said.
  EXPECT(loaded(weps, own, {"weps"}).empty() && loaded(weps, own, {"weps", "tex"}).empty());
  EXPECT(loaded(plain, nullptr, {}).empty() && loaded(bare, own, {}).empty() && loaded(plain, own, {"tex"}).empty());
  EXPECT(loaded(night, camp, {}).empty());
  const std::string a = loaded(weps, own, {"weps", "guns"}), b = loaded(weps, own, {}), c = loaded(weps, camp, {});
  const std::string d = loaded(plain, own, {"weps"}), e = loaded(weps, nullptr, {}), f = loaded(plain, camp, {});
  std::printf("  %s\n  %s\n  %s\n  %s\n  %s\n  %s\n", a.c_str(), b.c_str(), c.c_str(), d.c_str(), e.c_str(), f.c_str());
  EXPECT(a == "This campaign's mods changed since the save: then Freako Weapons; now Freako Weapons, Big Guns");
  EXPECT(b == "Saved in the campaign MechWarrior 3 with its mods; none now");  // unticked: the entry mounts nothing now
  EXPECT(c == "Saved in another campaign (MechWarrior 3); this is Long Night");
  EXPECT(d == "Saved without mods; the campaign MechWarrior 3 now has: Freako Weapons");  // a pilot copied over
  EXPECT(e == "Saved in the campaign MechWarrior 3 with its mods; none now");
  EXPECT(f == "Saved without mods; the campaign Long Night now has: its own files");
  EXPECT(mech3::save_mods_note().empty());  // said once
  // The campaign's own files changed since the save (the same ticks): said.
  put(wd + "/mods/camp/zbd/w1/gamez.zbd", "camp's world, edited");
  EXPECT(loaded(night, camp, {}) == "This campaign's own files changed since the save");
  // A tick's own files changed since: its mods changed.
  put(wd + "/mods/weps/zbd/c1/gamez.zbd", "weps 2");
  EXPECT(loaded(weps, own, {"weps"}) == "A mod of this campaign changed since the save (Freako Weapons)");
  // Outside an entry the mod list is as before (--mod, a developer's start).
  mech3::save_entry_clear();
  mech3::mods_identity_set(wd, {"guns"});
  const std::string old = wd + "/old.mw3";
  EXPECT(zsave::save(old.c_str(), 2) && has(old, "Mods/identity") && !has(old, "Mods/entry"));
  mech3::mods_identity() = mech3::ModsIdentity{};
  EXPECT(loaded(old, own, {"weps"}) == "Saved with mods: Big Guns; now in the campaign MechWarrior 3");
}
