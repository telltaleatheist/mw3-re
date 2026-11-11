// Mods by campaign, step 4c (upgrades/mods/mods_delete, docs/mods-campaigns-plan.md,
// Deleting and resetting): the one remover's refusals, and DELETE and RESET by
// the row chosen, each on built folders.
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.h"
#include "mech3/mod_campaign.h"
#include "mech3/mod_install.h"
#include "mech3/net_save.h"
#include "upgrades/mods/campaign_entries.h"
#include "upgrades/mods/mods_delete.h"

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
std::string snapshot(const fs::path& dir) {
  std::string t;
  std::error_code ec;
  std::vector<fs::path> all;
  for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) all.push_back(it->path());
  std::sort(all.begin(), all.end());
  for (const fs::path& f : all) {
    t += f.generic_string();
    if (fs::is_regular_file(fs::symlink_status(f, ec))) {
      std::ifstream in(f, std::ios::binary);
      t += ":" + std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
    t += "\n";
  }
  return t;
}
// A write dir with every kind of row: the game's own campaign with pilots, an imported asset mod ticked for
// two campaigns and Instant Action, an imported campaign with pilots, a copy of it of ours with pilots, a
// copy of that copy, a campaign whose mod is gone, a mod folder made by hand, and the main menu's pilots.
fs::path world(const char* name) {
  const fs::path wd = tmp(name);
  put(wd / "pilots/Main.mw3", "the main menu's pilot");
  put(wd / "pilots/games/Main0102.mw3", "its save");
  put(wd / "mods/weps/mod.txt", "name=Freako Weapons\n");
  put(wd / "mods/weps" / mech3::kModAddedMarker, "added by the game\n");
  put(wd / "mods/weps/zbd/reader.zbd", "x");
  put(wd / "mods/camp/mod.txt", "name=Camp\ncampaign=night.txt\n");
  put(wd / "mods/camp/night.txt", "name=Long Night\nmission=c1 1\n");
  put(wd / "mods/camp" / mech3::kModAddedMarker, "added by the game\n");
  put(wd / "mods/mine/mod.txt", "name=Night, mine\nkind=campaign\nbase=camp~night\n");
  put(wd / "mods/mine" / mech3::ai::kEditorMarker, "the editor's\n");
  put(wd / "mods/mine/zbd/c1/readerm1.zbd", "edited");
  put(wd / "mods/mine/notes of mine.txt", "a stray file in a folder of ours");
  put(wd / "mods/mine2/mod.txt", "name=Night, mine 2\nkind=campaign\nbase=mine\n");
  put(wd / "mods/mine2" / mech3::ai::kEditorMarker, "the editor's\n");
  put(wd / "mods/byhand/mod.txt", "name=By Hand\n");
  put(wd / "mods/byhand/zbd/x.zbd", "theirs");
  for (const char* id : {"mw3", "camp~night", "mine", "gone~far"}) {
    put(wd / "campaigns" / id / mech3::kCampaignMarker, std::string("\"") + (std::string(id) == "gone~far" ? "Far Away" : id) + "\" (" + id + ")\n");
    put(wd / "campaigns" / id / "pilots/Ace.mw3", "ace");
    put(wd / "campaigns" / id / "pilots/Bo.mw3", "bo");
    put(wd / "campaigns" / id / "pilots/games/Ace0101.mw3", "s1");
    put(wd / "campaigns" / id / "pilots/games/Ace0102.mw3", "s2");
    put(wd / "campaigns" / id / "pilots/games/Bo0101.mw3", "s3");
    put(wd / "campaigns" / id / "configs/Bushwacker.mw3", "loadouts");
  }
  put(wd / "campaigns/mw3/mods.txt", "mod=weps\n");
  put(wd / "campaigns/camp~night/mods.txt", "mod=weps\n");
  put(wd / "campaigns/ia" / mech3::kCampaignMarker, "ia\n");
  put(wd / "campaigns/ia/mods.txt", "mod=weps\n");
  return wd;
}
}  // namespace

