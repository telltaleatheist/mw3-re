// A mod campaign's game and pilots (mech3/mod_campaign, docs/custom-campaigns.md):
// its own pilots folder with our marker, its campaign games on the list, the
// original's pilots untouched, and its folder removed only on the player's
// yes and only with our marker. Kept apart from test_mods.cpp, whose headers
// declare the mech tables pilot.h's Store clashes with.
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/mod_campaign.h"
#include "mech3/pilot.h"

namespace {
namespace fs = std::filesystem;
// The pilot module's calls out: the "Replace existing game?" answer, and
// zSave's worlds (a saved game's file written, so readable() finds it; the
// reads listed).
struct CampaignHooks : mech3::PilotHooks {
  std::vector<std::string> saved, loaded;
  uint32_t unported(const char*, std::initializer_list<zgame::Arg>) override { return 0; }
  int32_t message_box(int32_t, int32_t, int32_t) override { return 1; }
  void save_world(const char* path, int32_t) override {
    saved.push_back(path);
    std::ofstream(zutl::write_path(path)) << "world";
  }
  bool load_world(const char* path, int32_t) override {
    loaded.push_back(path);
    return true;
  }
};
std::string tmp(const char* name) {
  const std::string d = std::string(MW3_TEST_TMP) + "/" + name;
  std::error_code ec;
  fs::remove_all(d, ec);
  fs::create_directories(d, ec);
  return d;
}
mech3::ModCampaign hard_rain() {
  mech3::ModCampaign c;
  c.mod = "m", c.id = "hard", c.file = "hard.txt";
  mech3::campaign_parse("name=Hard Rain\nop=Op A bases=3\nmission=c1 1\nmission=c1 2\nop=Op B\nmission=c4b 5\nmission=c4b 6\n", &c);
  return c;
}
void drop_all() {
  mech3::current_pilot() = nullptr;
  while (!mech3::pilots().empty()) mech3::delete_pilot(mech3::pilots().front()->name);
}
}  // namespace

