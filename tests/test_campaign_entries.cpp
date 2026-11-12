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
// two campaigns, a mod that says kind=campaign (the game's campaign over its
// files, a row of its own), mods laid over the game's own files with no
// campaign of their own (c1skin, and overlay: a mission's reader, the main
// reader and a new world: asset mods all the same), a copy of a campaign, a
// mod of another game.
void mods(const fs::path& wd) {
  put(wd / "mods/weps/mod.txt", "name=Freako Weapons\nweapon=GAUSS2 ammo=yes tech=sphere\n");
  put(wd / "mods/tex/mod.txt", "name=Textures\nneeds=weps\n");
  put(wd / "mods/tex/zbd/rtexture.zbd", "x");
  put(wd / "mods/camp/mod.txt", "name=Camp\ncampaign=night.txt\ncampaign=dawn.txt\nneeds=tex\n");
  put(wd / "mods/camp/night.txt", kCampaign);
  put(wd / "mods/camp/dawn.txt", "name=Dawn\nmission=c1 1\nbogus line\n");
  put(wd / "mods/rewrite/mod.txt", "name=Campaign Rewrite\nkind=campaign\n");
  put(wd / "mods/rewrite/zbd/c1/readerm1.zbd", "x");
  put(wd / "mods/overlay/mod.txt", "name=Overlay\nworld=w9\n");
  put(wd / "mods/overlay/zbd/c1/readerm1.zbd", "x");
  put(wd / "mods/overlay/zbd/reader.zbd", "x");
  put(wd / "mods/overlay/zbd/w9/gamez.zbd", "x");
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
  EXPECT(ids(all) == "mw3,ia,mp,camp~night,camp~dawn,copy~night,rewrite");
  const mech3::CampaignEntry* night = mech3::find_entry(all, "camp~night");
  const mech3::CampaignEntry* dawn = mech3::find_entry(all, "camp~dawn");
  const mech3::CampaignEntry* copy = mech3::find_entry(all, "copy~night");
  const mech3::CampaignEntry* rewrite = mech3::find_entry(all, "rewrite");
  EXPECT(night && night->kind == mech3::CampaignEntry::kImported && night->name == "Long Night" && night->error.empty());
  EXPECT(dawn && !dawn->error.empty());  // listed with why it cannot be played
  EXPECT(copy && copy->kind == mech3::CampaignEntry::kCopy && copy->base == "camp~night");
  EXPECT(rewrite && rewrite->campaign.empty() && rewrite->base == "mw3" && rewrite->name == "Campaign Rewrite");  // kind=campaign said
  // A mod laid over the game's own files with no campaign of its own makes no entry: it is ticked for campaigns.
  EXPECT(!mech3::find_entry(all, "overlay") && !mech3::find_entry(all, "c1skin"));
  for (const mech3::ModInfo& m : mech3::mods_found(wd.string()))
    if (m.folder == "overlay" || m.folder == "c1skin" || m.folder == "weps") EXPECT(!mech3::mod_is_campaign(m));
  // A folder of ours whose mod is gone: listed, its saves in reach; one without our marker: not ours, not listed.
  put(wd / "campaigns/gone~x" / mech3::kCampaignMarker, "ours");
  put(wd / "campaigns/gone~x/pilots/Ace.mw3", "pilot");
  put(wd / "campaigns/stranger/notes.txt", "theirs");
  const std::vector<mech3::CampaignEntry> again = mech3::campaign_entries(wd.string());
  EXPECT(ids(again) == "mw3,ia,mp,camp~night,camp~dawn,copy~night,rewrite,gone~x!");
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

// A mod laid over the game's own files (a mission's reader, the main reader,
// a new world) is ticked like any asset mod: for the game's own campaign,
// for an imported campaign, and for Instant Action, and mounts in each. In a
// campaign with a mod of its own that mod comes first in the order (the
// first wins a shared file): the campaign's own files win over a ticked
// overlay, and the tick list's note says which.
TEST(campaign_entries_an_overlay_mod_is_ticked_for_any_campaign_and_instant_action) {
  using V = std::vector<std::string>;
  const fs::path wd = tmp("entries_overlay");
  mods(wd);
  put(wd / "mods/camp/zbd/c1/readerm1.zbd", "camp's");  // the campaign's own copy of the file the overlay also holds
  const std::vector<mech3::CampaignEntry> all = mech3::campaign_entries(wd.string());
  for (const char* id : {"mw3", "ia", "camp~night"}) {
    const mech3::CampaignEntry* e = mech3::find_entry(all, id);
    EXPECT(e && mech3::entry_save_ticks(wd.string(), *e, V{"overlay"}));
    if (!e) continue;
    V locked;
    const V mount = mech3::entry_mount_list(wd.string(), *e, &locked);
    EXPECT(!mount.empty() && mount.back() == "overlay" && mech3::entry_missing_mods(wd.string(), *e).empty());
    if (std::string(id) == "camp~night") EXPECT(!locked.empty() && mount.front() == "camp" && locked.front() == "camp");  // the campaign's own first
    else EXPECT((mount == V{"overlay"}) && locked.empty());
  }
  // The order's note, as the tick list shows it for the campaign: the overlay shares the file, the campaign's wins.
  for (const mech3::ModsListEntry& m : mech3::mods_list(wd.string(), V{"camp", "tex", "weps", "overlay"}))
    if (m.info.folder == "overlay")
      EXPECT(m.on && !m.conflicts_with.empty() && m.conflicts_with.front() == "camp" && m.shadowed.size() == 1);
}

TEST(campaign_entries_kind_rule) {
  mech3::ModInfo m;
  m.path = tmp("entries_kind");
  EXPECT(!mech3::mod_is_campaign(m));
  m.kind = "campaign";
  EXPECT(mech3::mod_is_campaign(m));
  m.kind = "", m.worlds = {"w1"};
  EXPECT(!mech3::mod_is_campaign(m));  // a new world alone: an asset mod (Instant Action's maps)
  m.worlds.clear();
  put(fs::path(m.path) / "zbd/C4B/anim.zbd", "x");
  put(fs::path(m.path) / "zbd/reader.zbd", "x");
  EXPECT(!mech3::mod_is_campaign(m));  // laid over the game's own files: an asset mod
  m.campaigns = {"night.txt"};
  EXPECT(mech3::mod_is_campaign(m));   // a campaign of its own
  m.kind = "ASSETS";
  EXPECT(!mech3::mod_is_campaign(m));  // said otherwise
  m.kind = "", m.campaigns.clear(), m.base = "camp~night";
  EXPECT(mech3::mod_is_campaign(m));   // a copy
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
  put(wd / "mods/a/mod.txt", "name=A\n");
  put(wd / "mods/b/mod.txt", "name=B\n");
  const std::vector<mech3::CampaignEntry> all = mech3::campaign_entries(wd.string());
  const mech3::CampaignEntry* ia = mech3::find_entry(all, mech3::kIaEntry);
  EXPECT(ia && mech3::entry_save_ticks(wd.string(), *ia, V{"a", "b"}));
  EXPECT(mech3::ia_mods_line(wd.string()) == "MODS: off");  // ticked, but this run is the original
  mech3::entry_run() = mech3::EntryRun{};
  mech3::entry_run().screen = mech3::EntryRun::kInstantAction, mech3::entry_run().id = mech3::kIaEntry;
  EXPECT(mech3::ia_mods_line(wd.string()) == "MODS: 2 on");
  EXPECT(mech3::ia_mods_line(wd.string(), 1) == "MODS: 1 on" && mech3::ia_mods_line(wd.string(), 0) == "MODS: off");  // the tick list's own count
  // Multiplayer's entry (kMpEntry) is its own: its ticks apart from Instant Action's, its line only inside it;
  // not a campaign of the MODS screen's, and not deleted there.
  const mech3::CampaignEntry* mp = mech3::find_entry(all, mech3::kMpEntry);
  EXPECT(mp && mp->kind == mech3::CampaignEntry::kMultiplayer && mp->name == "Multiplayer");
  EXPECT(mech3::mp_mods_line(wd.string()) == "MODS: off");  // inside Instant Action's, not multiplayer's
  EXPECT(mp && mech3::entry_save_ticks(wd.string(), *mp, V{"b"}));
  EXPECT(mech3::mp_mods_line(wd.string()) == "MODS: off" && mech3::ia_mods_line(wd.string()) == "MODS: 2 on");
  mech3::entry_run().screen = mech3::EntryRun::kMultiplayer, mech3::entry_run().id = mech3::kMpEntry;
  EXPECT(mech3::mp_mods_line(wd.string()) == "MODS: 1 on" && mech3::ia_mods_line(wd.string()) == "MODS: off");
  EXPECT((mech3::entry_ticks(wd.string(), mech3::kMpEntry) == V{"b"}) && (mech3::entry_ticks(wd.string(), mech3::kIaEntry) == V{"a", "b"}));
  EXPECT(mp && !mech3::entry_plays_own_campaign(wd.string(), *mp));
  mech3::entry_run() = mech3::EntryRun{};
  fs::remove_all(wd / "mods/b");
  EXPECT(mech3::mp_mods_line(wd.string()) == "MODS: off");  // a plain run: off, whatever ticks are remembered (as Instant Action's)
  mech3::entry_run() = mech3::EntryRun{};
}

// Instant Action's MODS, second part (ia_mods.h): INSTANT ACTION goes straight
// into the modded one only with mods ticked and none missing; a plain run
// with a ticked mod gone says so.
TEST(ia_mods_straight_in_and_missing) {
  using V = std::vector<std::string>;
  const fs::path wd = tmp("ia_mods_pilot");
  put(wd / "mods/hud/mod.txt", "name=Green HUD\n");
  put(wd / "mods/weps/mod.txt", "name=Weps\n");
  const std::vector<mech3::CampaignEntry> all = mech3::campaign_entries(wd.string());
  const mech3::CampaignEntry* ia = mech3::find_entry(all, mech3::kIaEntry);
  EXPECT(ia != nullptr);
  mech3::entry_run() = mech3::EntryRun{};
  EXPECT(mech3::ia_mods_line(wd.string()) == "MODS: off");
  EXPECT(mech3::entry_save_ticks(wd.string(), *ia, V{"hud", "weps"}));
  // A plain run is the original whatever is ticked (playtest W9): the ticks are only remembered.
  EXPECT(mech3::ia_mods_line(wd.string()) == "MODS: off" && mech3::ia_mods_missing(wd.string()).empty());
  fs::remove_all(wd / "mods/weps");
  EXPECT((mech3::ia_mods_missing(wd.string()) == V{"weps"}) && mech3::ia_mods_line(wd.string()) == "MODS: off");
  EXPECT(mech3::ia_mods_line(wd.string(), 2) == "MODS: 2 on");  // the tick list's own count stays the ticks'
}

// Which entries a main-menu pilot carries on in (entry_plays_own_campaign):
// the game's own campaign and everything built on it; not a campaign with its
// own campaign file, a copy of one, an entry whose chain is broken, nor
// Instant Action's.
TEST(campaign_entry_plays_the_games_own_campaign_or_not) {
  const fs::path wd = tmp("entries_own");
  put(wd / "mods/camp/mod.txt", "name=Camp\ncampaign=night.txt\n");
  put(wd / "mods/camp/night.txt", "name=Long Night\nmission=c1 1\n");
  put(wd / "mods/rewrite/mod.txt", "name=Rewrite\nkind=campaign\n");  // the game's own campaign over its files, a row by its own word
  put(wd / "mods/rewrite/zbd/c1/anim.zbd", "x");
  put(wd / "mods/mine/mod.txt", "name=My Rewrite\nkind=campaign\nbase=rewrite\n");
  put(wd / "mods/nightedit/mod.txt", "name=Night Edit\nkind=campaign\nbase=camp~night\n");
  put(wd / "mods/orphan/mod.txt", "name=Orphan\nkind=campaign\nbase=nowhere\n");
  const std::vector<mech3::CampaignEntry> all = mech3::campaign_entries(wd.string());
  auto own = [&](const char* id) {
    const mech3::CampaignEntry* e = mech3::find_entry(all, id);
    EXPECT(e != nullptr);
    return e && mech3::entry_plays_own_campaign(wd.string(), *e);
  };
  EXPECT(own("mw3") && own("rewrite") && own("mine"));
  EXPECT(!own("ia") && !own("camp~night") && !own("nightedit") && !own("orphan"));
}
