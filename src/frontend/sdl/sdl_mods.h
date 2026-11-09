#pragma once
// ADD MOD's platform pieces (ours; docs/modding.md, ADD MOD), shared by the
// game's MODS screen and the launcher: a folder opened in the system's file
// browser, and the question of which game a mod is for.
#include <string>

struct SDL_Window;

namespace sdlfe {

// `dir` in Finder, Explorer or the desktop's file browser (SDL_OpenURL on its
// percent-encoded file:// URL); false when the system would not.
bool open_folder(const std::string& dir);
// "Which game is <name> for?": 1 MechWarrior 3, 2 Pirate's Moon, 0 cancelled.
int ask_mod_game(SDL_Window* parent, const std::string& name);

}  // namespace sdlfe
