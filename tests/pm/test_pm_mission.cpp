// Pirate's Moon missions run headless with the AI on, for what the AI does in play.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zgame/zgame_table.h"
#include "pm/pm_game.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_states.h"
#include "mech3/app_mech.h"
#include "mech3/mission.h"
#include "mech3/app_engine.h"
#include "mech3/app_states.h"
#include "mech3/options.h"
#include "mech3/zai_valve.h"
#include "mech3/zrdrvalve.h"
#include "platform/silent_sound.h"

namespace {
struct Window : platform::Window {
  uint32_t create(const std::string&, int, int) override { return 0x1234; }
  void set_title(const std::string&) override {}
  void show() override {}
  void client_size(int* w, int* h) override { *w = 640, *h = 480; }
};
struct Alert : platform::Alert {
  void abort_message(const std::string& m) override { std::fprintf(stderr, "abort: %s\n", m.c_str()); }
};
struct Clock : platform::Clock {
  uint32_t now = 0;
  uint32_t ticks_ms() override { return now += 16; }
};
struct MovieClock : platform::Clock {
  uint32_t now = 0;
  uint32_t ticks_ms() override { return now += 1000; }
};
struct Screen : platform::Screen2D {
  void present_16(const uint16_t*, int, int, int, bool) override {}
};
struct NoKeys : platform::Input {
  bool open(Device d) override { return d == kKeyboard; }
  void close(Device) override {}
  int32_t acquire(Device) override { return 0; }
  int32_t unacquire(Device) override { return 0; }
  int32_t keyboard_data(platform::KeyEvent*, uint32_t* count) override {
    *count = 0;
    return 0;
  }
  int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
  int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
};
struct Pump : platform::MessagePump {
  int passes = 0, limit = 200;
  bool peek() override { return ++passes > limit; }
  bool pump() override {
    mech3::missions().cleanup(false);
    return false;
  }
  void wait(uint32_t) override {}
  void post_quit() override {}
};
int32_t valve_value(const char* name) {
  const mech3::ai::Valve* v = mech3::ai::valve_find(name);
  return v ? v->value->get() : -1;
}
}  // namespace

// c1m1's two Shadow Cats start powered down; the data wakes them 180 s in, by `shadowcat_time`
// through `shadowcat_go`, or when the player's close. The first thing PM's AI was seen not to do.
TEST(pm_c1m1_shadow_cats_wake) {
  // A minute in an optimized build, several in the default: run with the campaign chains.
  if (!std::getenv("MW3_CAMPAIGN") || !std::filesystem::exists(MW3PM_GAME_DIR)) return;
  zgame::set_game(pm::table());
  zutl::mounts() = zutl::Mounts{};
  zutl::mount(MW3PM_GAME_DIR);
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_c1m1_write";
  std::filesystem::create_directories(wd);
  zutl::set_write_dir(wd.c_str());
  NoKeys keys;
  Window window;
  Alert alert;
  Clock clock;
  MovieClock movies;
  Screen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.movie_clock = &movies, p.screen = &screen;
  p.sound = &sound, p.files = &files, p.input = &keys;
  mech3::GameEngine engine(p, "mech3.exe -nofmv");
  engine.quiet = true;
  mech3::Mech3App app(&engine);
  mech3::set_app(&app);
  EXPECT(app.init_instance());
  mech3::GameEngine::Launch l;
  // PM_MISSION=index,type,number,frames: another mission for the same run, for looking at it.
  int limit = 16000;
  const char* other = std::getenv("PM_MISSION");
  if (other) std::sscanf(other, "%d,%d,%d,%d", &l.index, &l.type, &l.number, &limit);
  engine.set_launch(l);
  app.post_intro_ = &mech3::states().load;
  int frames = 0, up0 = -1, up1 = -1, go = -1;
  void* cats[2] = {};
  engine.on_present = [&](int) {
    if (app.current() != &mech3::states().core) return;
    if (++frames == 3) {
      int32_t made = 0;
      const mech3::AiMechMade* ai = mech3::mission_ai_mechs(&made);
      for (int32_t i = 0; i < made; ++i) {
        if (ai[i].name && !std::strcmp(ai[i].name, "shadowcat_01")) cats[0] = ai[i].mech;
        if (ai[i].name && !std::strcmp(ai[i].name, "shadowcat_02")) cats[1] = ai[i].mech;
      }
      std::printf("  c1m1: %d AI mechs, shadow cats %p %p\n", made, cats[0], cats[1]);
    }
    if (frames < 3 || (!cats[0] && !cats[1])) return;
    mech3::LaunchedMech a, b;
    if (cats[0]) mech3::describe_mech(cats[0], &a);
    if (cats[1]) mech3::describe_mech(cats[1], &b);
    if (go < 0 && valve_value("shadowcat_go") > 0) go = frames;
    if (up0 < 0 && cats[0] && !a.powered_down) up0 = frames;
    if (up1 < 0 && cats[1] && !b.powered_down) up1 = frames;
    if (frames % 1000 == 0)
      std::printf("  frame %d (%.0f s): go %d time %d close %d; down %d %d, asked %d %d\n", frames, mech3::ai::valve_hooks().game_time(),
                  valve_value("shadowcat_go"), valve_value("shadowcat_time"), valve_value("shadowcat_close"), a.powered_down,
                  b.powered_down, a.powering_down, b.powering_down);
  };
  Pump pump;
  pump.limit = limit;
  app.run(pump);
  std::printf("  frames %d: go at %d, up at %d and %d\n", frames, go, up0, up1);
  zgame::set_game(zgame::mw3_table());
  if (other) return;
  EXPECT(cats[0] && cats[1]);
  EXPECT(go > 0);
  EXPECT(up0 > 0 && up1 > 0);
}
