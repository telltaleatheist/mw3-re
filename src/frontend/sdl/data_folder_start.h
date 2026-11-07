#pragma once
// The usual folder and, through the pointer (mech3/data_folder.h), the player's
// data folder, as every program starts: mw3pm, mw3import.
#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "gamez/zutil/zutl_path.h"
#include "mech3/data_folder.h"

namespace sdlfe {

// The usual place; MW3_USUAL_FOLDER names another (checks).
inline std::string usual_folder(const char* game = "Pirate's Moon") {
  if (const char* env = std::getenv("MW3_USUAL_FOLDER")) return env;
  std::string dir;
  if (char* p = SDL_GetPrefPath("MicroProse", game)) dir = p, SDL_free(p);
  while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
  return dir;
}

// Windows before 10 1903: a folder of the program's or the data with letters
// outside ASCII fails in odd ways, so it is said at the start (docs/program.md,
// Names on Windows).
inline bool names_usable(const std::string& data, bool ask) {
  if (zutl::narrow_paths_utf8()) return true;
  const char* base = SDL_GetBasePath();
  const std::string program = base ? base : "";
  auto plain = [](const std::string& s) {
    for (const char c : s)
      if (static_cast<unsigned char>(c) >= 0x80) return false;
    return true;
  };
  if (plain(program) && plain(data)) return true;
  const std::string text = "This version of Windows cannot use a folder whose name has letters outside plain English "
                           "(it needs Windows 10 version 1903 or later):\n\n" + (plain(program) ? data : program);
  std::fprintf(stderr, "mw3: %s\n", text.c_str());
  if (ask) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "MechWarrior 3", text.c_str(), nullptr);
  return false;
}

// `dir` set to the folder to use. A pointer's folder that cannot be used is said
// and asked about again (a drive to connect), and never replaced by a fresh
// folder: false when the player quits, or at once when `ask` is false.
inline bool data_folder_at_start(std::string* dir, bool ask, bool* pointer = nullptr, const char* game = "Pirate's Moon") {
  const std::string usual = usual_folder(game);
  for (;;) {
    const mech3::data_folder::Resolved r = mech3::data_folder::resolve(usual);
    if (pointer) *pointer = r.pointer;
    if (r.error.empty()) {
      *dir = r.dir;
      return names_usable(r.dir, ask);
    }
    std::fprintf(stderr, "mw3: %s\n", r.error.c_str());
    if (!ask) return false;
    const SDL_MessageBoxButtonData buttons[] = {{SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Try again"},
                                                {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit"}};
    SDL_MessageBoxData box{};
    box.flags = SDL_MESSAGEBOX_ERROR, box.title = "Pirate's Moon", box.message = r.error.c_str();
    box.numbuttons = 2, box.buttons = buttons;
    int chose = 0;
    if (!SDL_ShowMessageBox(&box, &chose) || chose != 1) return false;
  }
}

}  // namespace sdlfe
