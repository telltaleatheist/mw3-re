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
#include "gamez/zsound/zsnd.h"
#include "gamez/zutil/zutl_path.h"
#include "gamez/zutil/zutl_zar.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_mech.h"
#include "mech3/app_states.h"
#include "mech3/mech_control.h"
#include "mech3/mech_ids.h"
#include "mech3/mech_reader.h"
#include "mech3/mission.h"
#include "mech3/mods.h"
#include "mech3/options.h"
#include "mech3/player.h"
#include "platform/silent_sound.h"
#include "pm/pm_game.h"

namespace mech3 {  // mech_names.h (its Engine clashes with the app's)
int32_t mech_count();
const char* mech_title(int32_t mech);
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