TEST(mod_campaign_pilots_and_games) {
  zutl::mounts().read.clear();
  const std::string wd = tmp("mod_campaign_pilots");
  zutl::set_write_dir(wd);
  CampaignHooks h;
  mech3::set_pilot_hooks(&h);
  const mech3::ModCampaign c = hard_rain();
  EXPECT(c.error.empty() && c.missions.size() == 4);
  const fs::path own = fs::path(wd) / "pilots/mods/m/hard";

  mech3::Pilot* orig = mech3::create_pilot("Orig");
  EXPECT(orig && fs::is_regular_file(fs::path(wd) / "pilots/Orig.mw3"));
  EXPECT(dynamic_cast<mech3::ModCampaignGame*>(orig->campaigns[0]) == nullptr);

  // Entered: its folder made with the marker, the original's pilots let go.
  std::string why;
  EXPECT(mech3::campaign_enter(c, &why) && why.empty());
  EXPECT(fs::is_regular_file(own / mech3::kCampaignMarker) && fs::is_directory(own / "games"));
  EXPECT(mech3::pilots().empty() && mech3::current_pilot() == nullptr && mech3::entered_campaign() != nullptr);
  EXPECT(mech3::pilots_folder() == "pilots\\mods\\m\\hard");
  mech3::Pilot* ace = mech3::create_pilot("Ace");
  EXPECT(ace && fs::is_regular_file(own / "Ace.mw3") && !fs::exists(fs::path(wd) / "pilots/Ace.mw3"));
  auto* g = ace ? dynamic_cast<mech3::ModCampaignGame*>(ace->campaigns[0]) : nullptr;
  EXPECT(g != nullptr);
  if (!g) return;
  // The first line: c1's m1 as the original's Op 1 Mission 1, with its stock.
  EXPECT(g->type() == mech3::kCampaign && g->campaign == 1 && g->mission == 1);
  EXPECT(g->operation() == 1 && g->mission_number() == 1 && g->world_index() == 0 && std::string(g->name()) == "Op A, Mission 1");
  EXPECT(g->store.count() != 0);

  // The next mission is the list's next line, saved in the campaign's own games\.
  mech3::current_pilot() = ace;
  EXPECT(ace->select_campaign(0) && !ace->record_mission(true));
  auto* g2 = dynamic_cast<mech3::ModCampaignGame*>(ace->current);
  EXPECT(g2 && g2->mission == 2 && g2->operation() == 1 && g2->mission_number() == 2 && g2->world_index() == 0);
  EXPECT(g2 && std::string(g2->name()) == "Op A, Mission 2");
  EXPECT(h.saved.size() == 1 && h.saved.back() == "pilots\\mods\\m\\hard//games//Ace0102.mw3");
  EXPECT(fs::is_regular_file(own / "games/Ace0102.mw3"));
  EXPECT(!ace->record_mission(true));
  auto* g3 = dynamic_cast<mech3::ModCampaignGame*>(ace->current);
  EXPECT(g3 && g3->mission == 3 && g3->operation() == 4 && g3->mission_number() == 5 && g3->world_index() == 4);
  EXPECT(g3 && std::string(g3->name()) == "Op B, Mission 1");
  // A saved game restores its world, except at a world's start (a new world here).
  h.loaded.clear();
  if (g2 && g3) {
    g2->b0 = 0, g3->b0 = 0;
    EXPECT(g2->restore() && h.loaded.size() == 1);
    EXPECT(!g3->restore() && h.loaded.size() == 1);
  }

  // Out again: the original's pilots back, with the original's games.
  mech3::campaign_leave();
  EXPECT(mech3::entered_campaign() == nullptr && mech3::pilots_folder() == "pilots");
  EXPECT(mech3::find_pilot("Orig") != nullptr && mech3::find_pilot("Ace") == nullptr);
  if (mech3::Pilot* o = mech3::find_pilot("Orig")) EXPECT(dynamic_cast<mech3::ModCampaignGame*>(o->campaigns[0]) == nullptr);
  // And in: Ace with its saved games on the list.
  EXPECT(mech3::campaign_enter(c, &why));
  ace = mech3::find_pilot("Ace");
  EXPECT(ace && mech3::find_pilot("Orig") == nullptr && ace->campaign_count() == 3);
  if (ace && ace->campaign_count() == 3) {
    auto* back = dynamic_cast<mech3::ModCampaignGame*>(ace->campaigns[2]);
    EXPECT(back && back->mission == 3 && back->mission_number() == 5);
  }
  // Its own removal while entered: refused; a pilot's delete takes its files only.
  EXPECT(!mech3::campaign_pilots_remove(c, [] { return true; }, &why) && why == "the campaign is entered");
  mech3::delete_pilot("Ace");
  EXPECT(!fs::exists(own / "Ace.mw3") && !fs::exists(own / "games/Ace0102.mw3") && fs::is_regular_file(own / mech3::kCampaignMarker));
  mech3::create_pilot("Bea");
  mech3::campaign_leave();

  // The player's removal: only on yes, only with our marker.
  int asked = 0;
  EXPECT(!mech3::campaign_pilots_remove(c, [&] { return ++asked, false; }, &why) && why == "not confirmed" && asked == 1);
  EXPECT(fs::is_regular_file(own / "Bea.mw3"));
  EXPECT(mech3::campaign_pilots_remove(c, [&] { return ++asked, true; }, &why) && asked == 2 && !fs::exists(own));
  EXPECT(fs::is_directory(fs::path(wd) / "pilots/mods/m") && fs::is_regular_file(fs::path(wd) / "pilots/Orig.mw3"));
  EXPECT(!mech3::campaign_pilots_remove(c, [] { return true; }, &why) && why == "no pilots for it");

  // A folder there the game did not make: never entered, never removed.
  mech3::ModCampaign other = c;
  other.id = "other";
  const fs::path foreign = fs::path(wd) / "pilots/mods/m/other";
  fs::create_directories(foreign);
  std::ofstream(foreign / "keep.txt") << "mine";
  EXPECT(!mech3::campaign_enter(other, &why) && why.find("holds files the game did not put there") != std::string::npos);
  EXPECT(mech3::entered_campaign() == nullptr && mech3::pilots_folder() == "pilots" && mech3::find_pilot("Orig") != nullptr);
  asked = 0;
  EXPECT(!mech3::campaign_pilots_remove(other, [&] { return ++asked, true; }, &why) && asked == 0 &&
         why.find("has no mw3-campaign.txt") != std::string::npos);
  EXPECT(fs::is_regular_file(foreign / "keep.txt"));
  // An empty one is taken (and marked).
  fs::remove(foreign / "keep.txt");
  EXPECT(mech3::campaign_enter(other, &why) && fs::is_regular_file(foreign / mech3::kCampaignMarker));
  mech3::campaign_leave();

  drop_all();
  mech3::set_pilot_hooks(nullptr);
}
