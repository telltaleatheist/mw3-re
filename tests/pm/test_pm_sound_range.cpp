// Pirate's Moon's sound range at the lowest object detail (0x00482ca0 at start-up, 0x00408120
// at each briefing, both through 0x00591830; docs/pm-tests.md): mw3pm started headless with
// ObjectLOD 2 in its settings hears every sound to half the distance it does at 0.
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>

#include "check.h"
#include "gamez/zgame/zgame_table.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_states.h"
#include "mech3/options.h"
#include "platform/reg_file.h"
#include "platform/silent_sound.h"
#include "pm/pm_game.h"

namespace {
namespace fs = std::filesystem;

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
struct Screen : platform::Screen2D {
  void present_16(const uint16_t*, int, int, int, bool) override {}
};
struct Pump : platform::MessagePump {
  int passes = 0;
  const bool* stop = nullptr;
  bool peek() override { return ++passes > 400 || (stop && *stop); }
  bool pump() override { return false; }
  void wait(uint32_t) override {}
  void post_quit() override {}
};
struct MountsBack {
  zutl::Mounts was = zutl::mounts();
  ~MountsBack() { zutl::mounts() = was; }
};

struct Heard {
  float scale = 0.0f;
  std::map<std::string, std::pair<float, float>> ranges;  // set/sound: near, far
};

// mw3pm's start with ObjectLOD_SW and _HW set to `lod`: the range scale and every sound's range.
Heard start_at(int lod) {
  std::error_code ec;
  MountsBack back;
  zgame::set_game(pm::table());
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_sound_range_" + std::to_string(lod);
  fs::remove_all(wd, ec);
  fs::create_directories(wd, ec);
  {
    const std::string text = "Windows Registry Editor Version 5.00\r\n\r\n"
                             "[HKEY_CURRENT_USER\\Software\\MicroProse\\MechWarrior 3 EP1\\1.0]\r\n"
                             "\"ObjectLOD_SW\"=dword:0000000" + std::to_string(lod) + "\r\n"
                             "\"ObjectLOD_HW\"=dword:0000000" + std::to_string(lod) + "\r\n";
    std::FILE* f = std::fopen((wd + "/settings.reg").c_str(), "wb");
    std::fputc(0xff, f), std::fputc(0xfe, f);
    for (char c : text) std::fputc(c, f), std::fputc(0, f);
    std::fclose(f);
  }
  platform::RegFile settings(wd + "/settings.reg");
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd.c_str());
  zutl::mount(MW3PM_GAME_DIR);
  mech3::reset_states();
  mech3::options() = mech3::Options{};
  Window window;
  Alert alert;
  Clock clock, movies;
  Screen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.movie_clock = &movies, p.screen = &screen;
  p.sound = &sound, p.files = &files, p.settings = &settings;
  Heard h;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    bool seen = false;
    engine.on_present = [&](int) {  // the menus up: the start-up's sounds read
      if (seen) return;
      seen = true;
      h.scale = zsnd::state().range_scale;
      for (zsnd::Set* s : zsnd::state().sets)
        for (zsnd::Sound& x : s->sounds) h.ranges[s->name + "/" + (x.name ? x.name : "")] = {x.range_min, x.range_max};
    };
    Pump pump;
    pump.stop = &seen;
    app.run(pump);
    std::printf("  lod %d: %d passes, seen %d\n", lod, pump.passes, seen);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  mech3::options() = mech3::Options{};
  return h;
}
}  // namespace

TEST(pm_sound_range_halved_at_the_lowest_object_detail_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const Heard full = start_at(0), low = start_at(2);
  int same = 0, halved = 0, other = 0;
  for (const auto& [name, r] : full.ranges) {
    auto it = low.ranges.find(name);
    if (it == low.ranges.end()) continue;
    if (it->second.first == 0.5f * r.first && it->second.second == 0.5f * r.second && r.second != 0.0f) ++halved;
    else if (it->second == r) ++same;
    else ++other;
  }
  std::printf("  scale %g / %g; %zu sounds: %d halved, %d the same, %d other\n", full.scale, low.scale,
              full.ranges.size(), halved, same, other);
  EXPECT(full.scale == 1.0f && low.scale == 0.5f);
  EXPECT(halved > 0 && same == 0 && other == 0);
}
