// Our option records on Pirate's Moon (patch12::apply_ours; docs/menus.md):
// PM's dlgOptions.zrd is wrapped ("3:33" "PM" "9/13/1999" (the dialogs)), so
// MOUSE SENSITIVITY, UPGRADES and its page went in nowhere and mw3pm logged
// "BindButton (UPGRADES) failed". Its records read, and PM's options screen
// bound and its UPGRADES page opened. MW3's: test_program.cpp's options_*.
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "check.h"
#include "gamez/zgame/zgame_table.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zfix.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_states.h"
#include "mech3/mainmenu.h"
#include "mech3/menu_states.h"
#include "mech3/mission.h"
#include "mech3/options.h"
#include "mech3/options_menu.h"
#include "mech3/patch12.h"
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
struct MovieClock : platform::Clock {
  uint32_t now = 0;
  uint32_t ticks_ms() override { return now += 1000; }
};
struct Screen : platform::Screen2D {
  void present_16(const uint16_t*, int, int, int, bool) override {}
};
struct Pump : platform::MessagePump {
  int passes = 0, limit = 400;
  std::function<bool()> stop;
  bool peek() override { return ++passes > limit || (stop && stop()); }
  bool pump() override { return false; }
  void wait(uint32_t) override {}
  void post_quit() override {}
};
struct MountsBack {
  zutl::Mounts was = zutl::mounts();
  ~MountsBack() { zutl::mounts() = was; }
};

std::string font_of(zrdr::Node* r, const char* button) {
  zrdr::Node* b = zrdr::find(r, button);
  zrdr::Node* label = b ? zrdr::find(b, "LABEL") : nullptr;
  zrdr::Node* f = label ? zrdr::find(label, "font") : nullptr;
  return f && f->type == zrdr::List && f->list[0].i > 1 && f->list[1].type == zrdr::String && f->list[1].s ? f->list[1].s : "";
}
}  // namespace

// PM's own archive: the three records go in, inside the wrapper beside its
// dialogs, in PM's small font (ia_small_amber).
TEST(pm_options_our_records_go_into_pms_dialogs) {
  std::error_code ec;
  const fs::path reader = fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd";
  if (!fs::exists(reader, ec)) return;
  zgame::set_game(pm::table());
  zrdr::init();
  zrdr::close_archives(true);
  EXPECT(zrdr::open_archive(fs::canonical(reader, ec).string().c_str(), true));
  zrdr::Node* r = zrdr::load("dlgoptions.zrd");
  EXPECT(r != nullptr);
  if (r) {
    EXPECT(zrdr::find(r, "UPGRADES_DIALOG") == nullptr && zrdr::find(r, "MOUSE_SENSITIVITY") == nullptr);
    EXPECT(mech3::patch12::apply_ours("zbd/reader.zbd", "dlgoptions.zrd", r));
    zrdr::Node* game = zrdr::find(r, "GAME_OPTION_DIALOG");
    zrdr::Node* buttons = game ? zrdr::find(game, "BUTTONS") : nullptr;
    std::printf("  UPGRADES %s, BUG_FIXES %s, slider %d, page %d\n", font_of(r, "UPGRADES").c_str(), font_of(r, "BUG_FIXES").c_str(),
                buttons && zrdr::find(buttons, "MOUSE_SENSITIVITY") != nullptr, zrdr::find(r, "UPGRADES_DIALOG") != nullptr);
    EXPECT(buttons && zrdr::find(buttons, "MOUSE_SENSITIVITY") && zrdr::find(buttons, "UPGRADES"));
    EXPECT(font_of(r, "UPGRADES") == "ia_small_amber" && font_of(r, "BUG_FIXES") == "ia_small_amber");
    // The page beside the game's dialogs, inside the wrapper, not at its top.
    EXPECT(r->type == zrdr::List && zrdr::find(r, "UPGRADES_DIALOG") != nullptr);
    bool at_top = false;
    for (int32_t i = 1; i < r->list[0].i; ++i)
      at_top |= r->list[i].type == zrdr::String && r->list[i].s && std::strcmp(r->list[i].s, "UPGRADES_DIALOG") == 0;
    EXPECT(!at_top);
    zrdr::free_node(r);
  }
  zrdr::close_archives(true);
}

