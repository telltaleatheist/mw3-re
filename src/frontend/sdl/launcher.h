#pragma once
// The launcher and the first-start setup (ours, not original code;
// docs/launcher.md): the first screen of a player's start, before any
// movie. It finds the MechWarrior 3 install quietly and offers the games; a
// game without its files opens the find screen (find it, install from the
// disc, an image or a download), and the notes offer what an install lacks
// (the movies, the music) from the disc, through the game's own --setup
// (the games' find screen until d1: docs/launcher-split.md, b6). Its art and
// sounds are ours (assets/launcher).
#include <string>
#include <vector>

struct SDL_Window;

namespace sdlfe {

struct LauncherOptions {
  std::string write_dir;
  std::string cfg_install;                // mw3.cfg's install=
  std::string cfg_pm_install;             // mw3.cfg's pm_install= (Pirate's Moon's folder)
  std::string cfg_pm_notes;               // mw3.cfg's pm_notes=: the CRC of the PM notes last shown
  std::string cfg_mw3_notes;              // mw3.cfg's mw3_notes=: the same for MechWarrior 3's start check
  std::string pm_write_dir;               // Pirate's Moon's settings folder (MW3_PM_WRITE_DIR; else found at its start)
  std::string cfg_mw1_game;               // mw3.cfg's mw1_game= (MechWarrior 1989's folder)
  std::vector<std::string> more_installs;  // a development build's copy
  bool fullscreen = false;  // windowed (launcher_main.cpp: macOS Game Mode)
  bool sound = true;  // the main menu's rollover and select sounds from the install (off with --nosound)
  int win_w = 1280, win_h = 720;  // 16:9, as the canvas
  // The window's size from the program's --window, when one was given: else
  // the 16:9 above (2026-11-10: 16:9 everywhere by default), not the
  // game's own 4:3 default, which letterboxed the canvas.
  void window_from(bool given, int w, int h) {
    if (given && w > 0 && h > 0) win_w = w, win_h = h;
  }
  // For checking (--launcher-shot FILE [--launcher-screen main|setup|pick|work|notes|disc]):
  // the screen drawn once into FILE (a BMP of the 640x480 canvas), no window shown.
  std::string shot, screen = "main";
  // A plain mw3 start that found no install (docs/games-layout.md): the find
  // screen alone, straight on into the game once there is one.
  bool setup_only = false;
  // With setup_only: an install the plain start's search found elsewhere, looked at first and offered to be
  // copied in (INSTALL), so the original may be deleted afterwards (2026-11-21).
  std::string look_first;
  bool choose_again = false;
  // mw3pm's own start (setup_only): the find screen is Pirate's Moon's, its folder chosen into write_dir/install;
  // cfg_install is its own mw3.cfg's install= (2026-11-21: the player chooses in PM too).
  bool pm = false;  // Shift held at a plain start: the files in use offered again, PLAY goes on with them
  bool write_dir_fixed = false;  // --write-dir given: no moving the data folder
  // mw3launcher (docs/launcher.md, The two programs): MECHWARRIOR 3 starts
  // this program as a child, `game_bin <install> game_args... --ready-file F`,
  // the launcher hidden from the game's first frame until it exits.
  std::string program_dir;  // this program's own folder (a bundle's Contents/MacOS, not its Resources)
  std::string game_bin;
  // mw3pm beside this program, empty when the package has none (PIRATE'S MOON
  // then stays "Coming later"). Its arguments have nothing of MW3's: it keeps
  // its own install, settings and saves (docs/launcher.md).
  std::string pm_bin;
  std::string mw1_bin;  // MechWarrior 1 Remake's program beside ours (MW3_MW1_BIN for checks)
  std::string mw1r_bin;  // MechWarrior 1 Rebuild's (mech1-re, MW1's ORIGINAL tab), beside ours or installed (MW3_MW1R_BIN)
  std::string chi_bin, chirm_bin;  // Inception Rebuild's and Inception Remake's (MW3_CHI_BIN, MW3_CHIRM_BIN)
  std::vector<std::string> pm_args;
  std::vector<std::string> game_args;
  bool hidden = false;  // --hidden: the window never shown (checks), passed on to the game
  bool no_import = false;  // --no-import: no movie conversion here (passed on to the game)
  bool play = false;  // --launcher-play: MECHWARRIOR 3 pressed once the window is up (checks)
  bool quit_after_game = false;  // with play: quit when the game exits (checks)
  bool play_pm = false;  // --launcher-pm: PIRATE'S MOON pressed once, if it can start, then quit (checks)
  bool play_mw1 = false;  // --launcher-mw1: MECHWARRIOR (1989) pressed once, then quit when it returns (checks)
};

struct LauncherChoice {
  enum Kind { kQuit, kPlay } kind = kQuit;
  std::string install;  // kPlay: MechWarrior 3's folder (saved to mw3.cfg)
  // kPlay: the launcher's window, its renderer gone, for the game to take
  // (SdlWindow::init): one window from the launcher into the game.
  SDL_Window* window = nullptr;
};

LauncherChoice run_launcher(const LauncherOptions& o);

}  // namespace sdlfe
