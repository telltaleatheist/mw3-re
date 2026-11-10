// Mods on Pirate's Moon (docs/modding.md): a mod's mech row launched as the
// player's mech in PM's c1m1, and (env-gated) a real mod's files read and
// played. MechWarrior 3's own checks are tests/test_mods.cpp and
// test_program.cpp's mods_*_headless.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <utility>

#if !defined(_WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#include <vector>

#include "check.h"
#include "crt/msvc_string.h"
#include "gamez/zgame/zgame_table.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zutil/zutl_msgdll.h"
#include "gamez/zweapon/zwep_init.h"
#include "gamez/zsave/zsave.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_mech.h"
#include "mech3/app_states.h"
#include "mech3/mech_control.h"
#include "mech3/mech_ids.h"
#include "mech3/mech_config.h"
#include "mech3/mech_reader.h"
#include "mech3/mod_campaign.h"
#include "mech3/mission.h"
#include "mech3/game_editor.h"
#include "mech3/mainmenu.h"
#include "mech3/mods.h"
#include "mech3/mods_menu.h"
#include "mech3/pause_menu.h"
#include "mech3/options.h"
#include "mech3/player.h"
#include "platform/silent_sound.h"
#include "pm/pm_game.h"
#include "upgrades/editor/mods_edit.h"
#include "upgrades/mods/mod_test.h"
#include "upgrades/mods/mod_export.h"
#include "upgrades/mods/mod_needs.h"
#include "upgrades/mods/mod_report.h"
#include "upgrades/mods/save_mods.h"
#include "upgrades/mods/starter_mods.h"

namespace mech3 {  // mech_names.h (its Engine clashes with the app's)
int32_t mech_count();
const char* mech_title(int32_t mech);
int32_t mech_index(const char* name);
}  // namespace mech3

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
  std::function<bool()> stop;
  bool peek() override { return ++passes > limit || (stop && stop()); }
  bool pump() override {
    mech3::missions().cleanup(false);
    return false;
  }
  void wait(uint32_t) override {}
  void post_quit() override {}
};

// The install's reader.zbd with `from` copied as `to`: a mod's repacked reader archive.
bool reader_with_copy(const std::string& install, const std::string& out, const char* from, const char* to) {
  zutl_zar::Archive src, dst;
  zutl_zar::init(src), zutl_zar::init(dst);
  bool ok = zutl_zar::open(src, (install + "/zbd/reader.zbd").c_str()), found = false;
  std::vector<std::pair<std::string, std::string>> entries;
  for (int i = 0; ok && i < src.count; ++i) {
    uint32_t n = src.entries[i].length;
    std::string b(n ? n : 1, '\0');
    ok = zutl_zar::read(src, i, b.data(), &n) == 0;
    entries.emplace_back(src.entries[i].name, b.substr(0, n));
  }
  zutl_zar::close(src), zutl_zar::destroy(src);
  if (!ok) return false;
  for (size_t i = 0, n = entries.size(); i < n; ++i)
    if (crt::msvc_stricmp(entries[i].first.c_str(), from) == 0) {
      const std::string bytes = entries[i].second;  // before the vector grows
      entries.emplace_back(to, bytes), found = true;
    }
  std::error_code ec;
  fs::create_directories(fs::path(out).parent_path(), ec);
  if (!found || !zutl_zar::create(dst, out.c_str())) return false;
  for (const auto& [name, bytes] : entries) zutl_zar::add(dst, name.c_str(), bytes.data(), static_cast<uint32_t>(bytes.size()));
  zutl_zar::close(dst), zutl_zar::destroy(dst);
  return true;
}