// mw3pm's options as the program runs them: MAIN MENU > OPTIONS binds MOUSE
// SENSITIVITY and UPGRADES, and UPGRADES opens its page.
TEST(pm_options_screen_binds_ours_and_opens_upgrades_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  MountsBack back;
  zgame::set_game(pm::table());
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_options_screen";
  fs::remove_all(wd, ec);
  fs::create_directories(wd, ec);
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd.c_str());
  zutl::mount(MW3PM_GAME_DIR);
  mech3::reset_states();
  mech3::options() = mech3::Options{};
  Window window;
  Alert alert;
  Clock clock;
  MovieClock movies;
  Screen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.movie_clock = &movies, p.screen = &screen;
  p.sound = &sound, p.files = &files;
  int stage = 0;
  bool slider = false, upgrades = false, page = false;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    engine.on_present = [&](int) {
      auto* os = dynamic_cast<mech3::OptionsState*>(app.current());
      auto* g = os ? dynamic_cast<mech3::GameOptionsScreen*>(engine.screen_of(os->screen_)) : nullptr;
      auto* u = os ? dynamic_cast<mech3::UpgradesScreen*>(engine.screen_of(os->screen_)) : nullptr;
      if (stage == 0 && app.current() == &mech3::states().main && mech3::main_menu()) {
        mech3::main_menu()->buttons[mech3::MainMenuButton::kOptions].on_left_up();
        stage = 1;
      } else if (stage == 1 && g) {
        slider = g->has_sensitivity, upgrades = g->has_upgrades;
        if (upgrades) g->upgrades.on_left_up();
        stage = 2;
      } else if (stage == 2 && u) {
        page = true;
        stage = 3;
      }
    };
    Pump pump;
    pump.stop = [&] { return stage == 3; };
    app.run(pump);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  mech3::options() = mech3::Options{};
  std::printf("  stage %d: slider %d, UPGRADES %d, page %d\n", stage, slider, upgrades, page);
  EXPECT(slider && upgrades && page && stage == 3);
}

