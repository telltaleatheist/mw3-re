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
#include "mech3/pilot_menu.h"
#include "mech3/patch12.h"
#include "platform/reg_file.h"
#include "platform/silent_sound.h"
#include "pm/pm_game.h"
#include "upgrades/mods/entry_run.h"
#include "upgrades/settings/upgrades.h"

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
    std::printf("  UPGRADES %s, SMALL_FIXES %s, slider %d, page %d\n", font_of(r, "UPGRADES").c_str(), font_of(r, "SMALL_FIXES").c_str(),
                buttons && zrdr::find(buttons, "MOUSE_SENSITIVITY") != nullptr, zrdr::find(r, "UPGRADES_DIALOG") != nullptr);
    EXPECT(buttons && zrdr::find(buttons, "MOUSE_SENSITIVITY") && zrdr::find(buttons, "UPGRADES"));
    EXPECT(font_of(r, "UPGRADES") == "ia_small_amber" && font_of(r, "SMALL_FIXES") == "ia_small_amber");
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
    // At the main menu: the stack's depth, whether the main menu's state is its bottom, the screens and
    // menu objects alive (each change frees the old ones: the main menu's state is replaced by itself).
    int depth = -1;
    bool bottom_is_main = false;
    size_t screens = 0, objects = 0;
  };
  std::vector<int32_t> picks = {13, 13, 5};
  for (int i = 0; i < 10; ++i) picks.push_back(13), picks.push_back(5);  // twenty more changes in a row
  Seen start;
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
        cur.depth = app.depth(), cur.bottom_is_main = app.stack(0) == &mech3::states().main;
        cur.screens = engine.open_screens(), cur.objects = engine.menu_objects();
        mech3::main_menu()->buttons[mech3::MainMenuButton::kOptions].on_left_up();
        stage = kOptions;
      } else if (stage == kOptions && game) {  // OPTIONS opens on the menus as they are now
        if (first) width0 = cur.width, mode0 = cur.mode, start = cur, first = false;
        else seen.push_back(cur);
        if (seen.size() == picks.size()) return void(stage = kDone);
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
    pump.limit = 9000;
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
  for (size_t i = 0; i < seen.size() && i < 3; ++i)
    std::printf("  after %d: %s, canvas %d, mode %d (saved %d)\n", picks[i], seen[i].to_main ? "the main menu made again" : "back in OPTIONS",
                seen[i].width, seen[i].mode, seen[i].saved);
  EXPECT(stage == kDone && width0 == 640 && mode0 == 5 && seen.size() == picks.size());
  if (seen.size() != picks.size()) return;
  // The main menu's state alone on the stack after every step, the screens and menu objects as many as at the start.
  std::printf("  at the start: depth %d, %zu screens, %zu menu objects; after %zu steps: depth %d, %zu screens, %zu menu objects\n", start.depth,
              start.screens, start.objects, seen.size(), seen.back().depth, seen.back().screens, seen.back().objects);
  EXPECT(start.depth == 0 && start.bottom_is_main && start.screens > 0 && start.objects > 0);
  int deep = 0, grown = 0;
  for (const Seen& r : seen) deep += r.depth != 0 || !r.bottom_is_main, grown += r.screens != start.screens || r.objects != start.objects;
  EXPECT(deep == 0 && grown == 0);
  for (size_t i = 3; i + 1 < seen.size(); i += 2)
    EXPECT(seen[i].to_main && seen[i].width == 853 && seen[i].mode == 13 && seen[i + 1].to_main && seen[i + 1].width == 640 && seen[i + 1].mode == 5);
  EXPECT(seen[0].to_main && seen[0].width == 853 && seen[0].mode == 13 && seen[0].saved == 13);   // 4:3 to 16:9
  EXPECT(!seen[1].to_main && seen[1].width == 853 && seen[1].mode == 13 && seen[1].saved == 13);  // the same again
  EXPECT(seen[2].to_main && seen[2].width == 640 && seen[2].mode == 5 && seen[2].saved == 5);     // 16:9 to 4:3
}

