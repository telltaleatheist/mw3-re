// Pirate's Moon's Salvage State enter (0x0047f7c0, the port's SalvageState::enter with the table's
// salvage_loads_world): with no world up it loads the mech lab's world (6, the mission load 0x00492830) before
// making the screen; with a world up, or on MW3's table (0x0047d310), the screen alone.
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zgame/zgame_table.h"
#include "mech3/app.h"
#include "mech3/app_states.h"
#include "mech3/options.h"
#include "pm/pm_game.h"

namespace {
struct Log : mech3::Engine {
  std::vector<std::string> calls;
  bool up = false;
  uint32_t unported(const char*, std::initializer_list<zgame::Arg>) override { return 0; }
  bool world_up() override { return up; }
  void load_lab_world() override { calls.push_back("world 6"); }
  mech3::Handle construct(uint32_t ctor, std::initializer_list<zgame::Arg>) override {
    calls.push_back(ctor == 0x47d3b0 ? "screen" : "other");
    return 0;
  }
};
}  // namespace

TEST(pm_salvage_loads_the_lab_world_when_none_is_up) {
  struct Case { bool pm, up; const char* want; };
  const Case cases[] = {{true, false, " world 6 screen"}, {true, true, " screen"}, {false, false, " screen"}};
  for (const Case& c : cases) {
    zgame::set_game(c.pm ? pm::table() : zgame::mw3_table());
    Log engine;
    engine.up = c.up;
    mech3::reset_states();
    mech3::options() = mech3::Options{};
    auto* app = new mech3::Mech3App(&engine);
    mech3::set_app(app);
    mech3::states().salvage.enter();
    std::string seen;
    for (const std::string& s : engine.calls) seen += " " + s;
    std::printf("  %s, world %s:%s\n", c.pm ? "PM" : "MW3", c.up ? "up" : "none", seen.c_str());
    EXPECT(seen == c.want);
    mech3::set_app(nullptr);
    delete app;
  }
  zgame::set_game(pm::table());
}