// PM's c1m1 launched (with the mods already mounted) as a debug start does,
// `mech` the player's: up to `frames` frames into the Core state, then what
// the launch made.
struct Played {
  bool core = false, made = false;
  int frames = 0;
  mech3::LaunchedMech mech;
};
Played play_c1m1(int32_t mech, int frames) {
  Played out;
  mech3::reset_states();
  if (!zsnd::state().open) zsnd::state() = zsnd::State{};
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
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
  {
    mech3::GameEngine engine(p, "mech3.exe -nofmv");
    engine.quiet = true;
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);
    EXPECT(app.init_instance());
    mech3::GameEngine::Launch l;  // c1, a campaign mission, mission 1
    l.mech = mech;
    engine.set_launch(l);
    app.post_intro_ = &mech3::states().load;
    engine.on_present = [&](int) {
      if (app.current() != &mech3::states().core) return;
      out.core = true;
      if (++out.frames == 5) out.made = mech3::launched_mech(&out.mech);
    };
    Pump pump;
    pump.limit = 4000;
    pump.stop = [&] { return out.frames >= frames; };
    app.run(pump);
    mech3::missions().cleanup(false);
    app.stop_game();
    app.exit_instance();
    mech3::set_app(nullptr);
  }
  zrdr::close_archives(true);
  mech3::free_dfns();
  mech3::reset_states();
  mech3::missions() = mech3::Missions{};
  mech3::mech_types_forget();
  mech3::player_controls_instance() = nullptr;
  mech3::options() = mech3::Options{};
  return out;
}

// A launch in a fresh process: this test run again alone (PM_MODS_CHILD set,
// the launch then made in it), as an earlier test's mission in this process
// leaves what crashes a second (zcls's lists; a fork would inherit it). The
// child's checks fail the parent.
void in_child(const char* test, const std::function<void()>& run) {
  if (std::getenv("PM_MODS_CHILD")) return run();
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
    setenv("PM_MODS_CHILD", "1", 1);
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

struct MountsBack {
  zutl::Mounts was = zutl::mounts();
  ~MountsBack() {
    mech3::mods_mount("", {}, {});
    zutl::mounts() = was;
  }
};
}  // namespace

// A mod's mech row on Pirate's Moon is mech 26 (25, 0x19, its "no mech");
// PM's c1m1 launches it as the player's mech from its own dfn_thor_b.zrd
// (the Thor's, in the mod's repacked reader.zbd) with the Thor's models,
// motions and sounds (model=thor).
TEST(pm_mods_mech_row_launches_as_the_player_c1m1_headless) {
  if (!fs::exists(MW3PM_GAME_DIR)) return;
  MountsBack back;
  zgame::set_game(pm::table());
  const std::string install = MW3PM_GAME_DIR, wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_mods_mech_row";
  std::error_code ec;
  fs::remove_all(wd, ec);
  fs::create_directories(wd + "/mods/thorb", ec);
  std::ofstream(wd + "/mods/thorb/mod.txt") << "name=Thor B\nfor=mw3pm\nmech=thor_b tech=clan mass=70 class=heavy model=thor title=Thor B\n";
  EXPECT(reader_with_copy(install, wd + "/mods/thorb/zbd/reader.zbd", "dfn_thor.zrd", "dfn_thor_b.zrd"));
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd.c_str());
  std::string log;
  for (const std::string& l : mech3::mods_mount(wd, {"thorb"}, {install})) log += l + "\n";
  zutl::mount(install);
  EXPECT(log.find("mech thor_b added (mech 26, 70 t, the models of thor)") != std::string::npos);
  EXPECT(mech3::mech_count() == 27 && std::string(mech3::mech_title(26)) == "Thor B");
  in_child("pm_mods_mech_row_launches_as_the_player_c1m1_headless", [] {
    const Played run = play_c1m1(26, 30);
    std::printf("  core %d, frames %d; roster %d, parts %d, weapons %d, height %.1f, speed %.1f, motions %d\n", run.core, run.frames,
                run.mech.roster, run.mech.parts, run.mech.weapons, static_cast<double>(run.mech.height),
                static_cast<double>(run.mech.max_speed), run.mech.motions);
    EXPECT(run.core && run.made && run.mech.roster == 26);
    EXPECT(run.mech.parts > 0 && run.mech.weapons > 0);  // the Thor's model and loadout
    EXPECT(run.mech.height > 5.0f && run.mech.height < 20.0f && run.mech.max_speed > 10.0f && run.mech.motions);
  });
}