// Mods by campaign on Pirate's Moon (upgrades/mods/entry_run.h; MainState::update): a run started into an entry
// with --entry-play opens that entry's own game from the main menu's state: the Campaign's entry its Campaign
// State, the Pirate Missions' entry ("pirate") their Standalone State (the main menu's training slot on PM),
// Instant Action's its Action State. (The pilot screen each shows first sits on top of it.)
TEST(pm_entry_play_opens_each_entrys_own_state_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const mech3::EntryRun keep = mech3::entry_run();
  struct Want {
    const char* id;
    mech3::EntryRun::Screen screen;
    mech3::GameMenuState::Kind kind;
  };
  const Want wants[] = {{"campaign", mech3::EntryRun::kPlay, mech3::GameMenuState::kCampaign},
                        {"pirate", mech3::EntryRun::kPlay, mech3::GameMenuState::kStandalone},
                        {"ia", mech3::EntryRun::kInstantAction, mech3::GameMenuState::kAction}};
  for (const Want& w : wants) {
    MountsBack back;
    zgame::set_game(pm::table());
    const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_entry_play_" + w.id;
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
    int found = -1, others = 0;
    {
      mech3::GameEngine engine(p, "mech3.exe -nofmv");
      engine.quiet = true;
      mech3::Mech3App app(&engine);
      mech3::set_app(&app);
      EXPECT(app.init_instance());
      mech3::entry_run() = mech3::EntryRun{};
      mech3::entry_run().screen = w.screen, mech3::entry_run().id = w.id;
      engine.on_present = [&](int) {
        if (found >= 0) return;
        for (int i = 0; i <= app.depth(); ++i)
          if (auto* g = dynamic_cast<mech3::GameMenuState*>(app.stack(i))) {
            if (g->kind == w.kind) found = i;
            else ++others;
          }
      };
      Pump pump;
      pump.stop = [&] { return found >= 0 || others > 0; };
      app.run(pump);
      mech3::entry_run() = mech3::EntryRun{};  // (its leave must not run as the app goes)
      app.stop_game();
      app.exit_instance();
      mech3::set_app(nullptr);
    }
    zrdr::close_archives(true);
    mech3::reset_states();
    mech3::options() = mech3::Options{};
    std::printf("  entry %s: its state on the stack at %d (another game's first: %d)\n", w.id, found, others);
    EXPECT(found >= 0 && others == 0);
    fs::remove_all(wd, ec);
  }
  mech3::entry_run() = keep;
}

