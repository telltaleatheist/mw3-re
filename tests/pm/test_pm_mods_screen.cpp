// Mods by campaign on Pirate's Moon, at the screens (docs/mods-campaigns-plan.md; MechWarrior 3's twins are in
// tests/test_program.cpp): MODS lists the Campaign and the Pirate Missions, each with its own ticks, its own
// RESET and its own pilots; the pilot screens inside an entry list the main menu's pilots and bring the one
// picked, with its saved games into the Campaign and with none into the Pirate Missions and Instant Action;
// IMPORT takes a modded Pirate's Moon folder and refuses a MechWarrior 3 one.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>
#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include "check.h"
#include "gamez/zgame/zgame_table.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/ztwod/twod.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_states.h"
#include "mech3/mainmenu.h"
#include "mech3/mech_config.h"
#include "mech3/mission.h"
#include "mech3/mod_campaign.h"
#include "mech3/mods.h"
#include "mech3/mods_menu.h"
#include "mech3/options.h"
#include "mech3/pilot.h"
#include "mech3/pilot_menu.h"
#include "platform/silent_sound.h"
#include "pm/pm_game.h"
#include "upgrades/mods/campaign_entries.h"
#include "upgrades/mods/entry_pilots.h"
#include "upgrades/mods/entry_run.h"
#include "upgrades/mods/import_folder.h"

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
  int passes = 0, limit = 3000;
  std::function<bool()> stop;
  bool peek() override { return ++passes > limit || (stop && stop()); }
  bool pump() override { return false; }
  void wait(uint32_t) override {}
  void post_quit() override {}
};

// The test run again alone in a fresh process (the menus' globals of an earlier test are not met), its checks
// failing the parent.
void in_child(const char* test, const std::function<void()>& run) {
  if (std::getenv("PM_SCREEN_CHILD")) return run();
#if !defined(_WIN32)
  std::string self(4096, '\0');
#if defined(__APPLE__)
  uint32_t size = static_cast<uint32_t>(self.size());
  if (_NSGetExecutablePath(self.data(), &size) != 0) self.clear();
#else
  const ssize_t n = readlink("/proc/self/exe", self.data(), self.size() - 1);
  self.resize(n > 0 ? static_cast<size_t>(n) : 0);
#endif
  self = self.c_str();
  EXPECT(!self.empty());
  std::fflush(nullptr);
  const pid_t child = fork();
  if (child == 0) {
    setenv("PM_SCREEN_CHILD", "1", 1);
    setenv("MW3_TEST", test, 1);
    execl(self.c_str(), self.c_str(), static_cast<char*>(nullptr));
    _exit(127);
  }
  int status = 1;
  waitpid(child, &status, 0);
  EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
#else
  run();
#endif
}

std::string fresh(const char* name) {
  const std::string d = std::string(MW3_TEST_TMP_ROOT) + "/" + name;
  std::error_code ec;
  fs::remove_all(d, ec);
  fs::create_directories(d, ec);
  return d;
}
void put(const std::string& path, const std::string& text) {
  std::error_code ec;
  fs::create_directories(fs::path(path).parent_path(), ec);
  std::ofstream(path, std::ios::binary) << text;
}
std::string text_of(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
// Every file under the main menu's pilots folder: its path, bytes and time.
std::string snapshot(const std::string& dir) {
  std::string t;
  std::error_code ec;
  std::vector<fs::path> all;
  for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) all.push_back(it->path());
  std::sort(all.begin(), all.end());
  for (const fs::path& f : all)
    if (fs::is_regular_file(f, ec))
      t += f.string() + ":" + text_of(f.string()) + ":" + std::to_string(static_cast<long long>(fs::last_write_time(f, ec).time_since_epoch().count())) + "\n";
  return t;
}
void drop_pilots() {
  mech3::current_pilot() = nullptr;
  for (mech3::Pilot* q : mech3::pilots()) delete q;
  mech3::pilots().clear();
}
// The game set up on Pirate's Moon's install with `wd` its write dir.
void pm_start(const std::string& wd) {
  zgame::set_game(pm::table());
  mech3::mods_game() = "mw3pm";
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd.c_str());
  zutl::mount(MW3PM_GAME_DIR);
  mech3::mods_menu_write_dir() = wd;
  mech3::mods_menu_install() = MW3PM_GAME_DIR;
  mech3::reset_states();
  if (!zsnd::state().open) zsnd::state() = zsnd::State{};
  mech3::missions() = mech3::Missions{};
  mech3::options() = mech3::Options{};
}
struct Fakes {
  Window window;
  Alert alert;
  Clock clock;
  MovieClock movies;
  Screen screen;
  platform::SilentSoundOutput sound;
  mech3::MountFiles files;
  mech3::Platform p;
  Fakes() {
    p.window = &window, p.alert = &alert, p.clock = &clock, p.movie_clock = &movies, p.screen = &screen;
    p.sound = &sound, p.files = &files;
  }
};
}  // namespace