// A mission whose world file is gone (lead, 2026-11-10): PM's c1 script makes an
// empty world1 when its GameZReadZBDFile fails, so the load ends on the failed
// read with the box, not in the anim load's walk of a def with no node (fixes.md).
TEST(pm_mission_without_its_world_fails_headless) {
  if (!fs::exists(MW3PM_GAME_DIR)) return;
  MountsBack back;
  zgame::set_game(pm::table());
  const std::string copy = std::string(MW3_TEST_TMP_ROOT) + "/pm_no_world", wd = copy + "-write";
  std::error_code ec;
  fs::remove_all(copy, ec), fs::remove_all(wd, ec);
  for (const auto& e : fs::recursive_directory_iterator(MW3PM_GAME_DIR)) {
    const std::string rel = fs::relative(e.path(), MW3PM_GAME_DIR).generic_string();
    if (rel == "zbd/c1/gamez.zbd") continue;
    if (e.is_directory()) fs::create_directories(fs::path(copy) / rel, ec);
    else fs::create_symlink(e.path(), fs::path(copy) / rel, ec);
  }
  fs::create_directories(wd, ec);
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd.c_str());
  zutl::mount(copy);
  in_child("pm_mission_without_its_world_fails_headless", [] {
    mech3::reset_states();
    mech3::missions() = mech3::Missions{};
    mech3::mech_types_forget();
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
    std::vector<std::string> boxes;
    {
      mech3::GameEngine engine(p, "mech3.exe -nofmv");
      engine.quiet = true;
      engine.on_message_box = [&](int32_t, int32_t, int32_t) {
        boxes.push_back(engine.last_box_text);
        return 1u;
      };
      mech3::Mech3App app(&engine);
      mech3::set_app(&app);
      EXPECT(app.init_instance());
      mech3::GameEngine::Launch l;
      l.mech = 0;
      engine.set_launch(l);
      engine.missions_start(0x6098d0);
      std::string said;
      for (const std::string& b : boxes) said += "[" + b + "] ";
      std::printf("  failed %d, boxes %s\n", mech3::missions().failed, said.c_str());
      EXPECT(mech3::missions().failed && boxes.size() == 1 && said.find("c1\\gamez.zbd") != std::string::npos);
      mech3::missions().cleanup(false);
      app.stop_game();
      app.exit_instance();
      mech3::set_app(nullptr);
    }
    zrdr::close_archives(true);
    mech3::reset_states();
    mech3::missions() = mech3::Missions{};
    mech3::mech_types_forget();
    mech3::options() = mech3::Options{};
  });
}

// The mech lab's world (mission world 6, "lab") ships no gamez.zbd: its
// script's read fails and lab.gs builds it from lab.flt, by design. The
// failed-read abort above leaves it alone.
TEST(pm_lab_world_loads_without_a_gamez_headless) {
  if (!fs::exists(MW3PM_GAME_DIR)) return;
  MountsBack back;
  zgame::set_game(pm::table());
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_lab_world";
  std::error_code ec;
  fs::remove_all(wd, ec);
  fs::create_directories(wd, ec);
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd.c_str());
  zutl::mount(MW3PM_GAME_DIR);
  in_child("pm_lab_world_loads_without_a_gamez_headless", [] {
    mech3::reset_states();
    mech3::missions() = mech3::Missions{};
    mech3::mech_types_forget();
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
      mech3::GameEngine engine(p, "mech3.exe -nofmv");
      engine.quiet = true;
      mech3::Mech3App app(&engine);
      mech3::set_app(&app);
      EXPECT(app.init_instance());
      const bool loaded = mech3::missions().load_world(6);
      std::printf("  lab world loaded %d, failed %d\n", loaded, mech3::missions().failed);
      EXPECT(loaded && !mech3::missions().failed);
      mech3::missions().cleanup(false);
      app.stop_game();
      app.exit_instance();
      mech3::set_app(nullptr);
    }
    zrdr::close_archives(true);
    mech3::reset_states();
    mech3::missions() = mech3::Missions{};
    mech3::mech_types_forget();
    mech3::options() = mech3::Options{};
  });
}

