#pragma once
// Ours, not original code (docs/mods-campaigns-plan.md, Entering and leaving
// restart the program): mods are read when the game starts, so MODS plays a
// campaign entry by starting the program again into it (--entry ID and the
// entry's screen), and the entry's BACK starts it again plain, on MODS. The
// run asks (restart_ask) and closes as on QUIT; main, once everything of the
// run is gone, starts the program again with the start's own arguments.
#include <string>
#include <vector>

namespace sdlfe {

struct RestartTo {
  enum Screen { kNone, kPlay, kInstantAction, kTest };
  std::string entry;       // the MODS campaign entry's id; "" a plain start
  Screen screen = kNone;   // the entry's screen: its pilots (--entry-play) or Instant Action (--entry-ia)
  bool open_mods = false;  // a plain start with MODS open (--open-mods)
  bool open_ia = false;    // a plain start on Instant Action (--open-ia)
  std::string test_world;  // kTest: the mission played at once (--entry-test WORLD N), then MODS's test mode in the entry
  int test_number = 0;
  std::string mods_entry;  // with open_mods: the campaign chosen on MODS (--mods-entry ID)
  std::string pilot;       // Instant Action's pilot picked at once (--entry-pilot NAME), so its screen opens, not the pilot screen
};

// The next start's arguments from this start's (without the program's name):
// what a start into an entry adds taken out (--entry, --entry-play,
// --entry-ia, --entry-test, --entry-pilot, --open-mods, --mods-entry, --open-ia, --restarted) with --mod and --no-mods and a
// check's scripted input (--click-at, --tap), the rest kept in order, then `to`'s and --restarted (no intro movies).
std::vector<std::string> restart_args(const std::vector<std::string>& args, const RestartTo& to);

// The run's request, and main's taking of it (false: none asked).
void restart_ask(const RestartTo& to);
bool restart_asked(RestartTo* to);

// This program's own file ("" when it cannot be told): the Mac's executable
// path, Linux's /proc/self/exe (inside a mounted AppImage, not $APPIMAGE:
// restart_into keeps the process, so the mount stays).
std::string restart_own_path();

// The program started again with `args`, after the run has closed (its log,
// SDL, its helpers). POSIX: this process becomes it (execv: the same PID, so
// a launcher waiting on the game and an AppImage's mount see one run); every
// handle above stderr is marked close-on-exec first. Returns only when it
// could not, with `why`.
bool restart_into(const std::string& argv0, const std::vector<std::string>& args, std::string* why);

// The handles above stderr still open (POSIX; for the checks and the log).
std::vector<int> restart_open_handles();
std::string restart_handle_name(int fd);  // its file's path, or what else it is

}  // namespace sdlfe