// The one remover: a folder directly under mods\ or campaigns\ of this write dir, with its marker, and nothing else.
TEST(mods_delete_remover_takes_only_a_marked_folder) {
  const fs::path wd = world("delete_remover");
  const std::string w = wd.string();
  const std::string before = snapshot(wd);
  std::string why;
  // No marker: a folder made by hand.
  EXPECT(!mech3::remove_marked_folder(w, "mods", "byhand", mech3::kModAddedMarker, &why) && why.find("not made by the game") != std::string::npos);
  // The wrong marker for it (an imported mod is not the editor's).
  EXPECT(!mech3::remove_marked_folder(w, "mods", "weps", mech3::ai::kEditorMarker, &why));
  // Names that are not one plain folder, and places that are not ours.
  for (const char* name : {"", ".", "..", "weps/zbd", "../pilots", "weps\\zbd"})
    EXPECT(!mech3::remove_marked_folder(w, "mods", name, mech3::kModAddedMarker, &why));
  EXPECT(!mech3::remove_marked_folder(w, "pilots", "games", mech3::kModAddedMarker, &why) && why == "not a folder of the game's");
  EXPECT(!mech3::remove_marked_folder(w, "", "mods", mech3::kModAddedMarker, &why));
  EXPECT(!mech3::remove_marked_folder(w, "mods", "nowhere", mech3::kModAddedMarker, &why) && why.find("is not there") != std::string::npos);
  EXPECT(!mech3::remove_marked_folder("", "mods", "weps", mech3::kModAddedMarker, &why));
  EXPECT(!mech3::remove_marked_folder(w, "mods", "weps", "", &why) && !mech3::remove_marked_folder(w, "mods", "weps", nullptr, &why));
  EXPECT(snapshot(wd) == before);
#if !defined(_WIN32)
  // A link standing for the folder, and a marker that is a link: refused. (On Windows a junction counts the
  // same by path_is_link's reparse-point test: tests/win/delete_check.cpp, run on the PC.)
  std::error_code ec;
  fs::create_directory_symlink(wd / "mods/weps", wd / "mods/linked", ec);
  EXPECT(mech3::path_is_link((wd / "mods/linked").string()) && !mech3::path_is_link((wd / "mods/weps").string()) &&
         !mech3::path_is_link((wd / "mods/weps/mod.txt").string()) && !mech3::path_is_link((wd / "mods/nowhere").string()));
  EXPECT(!ec && !mech3::remove_marked_folder(w, "mods", "linked", mech3::kModAddedMarker, &why) && why.find("is a link") != std::string::npos);
  fs::remove(wd / "mods/linked", ec);
  put(wd / "outside/real marker.txt", "x");
  fs::create_directories(wd / "mods/fake", ec);
  fs::create_symlink(wd / "outside/real marker.txt", wd / "mods/fake" / mech3::kModAddedMarker, ec);
  EXPECT(!ec && !mech3::remove_marked_folder(w, "mods", "fake", mech3::kModAddedMarker, &why) && why.find("has no") != std::string::npos);
  fs::remove_all(wd / "mods/fake", ec);
  // A link inside a marked folder is removed as a link: what it points at stays.
  put(wd / "outside/kept/file.txt", "must stay");
  fs::create_directory_symlink(wd / "outside/kept", wd / "mods/mine2/out", ec);
  fs::create_symlink(wd / "outside/real marker.txt", wd / "mods/mine2/out file", ec);
  EXPECT(mech3::remove_marked_folder(w, "mods", "mine2", mech3::ai::kEditorMarker, &why));
  EXPECT(!fs::exists(wd / "mods/mine2") && fs::is_regular_file(wd / "outside/kept/file.txt") && fs::is_regular_file(wd / "outside/real marker.txt"));
#endif
  // A marked mod folder goes whole, a stray file of the player's in it too (it is ours: we made the folder).
  EXPECT(mech3::remove_marked_folder(w, "mods", "mine", mech3::ai::kEditorMarker, &why) && !fs::exists(wd / "mods/mine"));
  // A marked campaign folder holding a folder the game does not make there: stopped, said, nothing removed.
  put(wd / "campaigns/mine/my backups/Ace.mw3", "theirs");
  const std::string camp_before = snapshot(wd / "campaigns/mine");
  EXPECT(!mech3::remove_marked_folder(w, "campaigns", "mine", mech3::kCampaignMarker, &why) && why.find("holds a folder the game did not put there (my backups)") != std::string::npos);
  EXPECT(snapshot(wd / "campaigns/mine") == camp_before);
  // A stray file there (not a folder) goes with the marked folder.
  put(wd / "campaigns/gone~far/notes.txt", "a file");
  EXPECT(mech3::remove_marked_folder(w, "campaigns", "gone~far", mech3::kCampaignMarker, &why) && !fs::exists(wd / "campaigns/gone~far"));
  EXPECT(fs::is_regular_file(wd / "pilots/Main.mw3") && fs::is_regular_file(wd / "mods/byhand/zbd/x.zbd"));
}