// MODS on Pirate's Moon: CAMPAIGNS lists the Campaign and the Pirate Missions; a tick made with the one chosen
// is written to its own mods.txt and not shown for the other; PLAY asks for the chosen one. RESET on the Pirate
// Missions clears their ticks and pilots and leaves the Campaign's; a copy saved from the Pirate Missions is
// deleted with its folder of ours; the main menu's pilots folder is never touched.
TEST(pm_mods_screen_campaigns_their_own_ticks_and_reset_headless) {
  if (!fs::exists(MW3PM_GAME_DIR)) return;
  in_child("pm_mods_screen_campaigns_their_own_ticks_and_reset_headless", [] {
    const std::string wd = fresh("pm_mods_screen_campaigns");
    put(wd + "/mods/hud/mod.txt", "name=Amber HUD\n");
    put(wd + "/mods/hud/mw3-mod-added.txt", "added\n");
    put(wd + "/mods/weps/mod.txt", "name=Weps\n");
    put(wd + "/mods/mine/mod.txt", "name=Pirates, mine\nkind=campaign\nbase=pirate\n");
    put(wd + "/mods/mine/mw3-editor.txt", "the editor's\n");
    for (const char* id : {"campaign", "pirate", "mine"}) {
      put(wd + "/campaigns/" + id + "/mw3-campaign.txt", "ours\n");
      put(wd + "/campaigns/" + id + "/pilots/kept.txt", "of this campaign");  // not a pilot file: the game would load one
      put(wd + "/campaigns/" + id + "/pilots/games/Ace02.mw3", "save");
    }
    put(wd + "/pilots/the main menu's.txt", "kept");
    pm_start(wd);
    Fakes f;
    {
      mech3::GameEngine engine(f.p, "mech3.exe -nofmv");
      engine.quiet = true;
      mech3::Mech3App app(&engine);
      mech3::set_app(&app);
      EXPECT(app.init_instance());
      mech3::States& st = mech3::states();
      int stage = 0;
      uint32_t answer = 2;
      std::vector<std::string> boxes, asked;
      engine.on_message_box = [&](int32_t, int32_t, int32_t) { return boxes.push_back(engine.last_box_text), answer; };
      mech3::mods_menu_play() = [&](const std::string& entry, std::string*) { return asked.push_back(entry), true; };
      std::string listed, details, campaign_rows, campaign_after, pirate_rows, pirate_after, campaign_again, tabs, cancelled, rows_end;
      bool pirate_reset = false, campaign_kept = false, copy_gone = false, main_kept = false;
      engine.on_present = [&](int) {
        zgame::State* now = app.current();
        if (stage == 0 && now == &st.main && mech3::main_menu()) {
          const_cast<mech3::MainMenu*>(mech3::main_menu())->mods.on_left_up();
          stage = 1;
          return;
        }
        auto* ms = dynamic_cast<mech3::ModsState*>(now);
        auto* scr = ms ? dynamic_cast<mech3::ModsScreen*>(engine.screen_of(ms->screen_)) : nullptr;
        if (stage != 1 || !scr) return;
        auto rows = [&] {
          std::string t;
          for (const auto* i : scr->mod_list.items) t += std::string(i->text) + "|";
          return t;
        };
        auto campaigns = [&] {
          std::string t;
          for (const auto* i : scr->campaign_list.items) t += std::string(i->text) + "|";
          return t;
        };
        auto campaign = [&](const char* name) {
          for (size_t i = 0; i < scr->tables.campaigns.size(); ++i)
            if (scr->tables.campaigns[i].name == name) scr->campaign_list.select(static_cast<int32_t>(i));
        };
        auto mod = [&](const char* name) {
          for (size_t i = 0; i < scr->ticks.rows.size(); ++i)
            if (scr->ticks.rows[i].info.name == name) scr->mod_list.select(static_cast<int32_t>(i));
        };
        auto tab = [&] {
          const bool del = !(scr->delete_btn.flags & ztwod::kHidden), res = !(scr->reset_btn.flags & ztwod::kHidden);
          return std::string(del && res ? "both" : del ? "DELETE" : res ? "RESET" : "none");
        };
        listed = campaigns();
        details = scr->detail.text;
        // The Campaign (chosen as the screen opens): a tick of its own.
        campaign_rows = rows();
        mod("Amber HUD");
        scr->on_button(mech3::ModsScreen::kToggle, 0, 0);
        campaign_after = rows();
        // The Pirate Missions: none of the Campaign's ticks; one of their own (a ticked row sorts first); PLAY asks for them.
        campaign("Pirate Missions");
        pirate_rows = rows();
        mod("Weps");
        scr->on_button(mech3::ModsScreen::kToggle, 0, 0);
        pirate_after = rows();
        scr->on_button(mech3::ModsScreen::kEnter, 0, 0);
        campaign("Campaign");
        campaign_again = rows();
        scr->on_button(mech3::ModsScreen::kEnter, 0, 0);
        // The tab by the row: RESET on each of the game's own, DELETE on the copy.
        tabs = tab() + ",";
        campaign("Pirate Missions"), tabs += tab() + ",";
        campaign("Pirates, mine"), tabs += tab();
        // RESET on the Pirate Missions: CANCEL first, then OK. The Campaign's folder is not part of it.
        campaign("Pirate Missions");
        answer = 2;
        scr->on_button(mech3::ModsScreen::kDelete, 0, 0);
        cancelled = scr->note.text;
        const bool kept = fs::exists(wd + "/campaigns/pirate/mods.txt") && fs::exists(wd + "/campaigns/pirate/pilots/kept.txt");
        answer = 1;
        scr->on_button(mech3::ModsScreen::kDelete, 0, 0);
        pirate_reset = kept && !fs::exists(wd + "/campaigns/pirate/mods.txt") && !fs::exists(wd + "/campaigns/pirate/pilots/kept.txt") &&
                       !fs::exists(wd + "/campaigns/pirate/pilots/games/Ace02.mw3") && fs::exists(wd + "/campaigns/pirate/mw3-campaign.txt");
        campaign_kept = text_of(wd + "/campaigns/campaign/mods.txt") == "mod=hud\n" && fs::exists(wd + "/campaigns/campaign/pilots/kept.txt") &&
                        fs::exists(wd + "/campaigns/campaign/pilots/games/Ace02.mw3");
        // The copy saved from the Pirate Missions, by its row.
        campaign("Pirates, mine");
        scr->on_button(mech3::ModsScreen::kDelete, 0, 0);
        copy_gone = !fs::exists(wd + "/mods/mine") && !fs::exists(wd + "/campaigns/mine");
        main_kept = text_of(wd + "/pilots/the main menu's.txt") == "kept" && fs::exists(wd + "/mods/weps/mod.txt");
        rows_end = campaigns();
        scr->on_button(mech3::ModsScreen::kBack, 0, 0);
        stage = 2;
      };
      Pump pump;
      pump.stop = [&] { return stage == 2 && app.current() == &st.main; };
      app.run(pump);
      std::printf("  listed \"%s\" (%s); Campaign \"%s\" -> \"%s\"; Pirate Missions \"%s\" -> \"%s\"; Campaign again \"%s\"; tabs %s; at the end \"%s\"\n",
                  listed.c_str(), details.c_str(), campaign_rows.c_str(), campaign_after.c_str(), pirate_rows.c_str(), pirate_after.c_str(),
                  campaign_again.c_str(), tabs.c_str(), rows_end.c_str());
      for (const std::string& b : boxes) std::printf("  box: %s\n", b.c_str());
      EXPECT(stage == 2 && listed == "Campaign|Pirate Missions|Pirates, mine|" && details == "Campaign");
      EXPECT(campaign_rows == "[ ] Amber HUD|[ ] Weps|" && campaign_after == "[x] Amber HUD|[ ] Weps|");
      EXPECT(pirate_rows == "[ ] Amber HUD|[ ] Weps|" && pirate_after == "[x] Weps|[ ] Amber HUD|" && campaign_again == campaign_after);
      EXPECT((asked == std::vector<std::string>{"pirate", "campaign"}));
      EXPECT(tabs == "RESET,RESET,DELETE");
      EXPECT(cancelled == "Not reset." && pirate_reset && campaign_kept);
      EXPECT(copy_gone && main_kept && rows_end == "Campaign|Pirate Missions|");
      EXPECT(boxes.size() == 3 && boxes[0] == boxes[1] && boxes[0].find("Reset Pirate Missions to the original?") == 0);
      if (boxes.size() == 3) EXPECT(boxes[2].find("Delete Pirates, mine?") == 0);
      EXPECT(mech3::pilots().empty());
      mech3::mods_menu_play() = nullptr;
      app.stop_game();
      app.exit_instance();
      mech3::set_app(nullptr);
    }
    mech3::mods_menu_write_dir().clear();
    mech3::mods_menu_install().clear();
    zgame::set_game(zgame::mw3_table());
  });
}

