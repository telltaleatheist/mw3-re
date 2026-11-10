#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zgame/zgame_table.h"
#include "gamez/zutil/zfix.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/pilot.h"
#include "pm/pm_game.h"

// pm-tests item 9: a mission on a fresh world (0x00435e40) gets no world of the last one. The win from mission 1 writes
// mission 2's (the same world, c1); the win from mission 2 writes none for mission 3 (c2), with the fix or without
// (next_mission's own rule). And a game for mission 3 whose file is there (the port's builds before 2026-11-08 wrote
// one) reads nothing back.
namespace {
struct FreshHooks : mech3::PilotHooks {
  std::vector<std::string> saved;
  int32_t loaded = 0;
  uint32_t unported(const char*, std::initializer_list<zgame::Arg>) override { return 0; }
  int32_t message_box(int32_t, int32_t, int32_t) override { return 1; }
  void save_world(const char* path, int32_t) override { saved.push_back(path ? path : ""); }
  bool load_world(const char*, int32_t) override { return ++loaded, true; }
};
}  // namespace
TEST(pm_campaign_fresh_world_keeps_no_world) {
  zutl::mounts().read.clear();
  const std::string dir = std::string(MW3_TEST_TMP_ROOT) + "/test_pm_fresh_world";
  std::filesystem::create_directories(dir + "/pilots/games");
  zutl::set_write_dir(dir.c_str());
  zgame::set_game(pm::table());
  FreshHooks h;
  mech3::set_pilot_hooks(&h);
  for (const bool fix : {true, false}) {
    zfix::set_all(fix);
    mech3::Pilot* p = mech3::create_pilot("Fresh");
    EXPECT(p && p->select_campaign(0));
    h.saved.clear();
    p->record_mission(true);  // mission 1 won: mission 2, on c1 still
    EXPECT(h.saved.size() == 1);
    h.saved.clear();
    p->record_mission(true);  // mission 2 won: mission 3, on c2
    auto* g = dynamic_cast<mech3::CampaignGame*>(p->current);
    EXPECT(g && g->mission_number() == 3);
    EXPECT(h.saved.empty());
    if (g) {
      std::FILE* f = std::fopen((dir + "/" + g->path()).c_str(), "wb");  // an old build's file
      if (f) std::fclose(f);
      h.loaded = 0;
      g->b0 = 0;
      EXPECT(!g->restore() && h.loaded == 0);
    }
    while (!mech3::pilots().empty()) mech3::delete_pilot(mech3::pilots().front()->name);
  }
  zfix::set_all(true);
  mech3::set_pilot_hooks(nullptr);
  zgame::set_game(zgame::mw3_table());
}
