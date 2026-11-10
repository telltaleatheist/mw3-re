// Mods by campaign, step 1 (upgrades/mods/campaign_entries, docs/mods-campaigns-plan.md):
// the entries listed from the mods folder, a mod's kind, each entry's folder
// and ticks, what it mounts, and the first start's move of mw3.cfg's mod= lines.
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/mech_config.h"
#include "mech3/mech_names.h"
#include "mech3/mod_campaign.h"
#include "upgrades/mods/campaign_entries.h"
#include "upgrades/mods/entry_run.h"
#include "upgrades/mods/ia_mods.h"

namespace {
namespace fs = std::filesystem;
std::string tmp(const char* name) {
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
const char* kCampaign = "name=Long Night\nop=Op 1\nmission=c1 1\n";
// A mods folder with each kind: assets (rows, a texture), a campaign mod with
// two campaigns, a mod over the game's own c1, one kind=assets over c1 all
// the same, a copy of a campaign, a mod of another game.
void mods(const fs::path& wd) {
  put(wd / "mods/weps/mod.txt", "name=Freako Weapons\nweapon=GAUSS2 ammo=yes tech=sphere\n");
  put(wd / "mods/tex/mod.txt", "name=Textures\nneeds=weps\n");
  put(wd / "mods/tex/zbd/rtexture.zbd", "x");
  put(wd / "mods/camp/mod.txt", "name=Camp\ncampaign=night.txt\ncampaign=dawn.txt\nneeds=tex\n");
  put(wd / "mods/camp/night.txt", kCampaign);
  put(wd / "mods/camp/dawn.txt", "name=Dawn\nmission=c1 1\nbogus line\n");
  put(wd / "mods/rewrite/mod.txt", "name=Campaign Rewrite\n");
  put(wd / "mods/rewrite/zbd/c1/readerm1.zbd", "x");
  put(wd / "mods/c1skin/mod.txt", "name=C1 Skin\nkind=assets\n");
  put(wd / "mods/c1skin/zbd/c1/gamez.zbd", "x");
  put(wd / "mods/copy/mod.txt", "name=My Night\nbase=camp~night\ncampaign=night.txt\n");
  put(wd / "mods/copy/night.txt", kCampaign);
  put(wd / "mods/pmonly/mod.txt", "name=PM\nfor=mw3pm\n");
}
}  // namespace

TEST(campaign_entries_listed_by_kind) {
  const fs::path wd = tmp("entries_listed");
  mods(wd);
  const std::vector<mech3::CampaignEntry> all = mech3::campaign_entries(wd.string());
  std::printf("  entries: %s\n", ids(all).c_str());
  EXPECT(ids(all) == "mw3,ia,camp~night,camp~dawn,copy~night,rewrite");
  const mech3::CampaignEntry* night = mech3::find_entry(all, "camp~night");
  const mech3::CampaignEntry* dawn = mech3::find_entry(all, "camp~dawn");
  const mech3::CampaignEntry* copy = mech3::find_entry(all, "copy~night");
  const mech3::CampaignEntry* rewrite = mech3::find_entry(all, "rewrite");
  EXPECT(night && night->kind == mech3::CampaignEntry::kImported && night->name == "Long Night" && night->error.empty());
  EXPECT(dawn && !dawn->error.empty());  // listed with why it cannot be played
  EXPECT(copy && copy->kind == mech3::CampaignEntry::kCopy && copy->base == "camp~night");
  EXPECT(rewrite && rewrite->campaign.empty() && rewrite->base == "mw3" && rewrite->name == "Campaign Rewrite");
  // A folder of ours whose mod is gone: listed, its saves in reach; one without our marker: not ours, not listed.
  put(wd / "campaigns/gone~x" / mech3::kCampaignMarker, "ours");
  put(wd / "campaigns/gone~x/pilots/Ace.mw3", "pilot");
  put(wd / "campaigns/stranger/notes.txt", "theirs");
  const std::vector<mech3::CampaignEntry> again = mech3::campaign_entries(wd.string());
  EXPECT(ids(again) == "mw3,ia,camp~night,camp~dawn,copy~night,rewrite,gone~x!");
  const mech3::CampaignEntry* gone = mech3::find_entry(again, "gone~x");
  EXPECT(gone && gone->missing && gone->mod == "gone" && gone->campaign == "x" && !gone->error.empty());
}

TEST(campaign_entries_folder_ticks_and_mounts) {
  const fs::path wd = tmp("entries_ticks");
  mods(wd);
  const std::vector<mech3::CampaignEntry> all = mech3::campaign_entries(wd.string());
  const mech3::CampaignEntry* mw3 = mech3::find_entry(all, "mw3");
  const mech3::CampaignEntry* copy = mech3::find_entry(all, "copy~night");
  const mech3::CampaignEntry* rewrite = mech3::find_entry(all, "rewrite");
  EXPECT(mw3 && copy && rewrite);
  if (!mw3 || !copy || !rewrite) return;
  // Its folder with our marker and pilots\games; ticks written and read back in order.
  EXPECT(mech3::entry_ticks(wd.string(), "mw3").empty());
  EXPECT(mech3::entry_save_ticks(wd.string(), *mw3, {"tex", "weps", "nosuch"}));
  EXPECT(fs::is_regular_file(wd / "campaigns/mw3" / mech3::kCampaignMarker) && fs::is_directory(wd / "campaigns/mw3/pilots/games"));
  EXPECT((mech3::entry_ticks(wd.string(), "mw3") == std::vector<std::string>{"tex", "weps", "nosuch"}));
  EXPECT(mech3::entry_pilots_folder("mw3") == "campaigns\\mw3\\pilots");
  // The game's own: its ticks alone (a tick whose mod is gone left out).
  EXPECT((mech3::entry_mount_list(wd.string(), *mw3) == std::vector<std::string>{"tex", "weps"}));
  // A copy: itself, what it was saved from, what that needs (tex, and weps for tex), then its ticks; those locked.
  EXPECT(mech3::entry_save_ticks(wd.string(), *copy, {"c1skin", "weps"}));
  std::vector<std::string> locked;
  const std::vector<std::string> m = mech3::entry_mount_list(wd.string(), *copy, &locked);
  EXPECT((m == std::vector<std::string>{"copy", "camp", "tex", "weps", "c1skin"}));
  EXPECT((locked == std::vector<std::string>{"copy", "camp", "tex", "weps"}));
  // A ticked mod not in the mods folder: said, so the entry does not start (its ticks and pilots kept).
  EXPECT((mech3::entry_missing_mods(wd.string(), *mw3) == std::vector<std::string>{"nosuch"}));
  EXPECT(mech3::entry_missing_mods(wd.string(), *copy).empty());
  put(wd / "mods/orphan/mod.txt", "name=Orphan\nbase=gone~x\n");  // a copy of a campaign that is gone
  const std::vector<mech3::CampaignEntry> with = mech3::campaign_entries(wd.string());
  if (const mech3::CampaignEntry* o = mech3::find_entry(with, "orphan"))
    EXPECT((mech3::entry_missing_mods(wd.string(), *o) == std::vector<std::string>{"gone~x"}));
  else
    EXPECT(false);
  // A mod over the game's own campaign: itself first.
  EXPECT((mech3::entry_mount_list(wd.string(), *rewrite) == std::vector<std::string>{"rewrite"}));
  // A folder the game did not make is never taken.
  put(wd / "campaigns/rewrite/theirs.txt", "x");
  std::string why;
  EXPECT(!mech3::entry_ensure(wd.string(), *rewrite, &why) && why.find("did not put there") != std::string::npos);
  EXPECT(!mech3::entry_save_ticks(wd.string(), *rewrite, {"weps"}) && !fs::exists(wd / "campaigns/rewrite/mods.txt"));
  // Mods never serve a campaign entry's files.
  EXPECT(!zutl::mod_may_serve("campaigns/mw3/mods.txt") && !zutl::mod_may_serve("Campaigns/mw3/pilots/A.mw3"));
}

TEST(campaign_entries_kind_rule) {
  mech3::ModInfo m;
  m.path = tmp("entries_kind");
  EXPECT(!mech3::mod_is_campaign(m));
  m.kind = "campaign";
  EXPECT(mech3::mod_is_campaign(m));
  m.kind = "", m.worlds = {"w1"};
  EXPECT(mech3::mod_is_campaign(m));
  m.kind = "ASSETS";
  EXPECT(!mech3::mod_is_campaign(m));
  m.kind = "", m.worlds.clear();
  put(fs::path(m.path) / "zbd/C4B/anim.zbd", "x");  // any case
  EXPECT(mech3::mod_is_campaign(m));
  mech3::ModInfo parsed;
  EXPECT(mech3::mod_parse("name=X\nkind=assets\nbase=camp~night\n", &parsed) && parsed.kind == "assets" && parsed.base == "camp~night");
}

// The first start of this build: mw3.cfg's mod= lines become the game's own
// entry's ticks (its asset mods) and each enabled campaign mod's entries' too;
// old custom-campaign pilots move to their entry; mw3.cfg keeps every byte
// and gets mods_moved=1; a second start changes nothing.
TEST(campaign_entries_first_start_moves_the_mods) {
  const fs::path wd = tmp("entries_migrate");
  mods(wd);
  const std::string cfg = "install=C:\\Games\\MW3\r\nmod=camp\r\nmod=weps\r\nsound=0.8  # kept\r\nmod=tex";
  put(wd / "mw3.cfg", cfg);
  put(wd / "pilots/mods/camp/night" / mech3::kCampaignMarker, "ours");
  put(wd / "pilots/mods/camp/night/Ace.mw3", "ace");
  put(wd / "pilots/mods/camp/night/games/Ace0102.mw3", "save");
  put(wd / "pilots/mods/camp/dawn/Mine.mw3", "not ours: no marker");
  put(wd / "pilots/Orig.mw3", "orig");
  const std::vector<std::string> log = mech3::entries_migrate(wd.string());
  for (const std::string& l : log) std::printf("  %s\n", l.c_str());
  EXPECT((mech3::entry_ticks(wd.string(), "mw3") == std::vector<std::string>{"weps", "tex"}));
  EXPECT((mech3::entry_ticks(wd.string(), "camp~night") == std::vector<std::string>{"weps", "tex"}));
  EXPECT((mech3::entry_ticks(wd.string(), "camp~dawn") == std::vector<std::string>{"weps", "tex"}));
  EXPECT(mech3::entry_ticks(wd.string(), "rewrite").empty() && !fs::exists(wd / "campaigns/rewrite"));  // it was off
  EXPECT(text_of(wd / "mw3.cfg") == cfg + "\r\nmods_moved=1\r\n");
  EXPECT(text_of(wd / "campaigns/camp~night/pilots/Ace.mw3") == "ace" &&
         text_of(wd / "campaigns/camp~night/pilots/games/Ace0102.mw3") == "save" &&
         fs::is_regular_file(wd / "campaigns/camp~night" / mech3::kCampaignMarker) &&
         !fs::exists(wd / "campaigns/camp~night/pilots" / mech3::kCampaignMarker) && !fs::exists(wd / "pilots/mods/camp/night"));
  EXPECT(text_of(wd / "pilots/mods/camp/dawn/Mine.mw3") == "not ours: no marker" && text_of(wd / "pilots/Orig.mw3") == "orig");
  // Once: a second start, the ticks changed meanwhile, nothing moves again.
  const std::vector<mech3::CampaignEntry> all = mech3::campaign_entries(wd.string());
  EXPECT(mech3::entry_save_ticks(wd.string(), *mech3::find_entry(all, "mw3"), {"tex"}));
  EXPECT(mech3::entries_migrate(wd.string()).empty());
  EXPECT((mech3::entry_ticks(wd.string(), "mw3") == std::vector<std::string>{"tex"}) && text_of(wd / "mw3.cfg") == cfg + "\r\nmods_moved=1\r\n");
}

// Step 2 (docs/mods-campaigns-plan.md): an entry's saved loadouts are its
// own, in campaigns/<id>/configs, made empty with the entry; the main
// menu's configs folder is neither read nor written from inside one.
TEST(campaign_entry_keeps_its_own_saved_loadouts) {
  const fs::path wd = tmp("entries_configs");
  const zutl::Mounts before = zutl::mounts();
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd.string());
  const std::vector<mech3::CampaignEntry> all = mech3::campaign_entries(wd.string());
  const mech3::CampaignEntry* ia = mech3::find_entry(all, "ia");
  EXPECT(ia && mech3::entry_ensure(wd.string(), *ia, nullptr) && fs::is_directory(wd / "campaigns/ia/configs"));
  EXPECT(mech3::configs_folder() == "configs");
  const std::string file = std::string(mech3::mech_name(0)) + ".mw3";
  {
    mech3::ConfigList main_menu(0, 0);
    EXPECT(main_menu.save_all() && fs::is_regular_file(wd / "configs" / file));  // the original's place
  }
  const auto stamp = fs::last_write_time(wd / "configs" / file);
  mech3::set_configs_folder(mech3::entry_configs_folder("ia"));
  {
    mech3::ConfigList list(0, 0);
    EXPECT(!list.load_saved());  // the main menu's is not the entry's: none yet
    EXPECT(list.save_all() && fs::is_regular_file(wd / "campaigns/ia/configs" / file));
    EXPECT(list.load_saved());
  }
  EXPECT(fs::last_write_time(wd / "configs" / file) == stamp);
  mech3::set_configs_folder("");
  EXPECT(mech3::configs_folder() == "configs");
  zutl::mounts() = before;
}

