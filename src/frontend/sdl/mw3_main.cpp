// mw3: MechWarrior 3 rebuilt, on SDL3 (docs/program.md). The original's own
// start-up runs (the app constructor, InitInstance, the engine start, the
// main loop zGameApp::Run and its game states) with the platform stand-ins of
// src/frontend/sdl underneath; what it reaches that is not ported is logged
// once by its original address and does nothing.
//
//   mw3 [install-dir] [options] [-nofmv] [-seasick] [-cdN]
//
//   --install DIR        the MW3 1.2 folder (default: mw3.cfg's install=, else game/mw3-1.2)
//   --assets DIR         a folder of replacement files searched first (repeatable)
//   --write-dir DIR      where saves and settings go (default: the user's app data)
//   --mission NAME       debug start into a mission through the Load and Core
//                        states: c1, c1m2 (mission 2 of c1), c4b, t1 ...
//   --fly NAME           debug start: the world alone with a free camera
//     --variant BLOCK    the world.zrd block (default m1 or the first)
//     --pos X Y Z, --look YAW PITCH (degrees): the camera's start
//   --dialog FILE NAME   debug start showing one zTwoD dialog, e.g. dialog.zrd MAINMENU
//   --shot FILE.bmp      save frame N (--shot-frame N, default 30) and quit; hides the window
//     --shot-any         count every frame shown (the fade's too), not only the 3D scene's
//   --frames N           quit after N frames of a debug start
//   --window WxH         window size (default 1280x960)
//   --nosound            no audio device (a silent DirectSound stand-in)
//   --hidden             keep the window hidden
//   --verbose            list every unported function reached at exit
#include <SDL3/SDL.h>

#include <cstdio>
#include <deque>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "frontend/sdl/debug_states.h"
#include "frontend/sdl/sdl_input.h"
#include "frontend/sdl/sdl_music.h"
#include "frontend/sdl/sdl_platform.h"
#include "frontend/sdl/sdl_sound.h"
#include "frontend/sdl/sdl_twod_host.h"
#include "gamez/ztwod/twod_host.h"
#include "gamez/zutil/zutl_path.h"
#include "mech3/app.h"
#include "mech3/app_engine.h"
#include "mech3/app_hud.h"
#include "mech3/app_mech.h"
#include "mech3/app_states.h"
#include "gamez/zinput/zinput.h"
#include "gamez/zweapon/zweapon.h"
#include "mech3/mech_control.h"
#include "mech3/mission.h"
#include "platform/reg_file.h"
#include "platform/silent_sound.h"