// The pilot screens inside Pirate's Moon's own entries: the Campaign's and the Pirate Missions' (the Standalone
// State's) both list the main menu's pilot by name and bring it when the player picks it, the main menu's files
// unchanged to the byte and the time. Into the Campaign it comes with its saved game and carries on, and its
// Pirate Missions record (the missions reached, in the pilot's file) comes with it; into the Pirate Missions it
// comes with that record and no saved game; into Instant Action with no saved game.
TEST(pm_entry_pilot_screens_bring_the_main_menus_pilot_headless) {
  if (!fs::exists(MW3PM_GAME_DIR)) return;
  in_child("pm_entry_pilot_screens_bring_the_main_menus_pilot_headless", [] {
    for (const char* id : {"campaign", "pirate", "ia"}) {
      const std::string wd = fresh((std::string("pm_entry_pilots_") + id).c_str());
      pm_start(wd);
      const bool screen = std::string(id) != "ia";
      Fakes f;
      {
        mech3::GameEngine engine(f.p, "mech3.exe -nofmv");
        engine.quiet = true;
        mech3::Mech3App app(&engine);
        mech3::set_app(&app);
        EXPECT(app.init_instance());
        // The main menu's pilot Ace: on to the Campaign's second mission with its saved game, three Pirate Missions reached.
        std::string save_name;
        int reached = -1;
        if (mech3::Pilot* ace = mech3::create_pilot("Ace")) {
          auto* c = ace->campaigns.empty() ? nullptr : dynamic_cast<mech3::CampaignGame*>(ace->campaigns.front());
          mech3::CampaignGame* n = c ? c->next_mission(true, false) : nullptr;
          EXPECT(n != nullptr);
          if (n) {
            ace->campaigns.push_back(n), ace->current = n;
            char digits[8];
            std::snprintf(digits, sizeof digits, "%02d", n->mission_number());
            save_name = std::string("Ace") + digits + ".mw3";
            put(wd + "/pilots/games/" + save_name, "ace's saved game");
            EXPECT(n->readable());  // the name the game itself looks for
          }
          EXPECT(ace->standalone != nullptr);
          if (ace->standalone) ace->standalone->x3c = 3, reached = 3;
          ace->save();
          EXPECT(fs::is_regular_file(wd + "/pilots/Ace.mw3"));
        }
        drop_pilots();
        const std::vector<mech3::CampaignEntry> all = mech3::campaign_entries(wd);
        const mech3::CampaignEntry* e = mech3::find_entry(all, id);
        EXPECT(e && mech3::entry_ensure(wd, *e, nullptr));
        mech3::set_pilots_folder(mech3::entry_pilots_folder(id));
        mech3::set_configs_folder(mech3::entry_configs_folder(id));
        mech3::EntryRun& run = mech3::entry_run();
        run = mech3::EntryRun{};
        run.screen = screen ? mech3::EntryRun::kPlay : mech3::EntryRun::kInstantAction, run.id = id;
        int left = 0;
        run.leave = [&] { ++left; };
        const std::string before = snapshot(wd + "/pilots");
        const std::string there = wd + "/campaigns/" + id + "/pilots/";
        std::string rows_at_open, rows_after;
        bool nothing_yet = false, picked = false;
        int mode = -1, games = -1, record = -1, listed = -1;
        mech3::PilotScreen* shown = nullptr;
        if (screen) {
          int stage = 0;
          engine.on_present = [&](int) {
            auto* ps = dynamic_cast<mech3::PilotState*>(app.current());
            if (stage != 0 || !ps || !ps->screen) return;
            mech3::PilotScreen* s = shown = ps->screen;
            auto rows = [&] {
              std::string t;
              for (const auto* i : s->pilots.items) t += std::string(i->text) + "|";
              return t;
            };
            mode = s->mode;
            rows_at_open = rows();
            nothing_yet = s->chosen == nullptr && s->main_row_chosen() && !mech3::find_pilot("Ace") && !fs::exists(there + "Ace.mw3");
            s->pilots.select(0);
            rows_after = rows();
            mech3::Pilot* ace = mech3::find_pilot("Ace");
            picked = ace && s->chosen == ace && fs::is_regular_file(there + "Ace.mw3");
            games = ace ? ace->campaign_count() : -1;
            record = ace && ace->standalone ? ace->standalone->x3c : -1;
            listed = s->games.item_count();
            stage = 1;
          };
          Pump pump;
          pump.stop = [&] { return stage == 1; };
          app.run(pump);
          EXPECT(stage == 1 && left == 0);
          if (shown) shown->chosen = nullptr;  // the screen is still up: its choice not left on a pilot about to go
        } else {
          std::string why;
          mech3::Pilot* ace = mech3::entry_bring_pilot("Ace", &why);
          picked = ace && fs::is_regular_file(there + "Ace.mw3");
          games = ace ? ace->campaign_count() : -1;
          record = ace && ace->standalone ? ace->standalone->x3c : -1;
          if (!why.empty()) std::printf("  ia: %s\n", why.c_str());
        }
        const bool save_came = !save_name.empty() && fs::exists(there + "games/" + save_name);
        const bool main_untouched = snapshot(wd + "/pilots") == before;
        std::printf("  %s: mode %d, open \"%s\", after \"%s\", picked %d; the saved game came %d, campaign games %d, Pirate Missions reached %d, "
                    "games listed %d; the main menu's untouched %d\n",
                    id, mode, rows_at_open.c_str(), rows_after.c_str(), picked, save_came, games, record, listed, main_untouched);
        EXPECT(picked && main_untouched && record == reached);
        if (screen) EXPECT(rows_at_open == "Ace|" && rows_after == "Ace|" && nothing_yet);
        if (std::string(id) == "campaign") {
          EXPECT(mode == mech3::kCampaign && save_came && games == 2 && listed == 2);  // carries on: the new game and mission 2
        } else {
          EXPECT(!save_came && fs::is_empty(there + "games"));
          EXPECT(games == 1);  // the Campaign's record without its saved game is dropped at the load (0x004790d0's kind)
          if (screen) EXPECT(mode == mech3::kStandalone && listed == 4);  // the missions reached and the next (0x0040e4d0)
        }
        mech3::entry_run() = mech3::EntryRun{};
        if (mech3::entered_campaign()) mech3::campaign_leave();
        mech3::set_pilots_folder("");
        mech3::set_configs_folder("");
        drop_pilots();
        app.stop_game();
        app.exit_instance();
        mech3::set_app(nullptr);
      }
      zrdr::close_archives(true);
      mech3::reset_states();
    }
    mech3::mods_menu_write_dir().clear();
    mech3::mods_menu_install().clear();
    zgame::set_game(zgame::mw3_table());
  });
}