// A mod's animation files Pirate's Moon does not read (unzbd's JSON, def and
// node files) are said so by --check-mod and counted on its MODS line, not
// shared; its whole anim.zbd is read (PM's reader takes the check's path).
TEST(pm_mods_anim_files_pm_does_not_read_are_said) {
  if (!fs::exists(MW3PM_GAME_DIR)) return;
  MountsBack back;
  zgame::set_game(pm::table());
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_mods_anim_unread";
  std::error_code ec;
  fs::remove_all(wd, ec);
  for (const char* m : {"a", "b"}) {
    const std::string dir = wd + "/mods/" + m;
    fs::create_directories(dir + "/zbd/c1/anim", ec);
    fs::create_directories(dir + "/zbd/anim", ec);
    std::ofstream(dir + "/mod.txt") << "name=" << m << "\nfor=mw3pm\n";
    std::ofstream(dir + "/zbd/c1/anim/door-open.json") << "{}";
    std::ofstream(dir + "/zbd/anim/door-open.json") << "{}";
  }
  fs::copy_file(fs::path(MW3PM_GAME_DIR) / "zbd/c1/anim.zbd", wd + "/mods/a/zbd/c1/anim.zbd", ec);
  int errors = -1;
  std::string report;
  for (const std::string& l : mech3::mod_check(wd + "/mods/a", MW3PM_GAME_DIR, &errors)) report += l + "\n";
  std::printf("%s", report.c_str());
  EXPECT(errors == 0);
  size_t said = 0;
  for (size_t at = 0; (at = report.find("not read by Pirate's Moon", at)) != std::string::npos; ++at) ++said;
  EXPECT(said == 2);
  EXPECT(report.find("animations read (bound to the world in play)") != std::string::npos);
  const std::vector<mech3::ModsListEntry> list = mech3::mods_list(wd, {"a", "b"});
  EXPECT(list.size() == 2);
  for (const mech3::ModsListEntry& e : list) {
    std::printf("  %s: %d unread, %zu anims shared, %zu bases\n", e.info.folder.c_str(), e.anims_unread, e.anims_taken.size(), e.anim_bases.size());
    EXPECT(e.anims_unread == 2 && e.anims_taken.empty() && e.anim_bases.empty());  // the same paths still shadow (files)
  }
  fs::remove_all(wd, ec);
}

// Modder steps 3, 7 and 8 on Pirate's Moon: a for=mw3pm mod's report read by
// PM's own readers against PM's install, its needs= and game_version= against
// mw3pm's own version (pm-v...), named for Pirate's Moon, and EXPORT checking
// it first; a MechWarrior 3 mod is "not for" this game in its report.
TEST(pm_mods_report_needs_and_checked_export) {
  if (!fs::exists(MW3PM_GAME_DIR)) return;
  MountsBack back;
  zgame::set_game(pm::table());
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_mods_report", install = MW3PM_GAME_DIR;
  std::error_code ec;
  fs::remove_all(wd, ec);
  fs::create_directories(wd + "/mods/a/zbd", ec);
  fs::create_directories(wd + "/mods/mw3only", ec);
  std::ofstream(wd + "/mods/a/mod.txt") << "name=A\nfor=mw3pm\nmech=broken\nneeds=gone\ngame_version=1.0.9\n";
  std::ofstream(wd + "/mods/a/zbd/x.bin") << "x";
  std::ofstream(wd + "/mods/mw3only/mod.txt") << "name=MW3 Only\nfor=mw3\n";
  int refused = -1;
  std::string said;
  for (const std::string& l : mech3::mod_report(wd + "/mods/a", install, "a", &refused)) said += l + "\n";
  std::printf("%s", said.c_str());
  EXPECT(refused == 1 && said.find("It adds 1 new file.") != std::string::npos);
  EXPECT(said.find("The mech broken is refused: ") != std::string::npos);
  const std::string version = mech3::mods_program_version();
  mech3::mods_program_version() = "pm-v1.0.2-12-gabc";
  std::string needs;
  for (const mech3::ModsListEntry& e : mech3::mods_list(wd, {"a"}))
    if (e.info.folder == "a")
      for (const std::string& w : e.needs) needs += w + "\n";
  std::printf("%s", needs.c_str());
  EXPECT(needs == "It needs the mod gone, which is not in the mods folder.\n"
                  "It needs Pirate's Moon Rebuild 1.0.9 or later; this is 1.0.2-12-gabc.\n");
  mech3::mods_program_version() = "pm-v1.1.0";  // new enough: no version warning
  for (const mech3::ModsListEntry& e : mech3::mods_list(wd, {"a"}))
    if (e.info.folder == "a") EXPECT(e.needs.size() == 1);
  mech3::mods_program_version() = version;
  for (const mech3::ModsListEntry& e : mech3::mods_list(wd, {}))
    if (e.info.folder == "mw3only") EXPECT(e.info.error == "it is for mw3, not Pirate's Moon");
  const mech3::ModExportResult r = mech3::mod_export(wd + "/mods/a", wd + "/a.zip", install, true);
  std::printf("  export: %s\n", r.message.c_str());
  EXPECT(r.ok && r.refused == 1 && r.message.find("The game would refuse 1 thing in it") != std::string::npos);
  fs::remove_all(wd, ec);
}

