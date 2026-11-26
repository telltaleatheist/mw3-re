// mw3launcher (ours, not original code; docs/launcher.md, The two programs;
// docs/games-layout.md): the rebuild's main icon. It finds the install, runs
// SETUP and starts the game the player picks as its own program, mw3 beside
// it, hiding while the game runs. mw3 itself starts without it.
//
//   mw3launcher [--version] [--write-dir DIR] [--windowed | --window WxH] [--nosound] [--no-import]
//               [--launcher-shot FILE [--launcher-screen main|setup|work|notes|found]]
//               [--launcher-play] [--launcher-mw1]
#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "platform/pointer_hands_off.h"
#include "frontend/sdl/crash_report.h"
#include "frontend/sdl/launcher_folder.h"
#include "frontend/sdl/launcher.h"
#include "frontend/sdl/launcher_own.h"
#include "frontend/sdl/run_log.h"
#include "frontend/sdl/sdl_child.h"

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#ifndef _WIN32
#include <unistd.h>
#endif

namespace {
namespace fs = std::filesystem;

// This program's own folder (an app bundle's Contents/MacOS, the AppImage's
// usr/bin), where the game is too; SDL_GetBasePath is the bundle's Resources.
fs::path own_dir() {
#if defined(__APPLE__)
  char buf[4096];
  uint32_t n = sizeof buf;
  if (_NSGetExecutablePath(buf, &n) == 0) return fs::weakly_canonical(fs::path(buf)).parent_path();
#elif defined(_WIN32)
  wchar_t buf[4096];
  const DWORD n = GetModuleFileNameW(nullptr, buf, 4096);
  if (n > 0 && n < 4096) return fs::path(std::wstring(buf, n)).parent_path();
#else
  std::error_code ec;
  const fs::path self = fs::read_symlink("/proc/self/exe", ec);
  if (!ec) return self.parent_path();
#endif
  const char* base = SDL_GetBasePath();
  return base ? fs::path(base) : fs::path(".");
}

// A program beside us; the variable names another (checks: a stand-in).
std::string beside(const char* env, const char* name) {
  if (const char* e = std::getenv(env)) return e;
#if defined(_WIN32)
  const fs::path p = own_dir() / (std::string(name) + ".exe");
#else
  const fs::path p = own_dir() / name;
#endif
  std::error_code ec;
  return fs::is_regular_file(p, ec) ? p.string() : std::string();
}

// mw3.cfg's install= and mw1_game= (the game's own settings file, shared).
void read_config(const std::string& write_dir, sdlfe::LauncherOptions& o) {
  std::ifstream f(fs::path(write_dir) / "mw3.cfg");
  std::string line;
  while (std::getline(f, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.rfind("install=", 0) == 0) o.cfg_install = sdlfe::own::path_from_config(line.substr(8));
    if (line.rfind("mw1_game=", 0) == 0) o.cfg_mw1_game = sdlfe::own::path_from_config(line.substr(9));
    if (line.rfind("pm_install=", 0) == 0) o.cfg_pm_install = sdlfe::own::path_from_config(line.substr(11));
    if (line.rfind("pm_notes=", 0) == 0) o.cfg_pm_notes = line.substr(9);
    if (line.rfind("mw3_notes=", 0) == 0) o.cfg_mw3_notes = line.substr(10);
    if (line.rfind("game_start=", 0) == 0) o.cfg_game_start = line.substr(11);
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (sdlfe::crash_box_main(argc, argv)) return 0;
#ifndef _WIN32
  // MW3 alone's AppImage starts this launcher: asked its status, or started by another launcher that linked it
  // (docs/launcher.md, Copies installed without the launcher), it is the game beside this one that answers.
  for (int i = 1; i < argc; ++i)
    if (const std::string s = argv[i]; s == "--status" || s == "--setup" || s == "--ready-file") {
      if (const std::string game = beside("MW3_GAME_BIN", "mw3"); !game.empty()) {
        argv[0] = const_cast<char*>(game.c_str());
        execv(game.c_str(), argv);
        std::perror("mw3launcher: the game beside it");
        return 1;
      }
      break;
    }
#endif
  sdlfe::LauncherOptions o;
  bool windowed = false, window_given = false, hidden_log = false;
  for (int i = 1; i < argc; ++i) {
    const std::string s = argv[i];
    auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : std::string(); };
    if (s == "--version") {  // as mw3's: the build (the package's version), nothing opened or written
      std::printf("mw3launcher (MechWarrior 3 rebuild) %s\n", MW3_VERSION);
      return 0;
    }
    if (s == "--write-dir") o.write_dir = next();
    else if (s == "--windowed") windowed = true;
    else if (s == "--window") {
      const std::string v = next();
      if (std::sscanf(v.c_str(), "%dx%d", &o.win_w, &o.win_h) == 2) window_given = true;
    } else if (s == "--nosound") o.sound = false;
    else if (s == "--hidden") o.hidden = true, platform::set_pointer_hands_off(true);  // no picker, no pointer
    else if (s == "--no-import") o.no_import = true;
    else if (s == "--launcher-shot") o.shot = next(), hidden_log = true, platform::set_pointer_hands_off(true);
    else if (s == "--launcher-screen") o.screen = next();
    else if (s == "--launcher-play") o.play = true;
    else if (s == "--launcher-mw1") o.play_mw1 = true;
    else if (s == "--launcher-pm") o.play_pm = true;
    else {
      std::fprintf(stderr,
                   "usage: mw3launcher [--version] [--write-dir DIR] [--windowed | --window WxH] [--nosound] [--hidden] [--no-import]\n"
                   "    [--launcher-shot FILE [--launcher-screen main|setup|work|notes|found]] [--launcher-play] [--launcher-mw1]\n");
      return 2;
    }
  }
  if (platform::pointer_hands_off()) SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "1");  // as mw3_main: no focus taken
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::fprintf(stderr, "mw3launcher: SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  const bool write_dir_given = !o.write_dir.empty();
  o.write_dir_fixed = write_dir_given;
  bool pointer = false;  // --write-dir wins; else the usual folder, or the one its pointer names
  if (!write_dir_given && !sdlfe::own::data_folder_at_start(&o.write_dir, !hidden_log && !o.hidden, &pointer)) {
    SDL_Quit();
    return 1;
  }
  while (o.write_dir.size() > 1 && o.write_dir.back() == '/') o.write_dir.pop_back();
  std::error_code ec;
  if (!o.write_dir.empty()) fs::create_directories(o.write_dir, ec);
  // Its own log beside the game's: mw3launcher.log (the game's is mw3.log).
  sdlfe::RunLog run_log;
  if (!hidden_log && !sdlfe::log_to_file(o.write_dir, "mw3launcher") && !o.write_dir.empty())
    run_log.open(o.write_dir, "mw3launcher");
  sdlfe::own::repair_old_names(o.write_dir);
  std::printf("mw3launcher (MechWarrior 3 rebuild) %s, write dir %s\n", MW3_VERSION, o.write_dir.c_str());
  if (!hidden_log && !o.write_dir.empty()) sdlfe::crash_report_install("mw3launcher", o.write_dir + "/mw3launcher.log", true);
  // Pirate's Moon's folder in place before its saved path is read (family_move; its old nested folder mw3pm's own).
  if (!write_dir_given && !std::getenv("MW3_USUAL_FOLDER")) sdlfe::own::usual_folder("Pirate's Moon");
  read_config(o.write_dir, o);
  // Windowed always (2026-11-20): on the Mac a fullscreen launcher is a fullscreen game to macOS (the bundle's
  // games category), so Game Mode slowed the file picker to 7-12 s (docs/launcher.md). The games keep fullscreen.
  o.fullscreen = false;
  o.program_dir = own_dir().string();
  o.game_bin = beside("MW3_GAME_BIN", "mw3");
  o.pm_bin = beside("MW3_PM_BIN", "mw3pm");
  o.mw1_bin = beside("MW3_MW1_BIN", "mw1");  // MechWarrior 1 Remake, when there
  o.mw1r_bin = beside("MW3_MW1R_BIN", "mw1r");  // MW1 Rebuild (MW1's ORIGINAL tab)
  o.chi_bin = beside("MW3_CHI_BIN", "chi");        // Inception Rebuild (INCEPTION's ORIGINAL tab)
  o.chirm_bin = beside("MW3_CHIRM_BIN", "chirm");  // Inception Remake (its REMAKE tab)
  if (const char* d = std::getenv("MW3_PM_WRITE_DIR")) o.pm_write_dir = d;  // checks
  if (o.game_bin.empty()) std::printf("mw3launcher: no mw3 beside this program: MECHWARRIOR 3 cannot start\n");
  // The games get this start's choices; MW3 a named write dir too (Pirate's Moon has its own).
  if (windowed) o.pm_args.push_back("--windowed");
  if (window_given) o.pm_args.push_back("--window"), o.pm_args.push_back(std::to_string(o.win_w) + "x" + std::to_string(o.win_h));
  if (!o.sound) o.pm_args.push_back("--nosound");
  if (o.hidden) o.pm_args.push_back("--hidden");
  if (o.no_import) o.pm_args.push_back("--no-import");
  if (write_dir_given || pointer) o.game_args.push_back("--write-dir"), o.game_args.push_back(o.write_dir);
  o.game_args.insert(o.game_args.end(), o.pm_args.begin(), o.pm_args.end());
  o.quit_after_game = o.play;
  sdlfe::run_launcher(o);
  SDL_Quit();
  return 0;
}