// A mission's return inside an entry on Pirate's Moon (upgrades/mods/entry_run.h): the game comes back to the
// main menu's state (a pause's QUIT, a debriefing's ABORT: replace_state(2)), which opens the entry's screen
// again; one BACK from it then leaves (the restart asked), never a second screen first. The Pirate Missions'
// return sets no flag for the main menu to act on: PM's main menu (0x00489380) presses a button on entry only
// for Instant Action's flag (0x0062cc50), the training slot's (0x0062cc52, set by the pause 0x00496270 for a
// game of kind 4 and by the training's end 0x00494e70, never for the Pirate Missions' kind 5) and the credits'.
// Instant Action's return comes with its flag, as its end sets it (IaDummyState).
TEST(pm_entry_mission_return_reopens_the_screen_and_one_back_leaves_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  const mech3::EntryRun keep = mech3::entry_run();
  struct Want {
    const char* id;
    mech3::EntryRun::Screen screen;
    mech3::GameMenuState::Kind kind;
  };
  const Want wants[] = {{"pirate", mech3::EntryRun::kPlay, mech3::GameMenuState::kStandalone},
                        {"campaign", mech3::EntryRun::kPlay, mech3::GameMenuState::kCampaign},
                        {"ia", mech3::EntryRun::kInstantAction, mech3::GameMenuState::kAction}};
  for (const Want& w : wants) {
    MountsBack back;
    zgame::set_game(pm::table());
    const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_entry_return_" + w.id;
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
    int stage = 0, leaves = 0, leaves_before_back = -1, opens = 0, others = 0, wait = 0;
    bool returned = false;  // the stack was down to the main menu's state after the return
    {
      mech3::GameEngine engine(p, "mech3.exe -nofmv");
      engine.quiet = true;
      mech3::Mech3App app(&engine);
      mech3::set_app(&app);
      EXPECT(app.init_instance());
      mech3::entry_run() = mech3::EntryRun{};
      mech3::entry_run().screen = w.screen, mech3::entry_run().id = w.id;
      mech3::entry_run().leave = [&] { ++leaves; };
      auto shown = [&] {  // the entry's own state on the stack, and no other game's
        bool own = false;
        for (int i = 0; i <= app.depth(); ++i)
          if (auto* g = dynamic_cast<mech3::GameMenuState*>(app.stack(i))) (g->kind == w.kind ? own : (++others, own)) = g->kind == w.kind || own;
        return own;
      };
      engine.on_present = [&](int) {
        if (app.pending()) return;
        if (stage == 0 && shown()) {  // opened at the start: now a mission's return to the main menu
          ++opens;
          // As a launch and a return do it: the screen's states popped and the main menu's state replaced in
          // one batch (it never updates between; a mission's states took its place), then replaced by itself
          // again at the mission's end (replace_state(2)): its leave, then its enter.
          if (w.screen == mech3::EntryRun::kInstantAction) app.set_reopen_instant_action(1, 1);  // as its end sets it
          for (int i = app.depth(); i > 0; --i) app.pop(1);
          app.replace(app.state(2), 0);
          stage = 1;
        } else if (stage == 1 && returned) {
          stage = 2;  // the main menu entered again
        } else if (stage == 2 && shown() && ++wait > 3) {  // opened again; BACK once: the screen's states popped
          ++opens;
          leaves_before_back = leaves;
          for (int i = app.depth(); i > 0; --i) app.pop(1);
          stage = 3;
        } else if (stage == 3 && (leaves > 0 || shown() || ++wait > 40)) {
          stage = 4;  // left, or (wrongly) a screen up again, or nothing
        }
      };
      Pump pump;
      pump.limit = 2000;
      pump.stop = [&] {
        if (stage == 1 && app.depth() == 0) returned = true;
        return stage == 4;
      };
      app.run(pump);
      const bool up_again = stage == 4 && leaves == 0;
      mech3::entry_run() = mech3::EntryRun{};
      app.stop_game();
      app.exit_instance();
      mech3::set_app(nullptr);
      std::printf("  entry %s: stage %d, opened %d times, left %d (before BACK %d), another game's state %d%s\n", w.id, stage, opens, leaves,
                  leaves_before_back, others, up_again ? ", NOT left after one BACK" : "");
    }
    zrdr::close_archives(true);
    mech3::reset_states();
    mech3::options() = mech3::Options{};
    EXPECT(stage == 4 && opens == 2 && leaves == 1 && leaves_before_back == 0 && others == 0);
    fs::remove_all(wd, ec);
  }
  mech3::entry_run() = keep;
}