namespace {

struct Args {
  std::string install, write_dir, mission, variant, dialog_file, dialog_name, shot;
  std::vector<std::string> assets;
  std::string original;  // -nofmv -seasick -cdN, passed on as the command line
  float pos[3] = {0, 60, 0};
  float look[2] = {0, -2.9f};
  bool have_pos = false, nosound = false, hidden = false, verbose = false, fly = false;
  int shot_frame = 30, frames = 0, win_w = 1280, win_h = 960, fire = 0, fire_group = 0, objectives = 0;
  int click = 0, click_up = 0, shot_series = 0;
  struct Tap { int down, up, dik; };
  std::vector<Tap> taps;  // --tap DIK@N[:M]
  bool walk = false;  // --walk: full throttle (THROTTLE0's key) at the start (ours)
  bool shot_any = false;
};

void usage() {
  std::printf(
      "mw3 [install-dir] [--install DIR] [--assets DIR]... [--write-dir DIR]\n"
      "    [--mission NAME | --fly NAME [--variant BLOCK]] [--pos X Y Z] [--look YAW PITCH] [--dialog FILE NAME]\n"
      "    [--shot FILE.bmp [--shot-frame N] [--shot-any] [--shot-series K]] [--fire N [--fire-group 1..5]] [--objectives N]\n"
      "    [--click N[:M]] [--walk] [--tap DIK@N[:M]]... [--frames N] [--window WxH] [--nosound] [--hidden] [--verbose]\n"
      "    [-nofmv] [-seasick] [-cdN]\n"
      "See docs/program.md.\n");
}

bool parse(int argc, char** argv, Args& a) {
  for (int i = 1; i < argc; ++i) {
    const std::string s = argv[i];
    auto next = [&](int n) { return i + n < argc; };
    if (s == "--help" || s == "-h") return usage(), false;
    else if (s == "--install" && next(1)) a.install = argv[++i];
    else if (s == "--assets" && next(1)) a.assets.emplace_back(argv[++i]);
    else if (s == "--write-dir" && next(1)) a.write_dir = argv[++i];
    else if (s == "--mission" && next(1)) a.mission = argv[++i];
    else if (s == "--fly" && next(1)) a.mission = argv[++i], a.fly = true;
    else if (s == "--variant" && next(1)) a.variant = argv[++i];
    else if (s == "--dialog" && next(2)) a.dialog_file = argv[++i], a.dialog_name = argv[++i];
    else if (s == "--shot" && next(1)) a.shot = argv[++i];
    else if (s == "--shot-frame" && next(1)) a.shot_frame = std::atoi(argv[++i]);
    else if (s == "--shot-any") a.shot_any = true;
    else if (s == "--shot-series" && next(1)) a.shot_series = std::atoi(argv[++i]);
    else if (s == "--fire" && next(1)) a.fire = std::atoi(argv[++i]);
    else if (s == "--fire-group" && next(1)) a.fire_group = std::atoi(argv[++i]);
    else if (s == "--objectives" && next(1)) a.objectives = std::atoi(argv[++i]);
    else if (s == "--frames" && next(1)) a.frames = std::atoi(argv[++i]);
    else if (s == "--window" && next(1)) std::sscanf(argv[++i], "%dx%d", &a.win_w, &a.win_h);
    else if (s == "--nosound") a.nosound = true;
    else if (s == "--walk") a.walk = true;
    else if (s == "--tap" && next(1)) {
      const char* v = argv[++i];
      const char* at = std::strchr(v, '@');
      const char* c = at ? std::strchr(at, ':') : nullptr;
      if (at) a.taps.push_back({std::atoi(at + 1), c ? std::atoi(c + 1) : std::atoi(at + 1) + 1,
                                static_cast<int>(std::strtol(v, nullptr, 0))});
    }
    else if (s == "--click" && next(1)) {
      const char* v = argv[++i];
      a.click = std::atoi(v);
      const char* c = std::strchr(v, ':');
      a.click_up = c ? std::atoi(c + 1) : a.click + 3;
    }
    else if (s == "--hidden") a.hidden = true;
    else if (s == "--verbose") a.verbose = true;
    else if (s == "--pos" && next(3)) {
      for (float& v : a.pos) v = std::strtof(argv[++i], nullptr);
      a.have_pos = true;
    } else if (s == "--look" && next(2)) {
      for (float& v : a.look) v = std::strtof(argv[++i], nullptr);
    } else if (s.size() > 1 && s[0] == '-' && s[1] != '-') {
      a.original += " " + s;  // the exe's own switches (startup.md, Command line)
    } else if (s[0] != '-' && a.install.empty()) {
      a.install = s;
    } else {
      std::fprintf(stderr, "mw3: unknown argument %s\n", s.c_str());
      return usage(), false;
    }
  }
  return true;
}

// mw3.cfg in the write dir: install=DIR and assets=DIR lines.
void read_config(const std::string& dir, Args& a) {
  std::ifstream f(dir + "/mw3.cfg");
  std::string line;
  while (std::getline(f, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.rfind("install=", 0) == 0 && a.install.empty()) a.install = line.substr(8);
    else if (line.rfind("assets=", 0) == 0) a.assets.push_back(line.substr(7));
  }
}

// "c1m2" is zbd/c1 with world block m2 and mission number 2; "c4b" is a dir.
void split_mission(const std::string& name, std::string& dir, std::string& variant, int& number) {
  dir = name, number = 1;
  const size_t m = name.rfind('m');
  if (m != std::string::npos && m > 0 && m + 1 < name.size() &&
      name.find_first_not_of("0123456789", m + 1) == std::string::npos && !zutl::exists(("zbd\\" + name + "\\gamez.zbd").c_str())) {
    dir = name.substr(0, m);
    if (variant.empty()) variant = name.substr(m);
    number = std::atoi(name.c_str() + m + 1);
  }
}

int run(const Args& a);

}  // namespace