// A real mod (private files, never in the repo): MW3_REAL_MOD names a folder
// laid out as an install's (zbd\...). `--check-mod` against Pirate's Moon's
// install finds nothing the game would refuse; mounted as a mod, PM's c1m1
// launches and plays with it. Skipped without the variable.
TEST(pm_mods_real_mod_reads_and_plays_headless) {
  const char* dir = std::getenv("MW3_REAL_MOD");
  if (!dir || !fs::exists(dir) || !fs::exists(MW3PM_GAME_DIR)) return;
  MountsBack back;
  zgame::set_game(pm::table());
  const std::string install = MW3PM_GAME_DIR, wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_mods_real";
  int errors = -1;
  std::string report;
  for (const std::string& l : mech3::mod_check(dir, install, &errors)) report += l + "\n";
  std::printf("%s", report.c_str());
  EXPECT(errors == 0);
  // As a player installs it: its files copied into a folder in mods\, a mod.txt beside them.
  std::error_code ec;
  fs::remove_all(wd, ec);
  fs::create_directories(wd + "/mods/real", ec);
  std::ofstream(wd + "/mods/real/mod.txt") << "name=Real\nfor=mw3pm\n";
  fs::copy(dir, wd + "/mods/real", fs::copy_options::recursive | fs::copy_options::skip_existing, ec);
  EXPECT(!ec);
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd.c_str());
  std::string log;
  for (const std::string& l : mech3::mods_mount(wd, {"real"}, {install})) log += l + "\n";
  std::printf("%s", log.c_str());
  zutl::mount(install);
  const std::string reader = zutl::find("zbd\\reader.zbd");
  EXPECT(reader.find("/mods/real/") != std::string::npos);  // the mod's, not the install's
  EXPECT(log.find("0 replaced") == std::string::npos);  // its files seen
  in_child("pm_mods_real_mod_reads_and_plays_headless", [] {
    const Played run = play_c1m1(mech3::kMadCat, 300);
    std::printf("  core %d, frames %d; roster %d, weapons %d\n", run.core, run.frames, run.mech.roster, run.mech.weapons);
    EXPECT(run.core && run.frames >= 300 && run.made && run.mech.roster == mech3::kMadCat && run.mech.weapons > 0);
  });
  fs::remove_all(wd, ec);  // ours (made above): the private files' copy gone
}

