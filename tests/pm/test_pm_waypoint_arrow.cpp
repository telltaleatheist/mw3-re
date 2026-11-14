// Pirate's Moon's selected-waypoint arrow (0x0047c1f0, the port's Waynet::hud_arrow with the table's
// waypoint_arrow_view): the main camera's view is set again (0x004b9620, MW3's 0x004b51f0) before the point is
// projected; MW3's 0x00479ef0 projects with the view last set.
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zgame/zgame_table.h"
#include "mech3/mech_waynet.h"
#include "pm/pm_game.h"

namespace {
struct Log : mech3::WaynetHooks {
  std::vector<std::string> calls;
  uint32_t unported(const char*, std::initializer_list<zgame::Arg>) override { return 0; }
  void view_prepare() override { calls.push_back("view"); }
  uint32_t project(const float[3], float out[3]) override {
    calls.push_back("project");
    out[0] = out[1] = 100.0f, out[2] = 0.5f;
    return 1;  // off the left: the arrow at the left edge
  }
  void arrow_hide() override {}
  void arrow_image(mech3::Handle) override {}
  void arrow_size(int32_t* w, int32_t* h) override { *w = *h = 8; }
  void arrow_place(int32_t, int32_t) override {}
  void arrow_show() override {}
  int32_t screen_w() override { return 640; }
  int32_t screen_h() override { return 480; }
};
}  // namespace

TEST(pm_waypoint_arrow_sets_the_main_view_before_projecting) {
  const float pos[3] = {10.0f, 0.0f, 20.0f};
  for (const bool pm : {true, false}) {
    zgame::set_game(pm ? pm::table() : zgame::mw3_table());
    Log log;
    mech3::set_waynet_hooks(&log);
    {
      mech3::Waynet w(10090, "nav");
      auto* p = new mech3::WaynetPoint("p", "a", pos, nullptr, 0, 0, 0, 1, 0, 0);
      w.add(p);
      w.current = 1;  // element 0 is the default behaviour
      w.hud_arrow(true);
    }
    mech3::set_waynet_hooks(nullptr);
    std::string seen;
    for (const std::string& c : log.calls) seen += " " + c;
    std::printf("  %s:%s\n", pm ? "PM" : "MW3", seen.c_str());
    EXPECT(seen == (pm ? " view project" : " project"));
  }
  zgame::set_game(pm::table());
}