int main(int argc, char** argv) {
  std::setvbuf(stdout, nullptr, _IOLBF, 0);  // in order with the log on stderr
  Args a;
  if (!parse(argc, argv, a)) return 2;
  if (!a.shot.empty()) a.hidden = true;
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK)) {
    std::fprintf(stderr, "mw3: SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  // The write dir: saves and settings, outside the install (assets-layer.md).
  if (a.write_dir.empty()) {
    if (char* p = SDL_GetPrefPath("MicroProse", "MechWarrior 3")) a.write_dir = p, SDL_free(p);
  }
  while (a.write_dir.size() > 1 && a.write_dir.back() == '/') a.write_dir.pop_back();
  if (!a.write_dir.empty()) read_config(a.write_dir, a);
#ifdef MW3_GAME_DIR
  if (a.install.empty()) a.install = MW3_GAME_DIR;
#endif
  for (const std::string& d : a.assets) zutl::mount(d);
  zutl::mount(a.install);
  zutl::set_write_dir(a.write_dir);
  std::error_code ec;
  if (!std::filesystem::exists(a.install + "/zbd/reader.zbd", ec)) {
    const std::string msg = "No MechWarrior 3 1.2 install at \"" + a.install +
                            "\". Start mw3 with the folder that holds Mech3.exe and zbd/ (docs/program.md).";
    std::fprintf(stderr, "mw3: %s\n", msg.c_str());
    if (!a.hidden) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "MechWarrior 3", msg.c_str(), nullptr);
    SDL_Quit();
    return 1;
  }
  std::printf("mw3: install %s, write dir %s\n", a.install.c_str(), a.write_dir.c_str());

  const int result = run(a);
  SDL_Quit();  // after every SDL object above is gone
  return result;
}