// The save's mods on PM (docs/modding.md, The save's mods): a game saved as PM's (its zSave archives, version 2)
// with a gameplay mod holds Mods/identity, one without holds none; the load's line as on MW3.
TEST(pm_saved_game_records_its_mods) {
  const std::string wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_save_identity";
  std::error_code ec;
  fs::remove_all(wd, ec);
  fs::create_directories(wd + "/mods/world/zbd/c1", ec);
  std::ofstream(wd + "/mods/world/mod.txt") << "name=World\nfor=mw3pm\n";
  std::ofstream(wd + "/mods/world/zbd/c1/gamez.zbd") << "world";
  const mech3::ModsIdentity keep = mech3::mods_identity();
  zgame::set_game(pm::table());
  const bool had = zsave::manager() != nullptr;
  if (!had) zsave::init();
  mech3::save_mods_register();
  const auto saved = [&](const std::vector<std::string>& mods, const std::string& name) {
    mech3::mods_identity_set(wd, mods);
    const std::string path = wd + "/" + name;
    EXPECT(zsave::save(path.c_str(), 2));
    return path;
  };
  const auto has_entry = [](const std::string& path) {
    zutl_zar::Archive a;
    zutl_zar::init(a);
    const bool ok = zutl_zar::open(a, path.c_str()) && zutl_zar::find(a, "Mods/identity") >= 0;
    zutl_zar::close(a);
    zutl_zar::destroy(a);
    return ok;
  };
  const std::string plain = saved({}, "plain.mw3"), world = saved({"world"}, "world.mw3");
  EXPECT(!has_entry(plain) && has_entry(world));
  mech3::mods_identity_set(wd, {});
  mech3::save_mods_begin_load();
  EXPECT(zsave::load(world.c_str(), 2));
  const std::string said = mech3::save_mods_note();
  std::printf("  %s\n", said.c_str());
  EXPECT(said == "Saved with mods: World; none now");
  mech3::save_mods_begin_load();
  EXPECT(zsave::load(plain.c_str(), 2) && mech3::save_mods_note().empty());
  if (!had) zsave::shutdown();
  zgame::set_game(zgame::mw3_table());
  mech3::mods_identity() = keep;
}

// MODS's TEST on PM (docs/modding.md, Quick testing): the game's missions in PM's play order (numbered across
// its four worlds), after the mod's own (none here).
TEST(pm_mods_test_rows_in_play_order) {
  if (!fs::exists(MW3PM_GAME_DIR)) return;
  mech3::ModInfo m;
  m.folder = "wpn";
  std::string listed;
  for (const mech3::ModsEditRow& r : mech3::mods_test_rows(m, MW3PM_GAME_DIR)) listed += r.world + " " + std::to_string(r.number) + "; ";
  std::printf("  %s\n", listed.c_str());
  EXPECT(listed == "c1 1; c1 2; c2 3; c2 4; c1 5; c3 6; c3 7; c2 8; c4 9; c4 10; ");
}

