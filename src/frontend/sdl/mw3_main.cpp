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
//   --mission NAME       debug start straight into a world: c1, c1m2, c4b, t1 ...
//     --variant BLOCK    the world.zrd block (default m1 or the first)
//     --pos X Y Z, --look YAW PITCH (degrees)
//   --dialog FILE NAME   debug start showing one zTwoD dialog, e.g. dialog.zrd MAINMENU
//   --shot FILE.bmp      save frame N (--shot-frame N, default 30) and quit; hides the window
//   --frames N           quit after N frames of a debug start
//   --window WxH         window size (default 1280x960)
//   --nosound            no audio device (a silent DirectSound stand-in)
//   --hidden             keep the window hidden
//   --verbose            list every unported function reached at exit
#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
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
#include "mech3/app_states.h"
#include "platform/silent_sound.h"

namespace {

struct Args {
  std::string install, write_dir, mission, variant, dialog_file, dialog_name, shot;
  std::vector<std::string> assets;
  std::string original;  // -nofmv -seasick -cdN, passed on as the command line
  float pos[3] = {0, 60, 0};
  float look[2] = {0, -2.9f};
  bool have_pos = false, nosound = false, hidden = false, verbose = false;
  int shot_frame = 30, frames = 0, win_w = 1280, win_h = 960;
};

void usage() {
  std::printf(
      "mw3 [install-dir] [--install DIR] [--assets DIR]... [--write-dir DIR]\n"
      "    [--mission NAME [--variant BLOCK] [--pos X Y Z] [--look YAW PITCH]] [--dialog FILE NAME]\n"
      "    [--shot FILE.bmp [--shot-frame N]] [--frames N] [--window WxH] [--nosound] [--hidden] [--verbose]\n"
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
    else if (s == "--variant" && next(1)) a.variant = argv[++i];
    else if (s == "--dialog" && next(2)) a.dialog_file = argv[++i], a.dialog_name = argv[++i];
    else if (s == "--shot" && next(1)) a.shot = argv[++i];
    else if (s == "--shot-frame" && next(1)) a.shot_frame = std::atoi(argv[++i]);
    else if (s == "--frames" && next(1)) a.frames = std::atoi(argv[++i]);
    else if (s == "--window" && next(1)) std::sscanf(argv[++i], "%dx%d", &a.win_w, &a.win_h);
    else if (s == "--nosound") a.nosound = true;
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

  int result = 1;
  {
    mech3::GameEngine engine(p, "mech3.exe" + a.original);
    sdlfe::SdlTwodHost host(&engine, &input, &window);
    ztwod::set_host(&host);
    mech3::reset_states();
    mech3::Mech3App app(&engine);
    mech3::set_app(&app);

    std::unique_ptr<mech3::State> debug;
    if (!a.mission.empty()) {
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
      if (!debug)
        std::printf("mw3: the main menu (0x00486260) and the screens are not ported yet; the window stays "
                    "dark. Start a world with --mission c1 or a dialog with --dialog dialog.zrd MAINMENU.\n");
      sdlfe::SdlPump pump(&input, &app);
      if (!debug && a.frames > 0) pump.should_quit = [&] { return screen.frames >= a.frames; };
      result = app.run(pump);
      if (app.current() != &mech3::states().exit) app.stop_game();
    }
    if (a.verbose) engine.print_unported_summary();
    else std::fprintf(stderr, "mw3: %zu original functions reached are not ported (--verbose lists them)\n",
                      engine.unported_counts().size());
    ztwod::set_host(nullptr);
    mech3::set_app(nullptr);
  }
  return result;
}

}  // namespace