// DELETE and RESET by the row: what each says, removes and leaves.
TEST(mods_delete_by_the_row_chosen) {
  using mech3::DeletePlan;
  std::string why;
  {  // A copy another copy is built on: refused; the top copy: the copy and its campaign folder.
    const fs::path wd = world("delete_copy");
    const std::string w = wd.string();
    put(wd / "campaigns/mine2" / mech3::kCampaignMarker, "mine2\n");
    DeletePlan p = mech3::delete_plan_for_entry(w, "mine");
    EXPECT(p.kind == DeletePlan::kNone && p.label.empty() && p.refused == "Night, mine cannot be deleted: Night, mine 2 is built on it. Delete that one first.");
    EXPECT(!mech3::delete_run(w, p, &why) && fs::exists(wd / "mods/mine"));
    p = mech3::delete_plan_for_entry(w, "mine2");
    EXPECT(p.kind == DeletePlan::kCopy && p.label == "DELETE" && p.question == "Delete Night, mine 2? It has no pilots or saved games.");
    EXPECT(mech3::delete_run(w, p, &why) && !fs::exists(wd / "mods/mine2") && !fs::exists(wd / "campaigns/mine2"));
    p = mech3::delete_plan_for_entry(w, "mine");
    EXPECT(p.kind == DeletePlan::kCopy && p.question == "Delete Night, mine? Its 2 pilots and 3 saved games go with it.");
    const std::string others = snapshot(wd / "mods/camp") + snapshot(wd / "campaigns/camp~night") + snapshot(wd / "pilots");
    EXPECT(mech3::delete_run(w, p, &why) && !fs::exists(wd / "mods/mine") && !fs::exists(wd / "campaigns/mine"));
    EXPECT(snapshot(wd / "mods/camp") + snapshot(wd / "campaigns/camp~night") + snapshot(wd / "pilots") == others);  // what it was saved from, untouched
  }
  {  // An imported campaign: refused while a copy is built on it; then its mod only, its pilots and saved games kept.
    const fs::path wd = world("delete_campaign");
    const std::string w = wd.string();
    DeletePlan p = mech3::delete_plan_for_entry(w, "camp~night");
    EXPECT(p.kind == DeletePlan::kNone && p.refused == "Long Night cannot be deleted: Night, mine is built on it. Delete that one first.");
    std::error_code ec;
    fs::remove_all(wd / "mods/mine2", ec), fs::remove_all(wd / "mods/mine", ec);
    p = mech3::delete_plan_for_entry(w, "camp~night");
    EXPECT(p.kind == DeletePlan::kImportedCampaign && p.question == "Delete Long Night? Its pilots and saved games are kept: import it again to go on.");
    const std::string kept = snapshot(wd / "campaigns/camp~night");
    EXPECT(mech3::delete_run(w, p, &why) && !fs::exists(wd / "mods/camp") && snapshot(wd / "campaigns/camp~night") == kept);
    // It is the "!" kind now, and DELETE there removes what is left.
    const std::vector<mech3::CampaignEntry> all = mech3::campaign_entries(w);
    const mech3::CampaignEntry* left = mech3::find_entry(all, "camp~night");
    EXPECT(left && left->missing);
    p = mech3::delete_plan_for_entry(w, "camp~night");
    EXPECT(p.kind == DeletePlan::kLeftover && p.question == "Delete what is left of camp~night? Its 2 pilots and 3 saved games go with it.");
    EXPECT(mech3::delete_run(w, p, &why) && !fs::exists(wd / "campaigns/camp~night"));
    p = mech3::delete_plan_for_entry(w, "gone~far");
    EXPECT(p.kind == DeletePlan::kLeftover && p.question.find("Delete what is left of ") == 0);
  }
  {  // An imported asset mod: says where it is ticked; its folder only; the ticks are left, so those campaigns show it missing.
    const fs::path wd = world("delete_asset");
    const std::string w = wd.string();
    DeletePlan p = mech3::delete_plan_for_mod(w, "weps");
    EXPECT(p.kind == DeletePlan::kImportedMod && p.question == "Delete Freako Weapons? It is ticked for: MechWarrior 3, Instant Action, Long Night.");
    const std::string campaigns = snapshot(wd / "campaigns");
    EXPECT(mech3::delete_run(w, p, &why) && !fs::exists(wd / "mods/weps") && snapshot(wd / "campaigns") == campaigns);
    const std::vector<mech3::CampaignEntry> all = mech3::campaign_entries(w);
    const mech3::CampaignEntry* own = mech3::find_entry(all, "mw3");
    EXPECT(own && (mech3::entry_missing_mods(w, *own) == std::vector<std::string>{"weps"}));
    // One not ticked anywhere; one made by hand: no DELETE, the note says where to remove it.
    put(wd / "mods/hud/mod.txt", "name=Green HUD\n");
    put(wd / "mods/hud" / mech3::kModAddedMarker, "added by the game\n");
    EXPECT(mech3::delete_plan_for_mod(w, "hud").question == "Delete Green HUD? It is not ticked for any campaign.");
    p = mech3::delete_plan_for_mod(w, "byhand");
    const std::string hand = snapshot(wd / "mods/byhand");
    EXPECT(p.kind == DeletePlan::kNone && p.label.empty() && p.refused == "By Hand was not added by the game: remove its folder yourself (MODS FOLDER).");
    EXPECT(!mech3::delete_run(w, p, &why) && snapshot(wd / "mods/byhand") == hand);
    // Forced past the plan (a plan made up with the wrong marker): the remover still refuses.
    p.kind = DeletePlan::kImportedMod, p.mod = "byhand", p.mod_marker = mech3::kModAddedMarker;
    EXPECT(!mech3::delete_run(w, p, &why) && snapshot(wd / "mods/byhand") == hand);
  }
  {  // The game's own campaign: RESET. Ticks, pilots, saved games and saved mechs cleared; the folder as a first start makes it.
    const fs::path wd = world("delete_reset");
    const std::string w = wd.string();
    DeletePlan p = mech3::delete_plan_for_entry(w, "mw3");
    EXPECT(p.kind == DeletePlan::kReset && p.label == "RESET" &&
           p.question == "Reset MechWarrior 3 to the original? Its ticked mods are cleared. Its 2 pilots and 3 saved games here go with it. "
                         "Your pilots in the main game are not changed.");
    const std::string main_pilots = snapshot(wd / "pilots"), others = snapshot(wd / "campaigns/camp~night") + snapshot(wd / "mods");
    EXPECT(mech3::delete_run(w, p, &why));
    EXPECT(fs::is_regular_file(wd / "campaigns/mw3" / mech3::kCampaignMarker) && fs::is_directory(wd / "campaigns/mw3/pilots/games") &&
           fs::is_directory(wd / "campaigns/mw3/configs"));
    EXPECT(mech3::entry_ticks(w, "mw3").empty() && !fs::exists(wd / "campaigns/mw3/pilots/Ace.mw3") && !fs::exists(wd / "campaigns/mw3/configs/Bushwacker.mw3"));
    EXPECT(snapshot(wd / "pilots") == main_pilots && snapshot(wd / "campaigns/camp~night") + snapshot(wd / "mods") == others);
    EXPECT(mech3::delete_plan_for_entry(w, "mw3").question.find("It has no pilots or saved games here.") != std::string::npos);  // counted at that moment, and none said plainly
    // Never: the campaign the run is inside; Instant Action's entry (ALL OFF is its way).
    p = mech3::delete_plan_for_entry(w, "camp~night", "camp~night");
    EXPECT(p.kind == DeletePlan::kNone && p.refused == "Long Night is being played: leave it first.");
    p = mech3::delete_plan_for_entry(w, "ia");
    EXPECT(p.kind == DeletePlan::kNone && p.refused.find("ALL OFF") != std::string::npos);
    EXPECT(mech3::delete_plan_for_entry(w, "nowhere").kind == DeletePlan::kNone);
  }
}