// Pirate's Moon's UPGRADES page has the five switches as MechWarrior 3's (docs/fixes.md, The Upgrades switches):
// WATER MOVEMENT first (Pirate's Moon's own wading eased in, Owen 2026-11-10), MFB REGROUP, IMPROVED AI, ARTEMIS
// TRACKING, SMALL FIXES, then WIDE MENUS. ACCEPT saves the five and wide menus; the Small fixes list is Pirate's
// Moon's own; a host's word of five bits sets the five. WATER's default is asserted here and nowhere else.
TEST(pm_upgrades_page_has_five_switches_headless) {
  EXPECT(zfix::Options{}.water);  // the default: on, as MechWarrior 3's (zfix.h; the one place it is checked)
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  MountsBack back;
  zgame::set_game(pm::table());
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_upgrades_page";
  fs::remove_all(wd, ec);
  fs::create_directories(wd, ec);
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd.c_str());
  zutl::mount(MW3PM_GAME_DIR);
  mech3::reset_states();
  mech3::options() = mech3::Options{};
  const zfix::Options fix_was = zfix::options();
  const auto saver_was = mech3::options_upgrades_saver();
  std::vector<std::string> saved;
  mech3::options_upgrades_saver() = [&](const char* key, bool on) { saved.push_back(std::string(key) + (on ? "=1" : "=0")); };
  zfix::set_all(true);
  zfix::options().water = false;  // a player's upgrade_water=0
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
  int stage = 0, rows[6] = {-1, -1, -1, -1, -1, -1}, lines = -1;
  bool water_on_screen = false, water_shown_off = false, water_after = false, small_after = true;
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
      if (app.pending()) return;
      if (stage == 0 && app.current() == &mech3::states().main && mech3::main_menu()) {
        mech3::main_menu()->buttons[mech3::MainMenuButton::kOptions].on_left_up();
        stage = 1;
      } else if (stage == 1 && g) {
        g->upgrades.on_left_up();
        stage = 2;
      } else if (stage == 2 && u) {
        ztwod::Object2D* const row[6] = {&u->water, &u->mfb_regroup, &u->improved_ai, &u->artemis, &u->small_fixes, &u->wide_menus};
        for (int k = 0; k < 6; ++k) rows[k] = row[k]->get_y();
        water_on_screen = false;
        for (ztwod::Object2D* o = u->head; o; o = o->next) water_on_screen = water_on_screen || o == &u->water;
        water_shown_off = u->water.selected_value() == 0;  // the page reads the setting
        u->water.select(true), u->small_fixes.select(false);
        u->accept.on_left_up();  // the five and wide menus saved
        water_after = zfix::options().water, small_after = zfix::options().small_fixes;
        lines = u->small_list.add_item("(the count)", 0);
        stage = 3;
      }
    };
    Pump pump;
    pump.limit = 1500;
    pump.stop = [&] { return stage == 3; };
    app.run(pump);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  mech3::options() = mech3::Options{};
  mech3::options_upgrades_saver() = saver_was;
  std::string keys;
  for (const std::string& s : saved) keys += s + " ";
  std::printf("  rows at %d %d %d %d %d, wide menus %d (WATER on the screen %d); the list %d lines; saved: %s\n", rows[0], rows[1], rows[2],
              rows[3], rows[4], rows[5], water_on_screen, lines, keys.c_str());
  EXPECT(stage == 3 && rows[0] == 64 && rows[1] == 94 && rows[2] == 124 && rows[3] == 154 && rows[4] == 184 && rows[5] == 214);
  EXPECT(water_on_screen && water_shown_off);
  EXPECT(lines == 10 && upgrades::small_fixes_list().size() == 10);
  EXPECT(keys == "upgrade_water=1 upgrade_mfb_regroup=1 upgrade_improved_ai=1 upgrade_artemis=1 upgrade_small_fixes=0 widemenus=1 ");
  EXPECT(water_after && !small_after);  // each switch as the page left it
  // A host's word, the five bits (water the lowest), as MechWarrior 3's.
  zfix::set_all(true);
  upgrades::take_hosts(0x15);  // water, improved AI, small fixes
  const zfix::Options a = zfix::options();
  upgrades::give_back();
  upgrades::take_hosts(0x14);  // the same without water
  const zfix::Options b = zfix::options();
  upgrades::give_back();
  EXPECT(a.water && !a.mfb_regroup && a.improved_ai && !a.artemis && a.small_fixes);
  EXPECT(!b.water && !b.mfb_regroup && b.improved_ai && !b.artemis && b.small_fixes);
  EXPECT(zfix::all() && !upgrades::hosts());  // given back
  zfix::options() = fix_was;
  fs::remove_all(wd, ec);
}