// IMPORT given a whole modded Pirate's Moon folder (upgrades/mods/import_folder.h): what differs from the
// player's Pirate's Moon install is taken as a mod for Pirate's Moon, nothing asked; the player's own install and
// a MechWarrior 3 folder are refused with the reason; neither folder is changed.
TEST(pm_import_from_a_modded_pirates_moon_folder) {
  const std::string was = mech3::mods_game();
  mech3::mods_game() = "mw3pm";
  const std::string root = fresh("pm_import_folder"), wd = root + "/wd";
  const std::string inst = root + "/install", modded = root + "/Modded Moon", mw3 = root + "/MechWarrior";
  for (const std::string& d : {inst, modded}) {
    put(d + "/Mech3.exe", "pm");
    put(d + "/zbd/reader.txt", "pm's reader");
    put(d + "/zbd/c1/readersa1.txt", "a Pirate Mission");
    put(d + "/zbd/c1/gamez.txt", "abc");
  }
  put(modded + "/zbd/reader.txt", "the mod's reader");
  put(modded + "/zbd/c1/New.txt", "new");
  put(modded + "/pilots/Ace.mw3", "the modder's pilot");
  put(mw3 + "/Mech3.exe", "mw3"), put(mw3 + "/zbd/reader.txt", "mw3's reader"), put(mw3 + "/zbd/c4b/m5.txt", "only MechWarrior 3 has it");
  fs::create_directories(wd);
  const std::string before = snapshot(inst) + snapshot(modded);
  int asks = 0;
  const auto ask = [&](const std::string&) { return ++asks, 2; };
  std::vector<std::string> on;
  std::string report;
  const mech3::ModAddDone own = mech3::import_folder_run(wd, inst, inst, ask, &on, &report);
  const mech3::ModAddDone other = mech3::import_folder_run(wd, mw3, inst, ask, &on, &report);
  const mech3::ModAddDone took = mech3::import_folder_run(wd, modded, inst, ask, &on, &report);
  std::printf("  own: %s\n  MechWarrior 3's: %s\n  took: %s (%s)\n", own.message.c_str(), other.message.c_str(), took.message.c_str(), report.c_str());
  EXPECT(!own.ok && own.message.find("your own install") != std::string::npos);
  EXPECT(!other.ok && other.message.find("MechWarrior 3 install") != std::string::npos);
  EXPECT(took.ok && asks == 0 && report.find("Took 2 files that differ from your install") == 0);
  const std::string mod = wd + "/mods/Modded Moon";
  EXPECT(text_of(mod + "/zbd/reader.txt") == "the mod's reader" && text_of(mod + "/zbd/c1/New.txt") == "new");
  EXPECT(fs::is_regular_file(mod + "/mw3-mod-added.txt") && !fs::exists(mod + "/Mech3.exe") && !fs::exists(mod + "/pilots") &&
         !fs::exists(mod + "/zbd/c1/gamez.txt"));
  EXPECT(text_of(mod + "/mod.txt").find("for=mw3pm\n") != std::string::npos);
  EXPECT(snapshot(inst) + snapshot(modded) == before);
  mech3::mods_game() = was;
  std::error_code ec;
  fs::remove_all(root, ec);
}