// The staging folders' remover (mod_install's and the folder import's, both
// remove_marked_tree): a real folder with its marker goes whole, links inside
// removed as links; a folder without the marker, a link standing for the
// folder, and a marker that is a link are refused with nothing removed.
TEST(mods_delete_staging_folders_by_the_same_rule) {
  const fs::path wd = tmp("delete_staging");
  std::string why;
  put(wd / "stage/marker.txt", "ours");
  put(wd / "stage/a/b/file.txt", "x");
  put(wd / "outside/keep.txt", "must stay");
  put(wd / "plain/file.txt", "no marker");
  EXPECT(!mech3::remove_marked_tree((wd / "plain").string(), "marker.txt", &why) && why.find("has no marker.txt") != std::string::npos);
  EXPECT(!mech3::remove_marked_tree("", "marker.txt", &why) && !mech3::remove_marked_tree((wd / "stage").string(), "", &why));
  EXPECT(!mech3::remove_marked_tree((wd / "stage").string(), "a/marker.txt", &why));
  EXPECT(!mech3::remove_marked_tree((wd / "nowhere").string(), "marker.txt", &why) && why.find("is not there") != std::string::npos);
  EXPECT(fs::is_regular_file(wd / "plain/file.txt") && fs::is_regular_file(wd / "stage/a/b/file.txt"));
#if !defined(_WIN32)
  std::error_code ec;
  fs::create_directory_symlink(wd / "outside", wd / "stage/out", ec);
  fs::create_directory_symlink(wd / "stage", wd / "linked stage", ec);
  EXPECT(!ec && !mech3::remove_marked_tree((wd / "linked stage").string(), "marker.txt", &why) && why.find("is a link") != std::string::npos);
  EXPECT(fs::is_regular_file(wd / "stage/a/b/file.txt"));
  fs::create_directories(wd / "fake", ec);
  fs::create_symlink(wd / "stage/marker.txt", wd / "fake/marker.txt", ec);
  EXPECT(!ec && !mech3::remove_marked_tree((wd / "fake").string(), "marker.txt", &why));
#endif
  EXPECT(mech3::remove_marked_tree((wd / "stage").string(), "marker.txt", &why) && !fs::exists(wd / "stage"));
  EXPECT(fs::is_regular_file(wd / "outside/keep.txt"));
}