namespace {

int run(const Args& a) {
  sdlfe::SdlWindow window;
  if (!window.init(a.hidden)) return 1;
  sdlfe::SdlInput input;
  input.window = window.sdl();
  input.mickey_scale = a.win_w > 0 ? 640.0f / static_cast<float>(a.win_w) : 1.0f;
  sdlfe::SdlZinHost zin_host(&input);
  zin::set_host(&zin_host);
  sdlfe::SdlClock clock;
  sdlfe::SdlAlert alert(window.sdl(), a.hidden);
  sdlfe::SdlScreen2D screen(window.render());
  std::unique_ptr<platform::SoundOutput> sound;
  if (a.nosound) sound = std::make_unique<platform::SilentSoundOutput>();
  else sound = std::make_unique<sdlfe::SdlSoundOutput>();
  sdlfe::SdlMusicOutput music;
  mech3::MountFiles files;

  mech3::Platform p;
  p.window = &window, p.alert = &alert, p.clock = &clock, p.input = &input, p.sound = sound.get();
  p.music = &music, p.files = &files, p.render = window.render(), p.screen = &screen;
  p.window_width = a.win_w, p.window_height = a.win_h;
  // The registry's keys as a file in the write dir (docs/settings.md).
  platform::RegFile settings(a.write_dir.empty() ? std::string() : a.write_dir + "/settings.reg");
  if (!a.write_dir.empty()) p.settings = &settings;
  p.ram_mb = static_cast<uint32_t>(SDL_GetSystemRAM());

  int result = 1;
  {
    mech3::GameEngine engine(p, "mech3.exe" + a.original);
    sdlfe::SdlTwodHost host(&engine, &input, &window);
    ztwod::set_host(&host);
    mech3::reset_states();
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);

    std::unique_ptr<mech3::State> debug;
    bool launch = false, launch_done = false;
    if (!a.mission.empty() && !a.fly) {
      // Through the original's states: the Load state loads the mission
      // (mech3/mission), then the Core state runs it (docs/mission-load.md).
      std::string dir, variant;
      mech3::GameEngine::Launch l;
      split_mission(a.mission, dir, variant, l.number);
      l.index = -1;
      for (int32_t i = 0; i < 9; ++i)
        if (dir == mech3::mission_dir(i)) l.index = i;
      if (l.index < 0) {
        std::fprintf(stderr, "mw3: --mission %s: not a mission (c1 c2 c3 c4 c4b t1)\n", a.mission.c_str());
        return 2;
      }
      l.have_pos = a.have_pos;
      for (int i = 0; i < 3; ++i) l.eye[i] = a.pos[i];
      l.yaw = a.look[0] * 0.0174533f, l.pitch = a.look[1] * 0.0174533f;
      engine.set_launch(l);
      launch = true;
      int shots = 0;
      // --fire N (ours, for checking the weapons' effects): from the N-th frame
      // shown, the fire command reads as held.
      static bool s_fire = false;
      if (a.fire > 0)
        mech3::controls_calls().command = [](int cmd) {
          return zin::command(cmd) | (s_fire && cmd == 0x3d ? 1u : 0u);
        };
      engine.on_present = [&, shots](int) mutable {
        if (!a.shot_any && !engine.drew_scene()) return;
        ++shots;
        if (a.fire > 0 && shots == a.fire) {
          s_fire = true;
          void* m = engine.object_of(engine.field32(engine.pilot(), 0x14));
          const int32_t g = a.fire_group > 0 ? (mech3::mech_select_group(m, a.fire_group - 1), a.fire_group - 1)
                                              : mech3::mech_select_hot_group(m);
          std::printf("mw3: firing group %d: %s\n", g + 1, mech3::mech_group_names(m, g).c_str());
        }
        // --objectives N (ours, for checking): F12's box shown from the N-th frame.
        if (a.objectives > 0 && shots == a.objectives) mech3::program_hud_objectives_toggle();
        if (a.walk && (shots == 2 || shots == 4))  // THROTTLE0's key (100%) pressed and let go
          for (int sc = 0; sc < SDL_SCANCODE_COUNT; ++sc)
            if (sdlfe::dik_from_scancode(static_cast<SDL_Scancode>(sc)) == (zin::key1_of(0x24) & 0xff)) {
              SDL_Event ev{};
              ev.type = shots == 2 ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
              ev.key.scancode = static_cast<SDL_Scancode>(sc);
              ev.key.down = shots == 2;
              SDL_PushEvent(&ev);
              break;
            }
        // --tap DIK@N[:M] (ours, for checking): the key pressed after the N-th
        // frame, let go after the M-th (the next by default).
        for (const Args::Tap& t : a.taps)
          if (shots == t.down || shots == t.up)
            for (int sc = 0; sc < SDL_SCANCODE_COUNT; ++sc)
              if (sdlfe::dik_from_scancode(static_cast<SDL_Scancode>(sc)) == t.dik) {
                SDL_Event ev{};
                ev.type = shots == t.down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
                ev.key.scancode = static_cast<SDL_Scancode>(sc);
                ev.key.down = shots == t.down;
                SDL_PushEvent(&ev);
                break;
              }
        // --click N[:M] (ours, for checking the fire): the left button pressed
        // as an SDL event after the N-th frame, let go after the M-th; each
        // frame from N logged (the current group's first weapon as a beam).
        if (a.click > 0 && a.fire_group > 0 && shots == a.click - 1) {
          void* m = engine.object_of(engine.field32(engine.pilot(), 0x14));
          mech3::mech_select_group(m, a.fire_group - 1);
          std::printf("mw3: group %d: %s\n", a.fire_group, mech3::mech_group_names(m, a.fire_group - 1).c_str());
        }
        if (a.click > 0 && (shots == a.click || shots == a.click_up)) {
          SDL_Event ev{};
          ev.type = shots == a.click ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
          ev.button.button = SDL_BUTTON_LEFT;
          ev.button.down = shots == a.click;
          SDL_PushEvent(&ev);
        }
        if (a.click > 0 && shots >= a.click && shots < a.click + 40) {
          void* m = engine.object_of(engine.field32(engine.pilot(), 0x14));
          mech3::BeamProbe b;
          if (mech3::mech_group_beam(m, mech3::mech_current_group(m), &b)) {
            const float off = std::sqrt((b.origin[0] - b.live[0]) * (b.origin[0] - b.live[0]) +
                                        (b.origin[1] - b.live[1]) * (b.origin[1] - b.live[1]) +
                                        (b.origin[2] - b.live[2]) * (b.origin[2] - b.live[2]));
            std::printf("mw3: frame %d dt %.1f ms: trigger %d/%d state %d, firing %d beam %d, ready %.2f, origin off %.2f m\n",
                        shots, engine.frame_dt() * 1000.0f, b.trigger_down, b.trigger_was, b.state, b.firing,
                        b.beam_active, b.ready, b.beam_active ? off : 0.0f);
          } else {
            const int32_t g = mech3::mech_current_group(m);
            std::printf("mw3: frame %d dt %.1f ms: group %d ammo %d ready %.2f\n", shots, engine.frame_dt() * 1000.0f,
                        g + 1, mech3::mech_group_ammo(m, g), mech3::mech_group_ready(m, g));
          }
        }
        // --shot-series K (ours, for checking flicker): the K frames up to the
        // shot's each into FILE_<frame>.bmp.
        static std::deque<std::string> series;
        if (!a.shot.empty() && a.shot_series > 1 && shots >= a.shot_frame - a.shot_series && shots < a.shot_frame - 1) {
          std::string base = a.shot;
          if (base.size() > 4 && base.compare(base.size() - 4, 4, ".bmp") == 0) base.resize(base.size() - 4);
          series.push_back(base + "_" + std::to_string(shots + 1) + ".bmp");
          window.render()->capture_next(series.back().c_str());
        }
        if (!a.shot.empty() && shots == a.shot_frame - 1) window.render()->capture_next(a.shot.c_str());
        if (!a.shot.empty() && shots == a.shot_frame) std::printf("mw3: wrote %s (frame %d)\n", a.shot.c_str(), shots);
        if (a.fire > 0 && shots >= a.fire && (shots - a.fire) % 5 == 0) {
          zwep::PoolStats ps;
          zwep::pool_stats(&ps);
          std::printf("mw3: frame %d, %d shots in flight\n", shots, ps.flying);
        }
        if ((!a.shot.empty() && shots >= a.shot_frame) || (a.frames > 0 && shots >= a.frames)) launch_done = true;
      };
    } else if (!a.mission.empty()) {
      sdlfe::DebugMissionOptions o;
      o.variant = a.variant;
      split_mission(a.mission, o.mission, o.variant, o.number);
      if (a.have_pos) for (int i = 0; i < 3; ++i) o.eye[i] = a.pos[i];
      o.yaw = a.look[0] * 0.0174533f, o.pitch = a.look[1] * 0.0174533f;
      o.shot = a.shot, o.shot_frame = a.shot_frame, o.frames = a.frames;
      debug = std::make_unique<sdlfe::DebugMissionState>(&engine, &input, window.render(), o);
    } else if (!a.dialog_file.empty()) {
      debug = std::make_unique<sdlfe::DebugDialogState>(&engine, &input, window.render(), a.dialog_file,
                                                        a.dialog_name, a.shot, a.shot_frame);
    }

    if (app.init_instance()) {
      // The state after the intro (the app's +0x1dc): the debug start takes
      // the place the lobby launch gives the Load state.
      if (debug) app.post_intro_ = debug.get();
      if (launch) app.post_intro_ = &mech3::states().load;
      if (!debug && !launch)
        std::printf("mw3: the main menu (0x00486260) and the screens are not ported yet; the window stays "
                    "dark. Start a world with --mission c1 or a dialog with --dialog dialog.zrd MAINMENU.\n");
      sdlfe::SdlPump pump(&input, &app);
      if (!debug && !launch && a.frames > 0) pump.should_quit = [&] { return screen.frames >= a.frames; };
      if (launch) pump.should_quit = [&] { return launch_done; };
      result = app.run(pump);
      if (app.current() != &mech3::states().exit) app.stop_game();
    }
    app.destroy();  // the app's destructor: the settings saved
    if (a.verbose) engine.print_unported_summary();
    else std::fprintf(stderr, "mw3: %zu original functions reached are not ported (--verbose lists them)\n",
                      engine.unported_counts().size());
    ztwod::set_host(nullptr);
    mech3::set_app(nullptr);
  }
  zin::set_host(nullptr);
  return result;
}

}  // namespace