// A mode picked in the menus takes hold in them at once, on Pirate's Moon as
// on MechWarrior 3 (test_program.cpp's menus_take_a_picked_mode_at_once_headless;
// resolution.md section 12): from a saved 4:3 mode, GRAPHICS' ACCEPT with
// 1920x1080 makes the menus again on the wide canvas, the main menu up; the
// same mode again goes back to OPTIONS as the exe does; 640x480 makes them
// 4:3 again. OPTIONS opens after each.
TEST(pm_menus_take_a_picked_mode_at_once_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  MountsBack back;
  zgame::set_game(pm::table());
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_menu_reshape";
  fs::remove_all(wd, ec);
  fs::create_directories(wd, ec);
  {  // a saved InGameVMode of 5 (640x480) under PM's key, as regedit writes it
    const char text[] =
        "Windows Registry Editor Version 5.00\r\n\r\n"
        "[HKEY_CURRENT_USER\\Software\\MicroProse\\MechWarrior 3 EP1\\1.0]\r\n"
        "\"InGameVMode\"=dword:00000005\r\n";
    std::FILE* f = std::fopen((wd + "/settings.reg").c_str(), "wb");
    std::fputc(0xff, f), std::fputc(0xfe, f);
    for (const char* c = text; *c; ++c) std::fputc(*c, f), std::fputc(0, f);
    std::fclose(f);
  }
  platform::RegFile settings(wd + "/settings.reg");
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd.c_str());
  zutl::mount(MW3PM_GAME_DIR);
  mech3::reset_states();
  mech3::options() = mech3::Options{};
  const zfix::Options fix_was = zfix::options();
  zfix::options().any_resolution = true, zfix::options().wide_menus = true;
  Window window;
  Alert alert;
  Clock clock;
  MovieClock movies;
  Screen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.movie_clock = &movies, p.screen = &screen;
  p.sound = &sound, p.files = &files, p.settings = &settings;
  struct Seen {
    bool to_main = false;
    int32_t width = 0, mode = 0, saved = 0;
  };
  const int32_t picks[] = {13, 13, 5};
  std::vector<Seen> seen;
  int32_t width0 = 0, mode0 = 0;
  enum { kMain, kOptions, kPage, kAfter, kDone } stage = kMain;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    EXPECT(mech3::options().in_game_vmode == 5);
    size_t k = 0;
    int wait = 0;
    bool first = true;
    Seen cur;
    engine.on_present = [&](int) {
      zgame::State* now = app.current();
      auto* os = dynamic_cast<mech3::OptionsState*>(now);
      auto* game = os ? dynamic_cast<mech3::GameOptionsScreen*>(engine.screen_of(os->screen_)) : nullptr;
      const bool at_main = now == &mech3::states().main && mech3::main_menu();
      if (app.pending()) return;
      if (stage == kMain && at_main) {
        cur.width = engine.buffer_width(), cur.mode = engine.vmode(), cur.saved = mech3::options().in_game_vmode;
        mech3::main_menu()->buttons[mech3::MainMenuButton::kOptions].on_left_up();
        stage = kOptions;
      } else if (stage == kOptions && game) {  // OPTIONS opens on the menus as they are now
        if (first) width0 = cur.width, mode0 = cur.mode, first = false;
        else seen.push_back(cur);
        if (seen.size() == std::size(picks)) return void(stage = kDone);
        cur = Seen{};
        game->gfx_audio.on_left_up();
        stage = kPage;
      } else if (stage == kPage && os && !game) {
        auto* g = dynamic_cast<mech3::GfxAudioScreen*>(engine.screen_of(os->screen_));
        if (!g) return;
        g->resolution.select_each_value(picks[k]);
        EXPECT(g->resolution.selected_value() == picks[k]);
        g->accept.on_left_up();
        ++k, wait = 0, stage = kAfter;
      } else if (stage == kAfter && ++wait > 3) {
        if (at_main) {
          cur.to_main = true, stage = kMain;
        } else if (game) {  // back in OPTIONS (nothing changed): its BACK to the main menu
          game->back.on_left_up();
          stage = kMain;
        }
      }
    };
    Pump pump;
    pump.limit = 3000;
    pump.stop = [&] { return stage == kDone; };
    app.run(pump);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zfix::options() = fix_was;
  zrdr::close_archives(true);
  mech3::reset_states();
  mech3::options() = mech3::Options{};
  std::printf("  PM's menus at the start: canvas %d, mode %d\n", width0, mode0);
  for (size_t i = 0; i < seen.size(); ++i)
    std::printf("  after %d: %s, canvas %d, mode %d (saved %d)\n", picks[i], seen[i].to_main ? "the main menu made again" : "back in OPTIONS",
                seen[i].width, seen[i].mode, seen[i].saved);
  EXPECT(stage == kDone && width0 == 640 && mode0 == 5 && seen.size() == std::size(picks));
  if (seen.size() != std::size(picks)) return;
  EXPECT(seen[0].to_main && seen[0].width == 853 && seen[0].mode == 13 && seen[0].saved == 13);   // 4:3 to 16:9
  EXPECT(!seen[1].to_main && seen[1].width == 853 && seen[1].mode == 13 && seen[1].saved == 13);  // the same again
  EXPECT(seen[2].to_main && seen[2].width == 640 && seen[2].mode == 5 && seen[2].saved == 5);     // 16:9 to 4:3
}