// MODS, a mod with no missions, TEST, ENTER on c2 mission 3: PM's mission played with the editor closed and the
// mechs' definitions let go first; the pause's QUIT lands on MODS again, the stand-in pilot let go.
TEST(pm_mods_screen_test_plays_c2_mission_3_headless) {
  if (!fs::exists(MW3PM_GAME_DIR)) return;
  in_child("pm_mods_screen_test_plays_c2_mission_3_headless", [] {
    MountsBack back;
    zgame::set_game(pm::table());
    const std::string install = MW3PM_GAME_DIR, wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_mods_test";
    std::error_code ec;
    fs::remove_all(wd, ec);
    fs::create_directories(wd + "/mods/wpn", ec);
    std::ofstream(wd + "/mods/wpn/mod.txt") << "name=Weapon Test\nfor=mw3pm\n";
    zutl::mounts() = zutl::Mounts{};
    zutl::set_write_dir(wd.c_str());
    mech3::mods_mount(wd, {"wpn"}, {install});
    zutl::mount(install);
    mech3::mods_save_enabled(wd, {"wpn"});
    mech3::mods_menu_write_dir() = wd;
    mech3::mods_menu_mounted() = {"wpn"};
    mech3::mods_menu_install() = install;
    mech3::reset_states();
    if (!zsnd::state().open) zsnd::state() = zsnd::State{};
    mech3::missions() = mech3::Missions{};
    mech3::options() = mech3::Options{};
    struct Keys : platform::Input {
      std::vector<platform::KeyEvent> queue;
      uint32_t t = 0;
      void key(uint32_t dik, bool down) { queue.push_back({dik, down ? 0x80u : 0u, ++t, t}); }
      bool open(Device) override { return true; }
      void close(Device) override {}
      int32_t acquire(Device) override { return 0; }
      int32_t unacquire(Device) override { return 0; }
      int32_t keyboard_data(platform::KeyEvent* o, uint32_t* count) override {
        const uint32_t n = std::min<uint32_t>(*count, static_cast<uint32_t>(queue.size()));
        std::copy(queue.begin(), queue.begin() + n, o);
        queue.erase(queue.begin(), queue.begin() + n);
        *count = n;
        return 0;
      }
      int32_t mouse_state(platform::MouseState* m) override { return *m = platform::MouseState{}, 0; }
      int32_t joystick_state(platform::JoyState*) override { return kNotAcquired; }
    } keys;
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
    int stage = 0, core = 0, rows = 0;
    bool played = false, editor_closed = false, reloaded = false, mods_again = false;
    uint32_t real_pilot = 0, pilot_after = 0;
    {
      mech3::GameEngine engine(p, "mech3.exe -nofmv");
      engine.quiet = true;
      mech3::Mech3App app(&engine);
      mech3::set_app(&app);
      EXPECT(app.init_instance());
      mech3::States& st = mech3::states();
      engine.on_present = [&](int) {
        zgame::State* now = app.current();
        if (stage == 0 && now == &st.main && mech3::main_menu()) {
          real_pilot = engine.peek32(0x60861c);
          const_cast<mech3::MainMenu*>(mech3::main_menu())->mods.on_left_up();  // MODS
          stage = 1;
        } else if (stage == 1) {
          auto* ms = dynamic_cast<mech3::ModsState*>(now);
          auto* scr = ms ? dynamic_cast<mech3::ModsScreen*>(engine.screen_of(ms->screen_)) : nullptr;
          if (!scr) return;
          scr->mod_list.FileList::select(0);
          scr->on_button(mech3::ModsScreen::kMods, 0, 0);
          scr->on_button(mech3::ModsScreen::kTest, 0, 0);  // TEST
          rows = int(scr->edit_rows.size());
          for (size_t i = 0; i < scr->edit_rows.size(); ++i)
            if (scr->edit_rows[i].world == "c2" && scr->edit_rows[i].number == 3) scr->campaign_list.FileList::select(int32_t(i));
          const int32_t reloads = mech3::mod_test_reloads();
          scr->on_button(mech3::ModsScreen::kEnter, 0, 0);
          reloaded = mech3::mod_test_reloads() == reloads + 1;
          stage = 2;
        } else if (stage == 2 && now == &st.core) {
          ++core;
          if (core == 5) {
            played = mech3::mods_edit_session().active && !mech3::mods_edit_session().editor;
            editor_closed = mech3::editor_state().active == 0;
            keys.key(1, true);  // Escape: the pause
          }
          if (core == 6) keys.key(1, false);
        } else if (stage == 2 && now == &st.pause) {
          auto* ps = dynamic_cast<mech3::PauseScreen*>(engine.screen_of(st.pause.screen_));
          if (ps) ps->on_button(mech3::PauseScreen::kQuit, 0, 0), stage = 3;
        } else if (stage == 3 && dynamic_cast<mech3::ModsState*>(now)) {
          mods_again = true;
          pilot_after = engine.peek32(0x60861c);
          stage = 4;
        }
      };
      Pump pump;
      pump.limit = 8000;
      pump.stop = [&] { return stage == 4; };
      app.run(pump);
      mech3::missions().cleanup(false);
      app.stop_game();
      app.exit_instance();
      mech3::set_app(nullptr);
    }
    std::printf("  stage %d, %d rows; played %d, the editor closed %d, reloaded %d, MODS again %d, the pilot back %d\n", stage,
                rows, played, editor_closed, reloaded, mods_again, pilot_after == real_pilot);
    EXPECT(rows == 10 && played && editor_closed && reloaded);
    EXPECT(mods_again && pilot_after == real_pilot && !mech3::mods_edit_session().active);
    mech3::mods_menu_write_dir().clear();
    mech3::mods_menu_mounted().clear();
    mech3::mods_menu_install().clear();
    zgame::set_game(zgame::mw3_table());
  });
}

