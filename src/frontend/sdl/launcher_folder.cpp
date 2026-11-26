#include "frontend/sdl/launcher_folder.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>

#include "frontend/sdl/launcher_own.h"

namespace sdlfe::own {

const rekit::data_folder::Game& data_game() {
  static const rekit::data_folder::Game g = [] {
    rekit::data_folder::Game k;
    k.title = "MechWarrior 3", k.configs = {"mw3.cfg"}, k.log_prefix = "mw3: ";
    k.plain_names_only = !narrow_paths_utf8();  // before Windows 10 1903 (docs/program.md, The data folder)
    k.path_from_text = [](const std::string& s) { return path_from_config(s); };
    return k;
  }();
  return g;
}

std::string usual_folder(const char* game) {
  if (const char* env = std::getenv("MW3_USUAL_FOLDER")) return env;
  std::string dir;
  if (char* p = SDL_GetPrefPath("BattleTech", game)) dir = p, SDL_free(p);
  while (dir.size() > 1 && (dir.back() == '/' || dir.back() == '\\')) dir.pop_back();
  if (dir.empty()) return dir;
#if defined(MW3_PACKAGE_BUILD)
  return rekit::data_folder::family_move(dir, data_game());
#else
  return rekit::data_folder::family_move(dir, data_game(), false);
#endif
}

namespace {

// Windows before 10 1903: a folder of the program's or the data with letters outside ASCII fails in odd ways, so it
// is said at the start (docs/program.md, Names on Windows).
bool names_usable(const std::string& data, bool ask) {
  if (narrow_paths_utf8()) return true;
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

}  // namespace

bool data_folder_at_start(std::string* dir, bool ask, bool* pointer) {
  const std::string usual = usual_folder();
  for (;;) {
    const rekit::data_folder::Resolved r = rekit::data_folder::resolve(usual, data_game());
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
    box.flags = SDL_MESSAGEBOX_ERROR, box.title = "MechWarrior 3", box.message = r.error.c_str();
    box.numbuttons = 2, box.buttons = buttons;
    int chose = 0;
    if (!SDL_ShowMessageBox(&box, &chose) || chose != 1) return false;
  }
}

}  // namespace sdlfe::own
