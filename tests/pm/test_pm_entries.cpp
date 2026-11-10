// Mods by campaign on Pirate's Moon (upgrades/mods/campaign_entries,
// docs/mods-campaigns-plan.md): its two campaigns and Instant Action are the
// game's own entries, each with its own folder, pilots and ticks; a mod over
// the game's files plays on the Campaign; the first start's move gives the
// mods that were on to both campaigns. MechWarrior 3's: tests/test_campaign_entries.cpp.
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/mod_campaign.h"
#include "mech3/mods.h"
#include "gamez/zsave/zsave.h"
#include "gamez/zutil/zutl_zar.h"
#include "upgrades/mods/campaign_entries.h"
#include "upgrades/mods/entry_pilot_copy.h"
#include "upgrades/mods/save_mods.h"

namespace {
namespace fs = std::filesystem;
fs::path tmp(const char* name) {
  const std::string d = std::string(MW3_TEST_TMP_ROOT) + "/" + name;
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
std::string text_of(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
std::string ids(const std::vector<mech3::CampaignEntry>& all) {
  std::string s;
  for (const auto& e : all) s += (s.empty() ? "" : ",") + e.id + (e.missing ? "!" : "");
  return s;
}
// The mods' game for the test, put back after (other tests read it).
struct PmMods {
  std::string was = mech3::mods_game();
  PmMods() { mech3::mods_game() = "mw3pm"; }
  ~PmMods() { mech3::mods_game() = was; }
};
// An asset mod, a campaign mod, a mod over the game's own c1, one made for MechWarrior 3.
void mods(const fs::path& wd) {
  put(wd / "mods/weps/mod.txt", "name=Weps\nfor=mw3pm\nweapon=GAUSS2 ammo=yes tech=sphere\n");
  put(wd / "mods/camp/mod.txt", "name=Camp\ncampaign=night.txt\n");
  put(wd / "mods/camp/night.txt", "name=Long Night\nop=Op 1\nmission=c1 1\n");
  put(wd / "mods/rewrite/mod.txt", "name=Moon Rewrite\n");
  put(wd / "mods/rewrite/zbd/c1/anim.zbd", "x");
  put(wd / "mods/other/mod.txt", "name=Other Game\nfor=mw3\ncampaign=x.txt\n");
  put(wd / "mods/other/x.txt", "name=X\nop=Op 1\nmission=c1 1\n");
}
}  // namespace

TEST(pm_entries_are_the_two_campaigns_and_instant_action) {
  PmMods game;
  const std::vector<mech3::CampaignEntry> own = mech3::official_entries();
  EXPECT(ids(own) == "campaign,pirate,ia");
  EXPECT(own.size() == 3 && own[0].kind == mech3::CampaignEntry::kOfficial && own[0].name == "Campaign" &&
         own[1].kind == mech3::CampaignEntry::kOfficial && own[1].name == "Pirate Missions" &&
         own[2].kind == mech3::CampaignEntry::kInstantAction && own[2].name == "Instant Action");
  for (const auto& e : own) EXPECT(e.mod.empty() && e.campaign.empty() && e.base.empty() && e.error.empty() && !e.missing);
  const fs::path wd = tmp("pm_entries_listed");
  mods(wd);
  const std::vector<mech3::CampaignEntry> all = mech3::campaign_entries(wd.string());
  std::printf("  entries: %s\n", ids(all).c_str());
  EXPECT(ids(all) == "campaign,pirate,ia,camp~night,rewrite");  // the other game's mod left out
  const mech3::CampaignEntry* rewrite = mech3::find_entry(all, "rewrite");
  EXPECT(rewrite && rewrite->base == "campaign" && rewrite->campaign.empty());  // over the game's own files: on the Campaign
  EXPECT(!mech3::find_entry(all, "mw3"));  // MechWarrior 3's entry is not one of Pirate's Moon's
}

TEST(pm_entries_each_keep_their_own_folder_pilots_and_ticks) {
  PmMods game;
  const fs::path wd = tmp("pm_entries_own");
  mods(wd);
  const std::vector<mech3::CampaignEntry> all = mech3::campaign_entries(wd.string());
  const mech3::CampaignEntry* campaign = mech3::find_entry(all, "campaign");
  const mech3::CampaignEntry* pirate = mech3::find_entry(all, "pirate");
  const mech3::CampaignEntry* ia = mech3::find_entry(all, "ia");
  EXPECT(campaign && pirate && ia);
  if (!campaign || !pirate || !ia) return;
  EXPECT(mech3::entry_pilots_folder("campaign") == "campaigns\\campaign\\pilots" &&
         mech3::entry_pilots_folder("pirate") == "campaigns\\pirate\\pilots" && mech3::entry_pilots_folder("ia") == "campaigns\\ia\\pilots");
  std::string why;
  for (const mech3::CampaignEntry* e : {campaign, pirate, ia}) {
    EXPECT(mech3::entry_ensure(wd.string(), *e, &why));
    EXPECT(fs::is_regular_file(wd / "campaigns" / e->id / mech3::kCampaignMarker) && fs::is_directory(wd / "campaigns" / e->id / "pilots/games"));
  }
  // Ticks: one campaign's are not the other's, nor Instant Action's.
  EXPECT(mech3::entry_save_ticks(wd.string(), *campaign, {"weps"}));
  EXPECT((mech3::entry_ticks(wd.string(), "campaign") == std::vector<std::string>{"weps"}));
  EXPECT(mech3::entry_ticks(wd.string(), "pirate").empty() && mech3::entry_ticks(wd.string(), "ia").empty());
  EXPECT((mech3::entry_mount_list(wd.string(), *campaign) == std::vector<std::string>{"weps"}));
  EXPECT(mech3::entry_mount_list(wd.string(), *pirate).empty() && mech3::entry_mount_list(wd.string(), *ia).empty());
  EXPECT(mech3::entry_save_ticks(wd.string(), *pirate, {"weps", "gone"}));
  EXPECT((mech3::entry_missing_mods(wd.string(), *pirate) == std::vector<std::string>{"gone"}) && mech3::entry_missing_mods(wd.string(), *campaign).empty());
  // A pilot in one is in that one's folder only.
  put(wd / "campaigns/pirate/pilots/Rook.mw3", "rook");
  EXPECT(!fs::exists(wd / "campaigns/campaign/pilots/Rook.mw3") && !fs::exists(wd / "pilots/Rook.mw3"));
  // A folder the game did not make under an entry's name is never taken.
  const fs::path other = tmp("pm_entries_stranger");
  put(other / "campaigns/pirate/theirs.txt", "x");
  EXPECT(!mech3::entry_ensure(other.string(), *pirate, &why) && !fs::exists(other / "campaigns/pirate" / mech3::kCampaignMarker));
  EXPECT(text_of(other / "campaigns/pirate/theirs.txt") == "x");
}

// The first start of this build on Pirate's Moon: the mods mw3.cfg had on
// were on for both campaigns, so both take them (Instant Action none);
// mw3.cfg keeps every byte and gets mods_moved=1, once.
TEST(pm_entries_first_start_gives_both_campaigns_the_mods) {
  PmMods game;
  const fs::path wd = tmp("pm_entries_migrate");
  mods(wd);
  const std::string cfg = "install=D:\\PM\r\nmod=weps\r\nmod=camp";
  put(wd / "mw3.cfg", cfg);
  const std::vector<std::string> log = mech3::entries_migrate(wd.string());
  for (const std::string& l : log) std::printf("  %s\n", l.c_str());
  EXPECT((mech3::entry_ticks(wd.string(), "campaign") == std::vector<std::string>{"weps"}));
  EXPECT((mech3::entry_ticks(wd.string(), "pirate") == std::vector<std::string>{"weps"}));
  EXPECT((mech3::entry_ticks(wd.string(), "camp~night") == std::vector<std::string>{"weps"}));
  EXPECT(mech3::entry_ticks(wd.string(), "ia").empty() && !fs::exists(wd / "campaigns/ia"));
  EXPECT(!fs::exists(wd / "campaigns/mw3"));
  EXPECT(text_of(wd / "mw3.cfg") == cfg + "\r\nmods_moved=1\r\n");
  EXPECT(mech3::entries_migrate(wd.string()).empty());
}

// Each of Pirate's Moon's entries has its own saved loadouts folder beside its pilots (entry_configs_folder),
// made with the entry, and a pilot copied into one (entry_copy_pilot) takes Pirate's Moon's saved games with it:
// games\<name>MM.mw3, the mission in two digits (PM 0x00435680), not MechWarrior 3's four.
TEST(pm_entries_have_their_own_loadouts_and_take_a_pilots_saves) {
  PmMods game;
  const fs::path wd = tmp("pm_entries_copy");
  EXPECT(mech3::entry_configs_folder("campaign") == "campaigns\\campaign\\configs" &&
         mech3::entry_configs_folder("pirate") == "campaigns\\pirate\\configs" && mech3::entry_configs_folder("ia") == "campaigns\\ia\\configs");
  const std::vector<mech3::CampaignEntry> all = mech3::campaign_entries(wd.string());
  std::string why;
  for (const char* id : {"campaign", "pirate", "ia"}) {
    const mech3::CampaignEntry* e = mech3::find_entry(all, id);
    EXPECT(e && mech3::entry_ensure(wd.string(), *e, &why));
    EXPECT(fs::is_directory(wd / "campaigns" / id / "configs") && fs::is_directory(wd / "campaigns" / id / "pilots/games"));
  }
  // The main menu's pilot Bob, with Pirate's Moon's saves; Bob2's and a four-digit file are not Bob's.
  put(wd / "pilots/Bob.mw3", "bob");
  put(wd / "pilots/games/Bob02.mw3", "bob mission 2");
  put(wd / "pilots/games/Bob10.mw3", "bob mission 10");
  put(wd / "pilots/games/Bob10.mw3.rows", "rows");
  put(wd / "pilots/Bob2.mw3", "bob2");
  put(wd / "pilots/games/Bob202.mw3", "bob2 mission 2");
  put(wd / "pilots/games/Bob0102.mw3", "not Pirate's Moon's name for a save");
  put(wd / "configs/Annihilator.mw3", "loadouts");
  const std::string entry = (wd / "campaigns/pirate").string(), configs = (wd / "campaigns/pirate/configs").string();
  const mech3::PilotCopy r = mech3::entry_copy_pilot(wd.string(), "Bob", entry, configs);
  std::printf("  copied %zu: %s\n", r.copied.size(), r.why.c_str());
  EXPECT(r.ok && r.copied.size() == 5);  // the pilot, two saves, one rows file, one loadout
  EXPECT(text_of(wd / "campaigns/pirate/pilots/Bob.mw3") == "bob" && text_of(wd / "campaigns/pirate/pilots/games/Bob02.mw3") == "bob mission 2" &&
         text_of(wd / "campaigns/pirate/pilots/games/Bob10.mw3") == "bob mission 10" &&
         text_of(wd / "campaigns/pirate/pilots/games/Bob10.mw3.rows") == "rows" && text_of(wd / "campaigns/pirate/configs/Annihilator.mw3") == "loadouts");
  EXPECT(!fs::exists(wd / "campaigns/pirate/pilots/games/Bob202.mw3") && !fs::exists(wd / "campaigns/pirate/pilots/games/Bob0102.mw3"));
  // The other entries got nothing of it, and the originals are as they were.
  EXPECT(fs::is_empty(wd / "campaigns/campaign/pilots/games") && fs::is_empty(wd / "campaigns/campaign/configs") && fs::is_empty(wd / "campaigns/ia/configs"));
  EXPECT(text_of(wd / "pilots/Bob.mw3") == "bob" && text_of(wd / "pilots/games/Bob02.mw3") == "bob mission 2");
  // Again: a save of Bob's already there refuses it.
  fs::remove(wd / "campaigns/pirate/pilots/Bob.mw3");
  const mech3::PilotCopy again = mech3::entry_copy_pilot(wd.string(), "Bob", entry, configs);
  EXPECT(!again.ok && again.why.find("saved game") != std::string::npos);
}

// A saved game belongs to its entry on Pirate's Moon too (upgrades/mods/save_mods): saved in the Campaign with
// a gameplay tick it holds "Mods/entry"; loaded in the Pirate Missions it says so; the same entry and ticks
// nothing; the game's own entries with no gameplay tick write the original's save.
TEST(pm_entries_saved_game_names_its_campaign) {
  PmMods game;
  const fs::path wd = tmp("pm_entries_save");
  put(wd / "mods/weps/mod.txt", "name=Weps\nfor=mw3pm\n");
  put(wd / "mods/weps/zbd/c1/gamez.zbd", "weps");
  const mech3::SaveEntry keep_entry = mech3::save_entry();
  const mech3::ModsIdentity keep_id = mech3::mods_identity();
  mech3::mods_identity() = mech3::ModsIdentity{};
  const bool had = zsave::manager() != nullptr;
  if (!had) zsave::init();
  mech3::save_mods_register();
  const std::vector<mech3::CampaignEntry> all = mech3::campaign_entries(wd.string());
  const mech3::CampaignEntry* campaign = mech3::find_entry(all, "campaign");
  const mech3::CampaignEntry* pirate = mech3::find_entry(all, "pirate");
  EXPECT(campaign && pirate);
  if (campaign && pirate) {
    auto in = [&](const mech3::CampaignEntry& e, const std::vector<std::string>& ticks) {
      EXPECT(mech3::entry_save_ticks(wd.string(), e, ticks));
      mech3::save_entry_set(wd.string(), e);
    };
    auto has = [](const std::string& path, const char* entry) {
      zutl_zar::Archive a;
      zutl_zar::init(a);
      const bool ok = zutl_zar::open(a, path.c_str()) && zutl_zar::find(a, entry) >= 0;
      zutl_zar::close(a);
      zutl_zar::destroy(a);
      return ok;
    };
    auto loaded = [&](const std::string& path) {
      mech3::save_mods_begin_load();
      EXPECT(zsave::load(path.c_str(), 2));
      return mech3::save_mods_note();
    };
    const std::string bare = (wd / "bare.mw3").string(), with = (wd / "with.mw3").string();
    in(*campaign, {});
    EXPECT(mech3::save_entry().id == "campaign" && mech3::save_entry().name == "Campaign" && mech3::save_entry().mounted_hash == 0);
    EXPECT(zsave::save(bare.c_str(), 2) && !has(bare, "Mods/entry") && !has(bare, "Mods/identity"));
    in(*campaign, {"weps"});
    EXPECT((mech3::save_entry().ticks == std::vector<std::string>{"Weps"}));
    EXPECT(zsave::save(with.c_str(), 2) && has(with, "Mods/entry"));
    EXPECT(loaded(with).empty());
    in(*pirate, {"weps"});
    const std::string other = loaded(with);
    std::printf("  %s\n", other.c_str());
    EXPECT(other == "Saved in another campaign (Campaign); this is Pirate Missions");
    in(*campaign, {});
    EXPECT(loaded(with) == "Saved in the campaign Campaign with its mods; none now");
    in(*pirate, {"weps"});
    EXPECT(loaded(bare) == "Saved without mods; the campaign Pirate Missions now has: Weps");
  }
  if (!had) zsave::shutdown();
  mech3::save_entry() = keep_entry, mech3::mods_identity() = keep_id;
}