// The starter mods on PM (docs/modding.md, Starter mods): each made from PM's install (for=mw3pm, its own message
// numbers), --check-mod and the report clean, then loaded: Thor B's loadouts with 1.5 times the Thor's accel_rate,
// ERPPC_B 1.25 times ERPPC's damage, the menu's messages in German, the one-mission campaign of PM's c1 1.
TEST(pm_starter_mods_made_checked_and_loaded) {
  if (!fs::exists(MW3PM_GAME_DIR)) return;
  MountsBack back;
  zgame::set_game(pm::table());
  const zwep::InitHooks hooks = zwep::init_hooks();
  zwep::init_hooks() = zwep::InitHooks{};
  const std::string install = MW3PM_GAME_DIR, wd = std::string(MW3_TEST_TMP_ROOT) + "/pm_starters", mods = wd + "/mods";
  std::error_code ec;
  fs::remove_all(wd, ec);
  for (const std::string& k : mech3::starter_kinds()) {
    const mech3::StarterMade m = mech3::starter_make(k, install, mods);
    int errors = -1, refused = -1;
    std::string notes;
    for (const std::string& l : mech3::mod_check(m.path, install, &errors))
      if (l.rfind("warning", 0) == 0 || l.rfind("error", 0) == 0) notes += l + "\n";
    for (const std::string& l : mech3::mod_report(m.path, install, "", &refused))
      if (l.rfind("Note", 0) == 0 || l.find("replaces") != std::string::npos) notes += l + "\n";
    std::printf("  %s: %s%s", k.c_str(), m.ok ? "" : (m.message + " ").c_str(), notes.empty() ? "clean\n" : notes.c_str());
    EXPECT(m.ok && errors == 0 && refused == 0 && notes.empty());
  }
  std::ifstream head(mods + "/starter_mech/mod.txt");
  const std::string mod_txt((std::istreambuf_iterator<char>(head)), std::istreambuf_iterator<char>());
  EXPECT(mod_txt.find("for=mw3pm\n") != std::string::npos);
  zutl::mounts() = zutl::Mounts{};
  zutl::set_write_dir(wd.c_str());
  mech3::mods_mount(wd, {"starter_mech", "starter_weapon", "starter_translation", "starter_mission"}, {install});
  zutl::mount(install);
  zrdr::init();
  EXPECT(zrdr::open_archive("zbd\\reader.zbd", true));
  const int32_t thor_b = mech3::mech_index("thor_b"), thor = mech3::mech_index("thor");
  const auto accel = [](int32_t mech) {
    mech3::MechDfn* d = mech3::mech_dfn(mech);
    return d && mech3::read_dfn(*d) && d->configs && !d->configs->configs.empty() ? d->configs->configs[0]->accel_rate : -1.0f;
  };
  const float a_b = accel(thor_b), a = accel(thor);
  zwep::init(nullptr, "weapons.zrd", 0, nullptr);
  const zwep::WeaponDef* b = zwep::find_def("ERPPC_B");
  const zwep::WeaponDef* e = zwep::find_def("ERPPC");
  const float dmg_b = b ? b->damage : -1, dmg = e ? e->damage : -1;
  zwep::shutdown(nullptr);
  zutl::MessageDll dll;
  EXPECT(zutl::read_message_dll(install + "/Mech3Msg.dll", dll));
  std::map<int32_t, std::string> text = dll.text;
  mech3::mods_apply_strings(text, dll.ids);
  const std::string campaign = text[dll.ids["MSG_CAMPAIGN_BTN"]];
  const mech3::ModCampaign* one = nullptr;
  for (const mech3::ModCampaign& c : mech3::mod_campaigns())
    if (c.mod == "starter_mission") one = &c;
  std::printf("  Thor B %d accel %.3f (the Thor's %.3f); ER PPC B damage %.3f (ERPPC %.3f); \"%s\"; campaign %s\n", thor_b,
              a_b, a, dmg_b, dmg, campaign.c_str(), one ? one->name.c_str() : "none");
  EXPECT(thor_b > thor && thor >= 0 && std::fabs(a_b - a * 1.5f) < 1e-4f);
  EXPECT(b && e && std::fabs(dmg_b - dmg * 1.25f) < 1e-4f);
  EXPECT(campaign == "K A M P A G N E");
  EXPECT(one && one->error.empty() && one->missions.size() == 1 && one->missions[0].world == "c1");
  zrdr::close_archives(true);
  mech3::free_dfns();
  zwep::init_hooks() = hooks;
  zgame::set_game(zgame::mw3_table());
}