// Pirate's Moon's option screens from a running mission (0x0042bb80, 0x00438c10, 0x00439a30): the
// background scrolls as in the menus, where MW3 stopped it (0x0047fb70); and the graphics and audio
// state (0x0043b7c0 / 0x0043b790) keeps the effects and music volumes on entering and puts them back
// on BACK, while ACCEPT keeps what the sliders set (MW3 keeps them either way).
TEST(pm_gfx_audio_back_undoes_the_volumes_and_backgrounds_scroll_in_a_mission_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  MountsBack back;
  zgame::set_game(pm::table());
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_gfx_back";
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
  enum { kMain, kOptions, kBack, kAfterBack, kAccept, kAfterAccept, kDone };
  int stage = kMain, scrolling = 0, screens = 0;
  uint8_t menu_moving = 0;
  float entered[2] = {}, after_back[2] = {}, after_accept[2] = {};
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    engine.on_present = [&](int) {
      zgame::State* now = app.current();
      auto* os = dynamic_cast<mech3::OptionsState*>(now);
      auto* game = os ? dynamic_cast<mech3::GameOptionsScreen*>(engine.screen_of(os->screen_)) : nullptr;
      auto* g = os ? dynamic_cast<mech3::GfxAudioScreen*>(engine.screen_of(os->screen_)) : nullptr;
      if (app.pending()) return;
      if (stage == kMain && now == &mech3::states().main && mech3::main_menu()) {
        zimg::Texture* img = nullptr;
        engine.main_menu_background(&img, &menu_moving);
        mech3::states().core.active_ = 1;  // as from the pause
        for (uint32_t ctor : {0x4379f0u, 0x438f60u}) {
          const mech3::Handle h = engine.construct(ctor, {0});
          if (auto* s = engine.screen_of(h)) ++screens, scrolling += s->background.moving == menu_moving;
          engine.vcall(h, 0x08, {1});
        }
        mech3::states().core.active_ = 0;
        mech3::main_menu()->buttons[mech3::MainMenuButton::kOptions].on_left_up();
        stage = kOptions;
      } else if ((stage == kOptions || stage == kAfterBack) && game) {
        game->gfx_audio.on_left_up();
        stage = stage == kOptions ? kBack : kAccept;
      } else if ((stage == kBack || stage == kAccept) && g) {
        if (stage == kBack) entered[0] = engine.sound_frequency(), entered[1] = engine.cd_volume();
        g->sfx.set_raw(0.25f), g->music.set_raw(0.75f);  // the sliders moved, each setting its volume
        engine.set_sound_frequency(g->sfx.value), engine.set_cd_volume(g->music.value);
        if (stage == kBack) g->back.on_left_up();
        else g->accept.on_left_up();
        stage = stage == kBack ? kAfterBack : kAfterAccept;
        if (stage == kAfterBack) after_back[0] = engine.sound_frequency(), after_back[1] = engine.cd_volume();
        else after_accept[0] = engine.sound_frequency(), after_accept[1] = engine.cd_volume(), stage = kDone;
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
  zrdr::close_archives(true);
  mech3::reset_states();
  mech3::options() = mech3::Options{};
  std::printf("  in a mission: %d of %d screens scroll as the menu (%d); volumes %g %g, after BACK %g %g, after ACCEPT %g %g\n",
              scrolling, screens, menu_moving, entered[0], entered[1], after_back[0], after_back[1], after_accept[0], after_accept[1]);
  EXPECT(stage == kDone && screens == 2 && scrolling == 2 && menu_moving == 1);
  EXPECT(after_back[0] == entered[0] && after_back[1] == entered[1] && (entered[0] != 0.25f || entered[1] != 0.75f));
  EXPECT(after_accept[0] == 0.25f && after_accept[1] == 0.75f);
}

namespace {
// The keyboard: each queued DIK goes down on one read and up on the next.
struct TapKeys : platform::Input {
  std::vector<uint32_t> queue;
  uint32_t up = 0;
  bool open(Device d) override { return d == kKeyboard; }
  void close(Device) override {}
  int32_t acquire(Device) override { return 0; }
  int32_t unacquire(Device) override { return 0; }
  int32_t keyboard_data(platform::KeyEvent* ev, uint32_t* count) override {
    uint32_t n = 0;
    if (up) ev[n++] = platform::KeyEvent{up, 0, 0, 0}, up = 0;
    else if (!queue.empty() && *count > 0) ev[n++] = platform::KeyEvent{queue.front(), 0x80, 0, 0}, up = queue.front(),
                                           queue.erase(queue.begin());
    *count = n;
    return 0;
  }
  int32_t mouse_state(platform::MouseState*) override { return kNotAcquired; }
  int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
};
}  // namespace

// Pirate's Moon's main state (0x004896e0): its idle minute only starts again (0x00489370), where
// MW3's pushed the attract movie; its fifth hotkey is P for PIRATE MISSIONS (STANDALONE), where
// MW3's T was TRAINING, which PM's menu does not have.
TEST(pm_main_menu_idles_without_the_attract_movie_and_p_opens_pirate_missions_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  MountsBack back;
  zgame::set_game(pm::table());
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_main_idle";
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
  TapKeys keys;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.movie_clock = &movies, p.screen = &screen;
  p.sound = &sound, p.files = &files, p.input = &keys;
  enum { kWait, kIdle, kT, kP, kDone };
  int stage = kWait, frames = 0;
  bool stayed = false, refilled = false, t_stayed = false, left = false;
  std::string after_p;
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::MainState& main = mech3::states().main;
    auto name = [&] {
      auto* st = dynamic_cast<mech3::ShellState*>(app.current());
      return std::string(st ? st->name() : "(none)");
    };
    engine.on_present = [&](int) {
      const bool at_main = app.current() == &main && mech3::main_menu();
      if (app.pending()) return;
      if (stage == kWait && at_main) {
        main.idle_ = 0.0001f;  // the minute about to run out
        stage = kIdle, frames = 0;
      } else if (stage == kIdle && ++frames == 5) {
        stayed = app.current() == &main, refilled = main.idle_ > 59.0f;
        keys.queue.push_back(0x14);  // T
        stage = kT, frames = 0;
      } else if (stage == kT && ++frames == 5) {
        t_stayed = app.current() == &main;
        keys.queue.push_back(0x19);  // P
        stage = kP, frames = 0;
      } else if (stage == kP && ++frames == 5) {
        after_p = name();
        stage = kDone;
      }
    };
    Pump pump;
    pump.limit = 3000;
    pump.stop = [&] {  // every pass, not only the presented ones: with -nofmv an attract state comes and goes between
      if (stage == kIdle && (app.current() != &main || app.pending())) left = true;
      return stage == kDone;
    };
    app.run(pump);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  mech3::options() = mech3::Options{};
  stayed = stayed && !left;
  std::printf("  idle out: still the main menu %d (left it %d), the minute again %d; T: still the main menu %d; P: %s\n", stayed,
              left, refilled, t_stayed, after_p.c_str());
  EXPECT(stage == kDone && stayed && refilled && t_stayed);
  EXPECT(after_p == "Pilot State");  // as the PIRATE MISSIONS button: the pilot screen first
}

namespace {
struct MovieNames : mech3::GameEngine {
  using mech3::GameEngine::GameEngine;
  std::vector<std::string> names;
  int32_t movie_load(mech3::Handle m, const char* file, const char* name) override {
    names.emplace_back(name ? name : "");
    return mech3::GameEngine::movie_load(m, file, name);
  }
};
}  // namespace

// Pirate's Moon's Intro state (0x004850a0) plays INTRO only, then the main menu (state 12; its
// reader.zbd and self checks are the trap the rebuild drops); MW3's played SPLASH after it, with
// movies off too. With and without -nofmv: the movies asked for between the start and the menu.
TEST(pm_intro_plays_intro_only_and_no_splash_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  std::vector<std::string> seen[2];
  bool menu[2] = {};
  for (int fmv = 0; fmv < 2; ++fmv) {
    MountsBack back;
    zgame::set_game(pm::table());
    const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_intro_" + std::to_string(fmv);
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
    {
      MovieNames engine(p, fmv ? "mech3.exe" : "mech3.exe -nofmv");
      engine.quiet = true;
      mech3::Mech3App app(&engine);
      mech3::set_app(&app);
      EXPECT(app.init_instance());
      Pump pump;
      pump.limit = 3000;
      pump.stop = [&] { return app.current() == &mech3::states().main && mech3::main_menu() != nullptr; };
      app.run(pump);
      menu[fmv] = app.current() == &mech3::states().main;
      seen[fmv] = engine.names;
      app.stop_game();
      app.exit_instance();
      mech3::set_app(nullptr);
    }
    zrdr::close_archives(true);
    mech3::reset_states();
    mech3::options() = mech3::Options{};
  }
  auto join = [](const std::vector<std::string>& v) {
    std::string s;
    for (const std::string& n : v) s += (s.empty() ? "" : " ") + n;
    return s.empty() ? std::string("none") : s;
  };
  std::printf("  -nofmv: %s, the menu %d; movies on: %s, the menu %d\n", join(seen[0]).c_str(), menu[0], join(seen[1]).c_str(),
              menu[1]);
  EXPECT(menu[0] && menu[1]);
  EXPECT(seen[0].empty());
  EXPECT(seen[1] == std::vector<std::string>{"INTRO"});
}

// Pirate's Moon's FMV loader (0x005ba110) on its own fmv.zrd: INTRO is PLAYCD 2 (a CD action,
// 0x005bb190) and the logos (pictures, waits, fades): 15 actions; the CD action starts track 2
// in mode 6 (0x005bb1d0); only the pictures are not skippable.
#include "gamez/zfmv/fmv_script.h"
namespace {
struct CdHost : zfmv::Host {
  std::vector<std::pair<int32_t, int32_t>> played;
  int stops = 0;
  void image_path(const char*) override {}
  void cd_play(int32_t track, int32_t mode) override { played.emplace_back(track, mode); }
  void cd_stop() override { ++stops; }
};
}  // namespace

TEST(pm_fmv_intro_opens_with_its_cd_track_and_only_pictures_hold_a_key) {
  std::error_code ec;
  const fs::path reader = fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd";
  if (!fs::exists(reader, ec)) return;
  zgame::set_game(pm::table());
  zrdr::init();
  zrdr::close_archives(true);
  EXPECT(zrdr::open_archive(fs::canonical(reader, ec).string().c_str(), true));
  zrdr::Node* root = zrdr::load("fmv.zrd");
  EXPECT(root != nullptr);
  CdHost h;
  zfmv::set_host(&h);
  if (root) {
    zfmv::Script s(nullptr, nullptr, 0);
    const int32_t n = s.load_tree(root, "INTRO");
    int32_t count = 0, pictures = 0, held = 0;
    for (zfmv::Action* a = s.head; a; a = a->next) {
      ++count;
      const bool picture = dynamic_cast<zfmv::ImageAction*>(a) != nullptr;
      pictures += picture;
      held += !a->skippable();
    }
    auto* cd = dynamic_cast<zfmv::CdAction*>(s.head);
    std::printf("  INTRO: %d entries, %d actions, %d pictures, %d not skippable; first a CD action %d (track %d)\n", n, count,
                pictures, held, cd != nullptr, cd ? cd->track : -1);
    EXPECT(n == 15 && count == 15 && pictures == 5 && held == 5);
    EXPECT(cd && cd->track == 2 && !cd->stop && cd->skippable());
    if (cd) cd->start(0.0);
    EXPECT(h.played.size() == 1 && h.played[0] == std::make_pair(2, 6) && h.stops == 0);
    zfmv::CdAction stop(0, true);
    stop.start(0.0);
    EXPECT(h.stops == 1 && !stop.skippable());
    zrdr::free_node(root);
  }
  zfmv::set_host(nullptr);
  zrdr::close_archives(true);
}

namespace {
struct PopEngine : mech3::GameEngine {
  using mech3::GameEngine::GameEngine;
  std::vector<uint32_t> pops;
  void pop_state(uint32_t arg) override {
    pops.push_back(arg);
    mech3::GameEngine::pop_state(arg);
  }
};
}  // namespace

// Pirate's Moon's game options leave with a pop of 7 (0x004399e0; MW3's 1), the graphics and audio
// page with 1 as before (0x0043b790).
TEST(pm_game_options_leave_with_a_pop_of_7_headless) {
  std::error_code ec;
  if (!fs::exists(fs::path(MW3PM_GAME_DIR) / "zbd" / "reader.zbd", ec)) return;
  MountsBack back;
  zgame::set_game(pm::table());
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_options_pop";
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
  enum { kMain, kOptions, kGfx, kBackInOptions, kLeave, kDone };
  int stage = kMain;
  std::vector<uint32_t> pops;
  {
    PopEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    engine.on_present = [&](int) {
      zgame::State* now = app.current();
      auto* os = dynamic_cast<mech3::OptionsState*>(now);
      auto* game = os ? dynamic_cast<mech3::GameOptionsScreen*>(engine.screen_of(os->screen_)) : nullptr;
      auto* g = os ? dynamic_cast<mech3::GfxAudioScreen*>(engine.screen_of(os->screen_)) : nullptr;
      if (app.pending()) return;
      if (stage == kMain && now == &mech3::states().main && mech3::main_menu()) {
        mech3::main_menu()->buttons[mech3::MainMenuButton::kOptions].on_left_up();
        stage = kOptions;
      } else if (stage == kOptions && game) {
        game->gfx_audio.on_left_up();
        stage = kGfx;
      } else if (stage == kGfx && g) {
        g->back.on_left_up();  // the graphics page: 1
        stage = kBackInOptions;
      } else if (stage == kBackInOptions && game) {
        game->back.on_left_up();  // the game options: 7
        stage = kLeave;
      } else if (stage == kLeave && now == &mech3::states().main) {
        stage = kDone;
      }
    };
    Pump pump;
    pump.limit = 3000;
    pump.stop = [&] { return stage == kDone; };
    app.run(pump);
    pops = engine.pops;
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::reset_states();
  mech3::options() = mech3::Options{};
  std::string said;
  for (uint32_t v : pops) said += (said.empty() ? "" : " ") + std::to_string(v);
  std::printf("  stage %d, pops: %s\n", stage, said.c_str());
  EXPECT(stage == kDone && pops.size() >= 2 && pops[pops.size() - 2] == 1 && pops.back() == 7);
}