// Instant Action's MODS (upgrades/mods/ia_mods_menu.h): what ACCEPT does with
// the ticks as they were and are, and the screen's line.
TEST(ia_mods_accept_steps_and_line) {
  using V = std::vector<std::string>;
  using mech3::IaModsStep;
  // A plain run: nothing ticked is the original and nothing starts again; any ticked starts into the entry.
  EXPECT(mech3::ia_mods_step({}, {}, false) == IaModsStep::kBack);
  EXPECT(mech3::ia_mods_step({"a"}, {}, false) == IaModsStep::kBack);
  EXPECT(mech3::ia_mods_step({}, {"a"}, false) == IaModsStep::kIntoEntry);
  EXPECT(mech3::ia_mods_step({"a"}, {"a"}, false) == IaModsStep::kIntoEntry);  // ticked but not mounted in this run
  // Inside the entry: unchanged goes back, changed starts again with the new set, none left goes back to the plain run.
  EXPECT(mech3::ia_mods_step({"a", "b"}, {"a", "b"}, true) == IaModsStep::kBack);
  EXPECT(mech3::ia_mods_step({"a", "b"}, {"b", "a"}, true) == IaModsStep::kIntoEntry);  // the order is a change
  EXPECT(mech3::ia_mods_step({"a"}, {"a", "b"}, true) == IaModsStep::kIntoEntry);
  EXPECT(mech3::ia_mods_step({"a"}, {}, true) == IaModsStep::kPlain);
  const fs::path wd = tmp("ia_mods_line");
  const std::vector<mech3::CampaignEntry> all = mech3::campaign_entries(wd.string());
  const mech3::CampaignEntry* ia = mech3::find_entry(all, mech3::kIaEntry);
  EXPECT(ia && mech3::entry_save_ticks(wd.string(), *ia, V{"a", "b"}));
  EXPECT(mech3::ia_mods_line(wd.string()) == "MODS: off");  // ticked, but this run is the original
  mech3::entry_run() = mech3::EntryRun{};
  mech3::entry_run().screen = mech3::EntryRun::kInstantAction, mech3::entry_run().id = mech3::kIaEntry;
  EXPECT(mech3::ia_mods_line(wd.string()) == "MODS: 2 on");
  EXPECT(mech3::ia_mods_line(wd.string(), 1) == "MODS: 1 on" && mech3::ia_mods_line(wd.string(), 0) == "MODS: off");  // the tick list's own count
  mech3::entry_run() = mech3::EntryRun{};
}
