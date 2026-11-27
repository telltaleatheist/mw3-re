#pragma once
// Ours: the launcher's games on demand, the doing part (docs/launcher.md, Games on demand; the logic is
// upgrades/update/games.h). Each game lives in a folder of the launcher's own under the games folder, unpacked
// from that game's package for this platform: Windows' zip, the Mac's disk image (its app copied out), Linux's
// AppImage (unpacked with its own --appimage-extract). A folder is replaced or removed only when it holds the
// launcher's record (rebuild-game.txt), and the games folder only holds what the launcher put there.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "rekit/program_record.h"
#include "rekit/status.h"
#include "upgrades/update/games.h"

namespace sdlfe::games {

namespace u = upgrades::update;

// The games folder: MW3_GAMES_DIR (checks), else per user: %LOCALAPPDATA%\BattleTech\games,
// ~/Library/Application Support/BattleTech/games, $XDG_DATA_HOME (~/.local/share)/BattleTech/games (
// 2026-11-22: the family's folder, no longer MicroProse/BattleTech). Empty for a hidden run (checks) without
// MW3_GAMES_DIR, so a check never sees the player's games.
std::string root(bool hidden);

// The games folder of 1.3.x, <base>/MicroProse/BattleTech/games for `root`'s <base>/BattleTech/games: renamed to
// `root` once in a package (rekit::move_once), the saved paths in `cfg` that named it following, and the empty
// MicroProse/BattleTech left behind removed. Returns the folder to use: `root`, or the old one while it cannot be
// moved (a game running from it) or, in a development build, while it is the one with the games.
std::string root_moved(const std::string& root, const std::string& cfg);

// A game's program inside an unpacked package or an installed folder, wherever its package keeps it (Windows'
// <dir>/mw3.exe, the Mac's <dir>/<name>.app/Contents/MacOS/mw3, Linux's <dir>/squashfs-root/usr/bin/mw3); empty
// when it is not there.
std::string program_in(const std::string& dir, u::Game g);

// The game installed under `root`: its record and program; false when there is none (or no program in it).
bool installed(const std::string& root, u::Game g, u::InstalledGame* record, std::string* program);

// A program's version as its file holds it ("MW3-PROGRAM-VERSION:pm-v1.2.3"), read, never run. Empty when none.
std::string program_version(const std::string& bin);

// The game's repo's releases (MW3_GAMES_API: a URL with {repo} in it, file:// allowed, for checks), judged against
// the installed tag.
u::GameCheck check(u::Game g, const std::string& installed_tag);

using Progress = std::function<bool(double, const std::string&)>;  // the fraction, the words; false cancels
// The launcher's one bar (2026-11-27, launcher_job.h): a download's bytes as they come, the end of its bytes (its check
// and the rest follow, said through Progress), and the start of the game's own setup. Any may be empty.
struct Meter {
  std::function<void(uint64_t got)> bytes;
  std::function<void()> ended, setup;
};
constexpr double kOriginalSetupFrom = 0.6;  // get_original's Progress: its setup's steps from here to 1
struct Done {
  bool ok = false, cancelled = false;
  std::string words, program;  // program: the installed game's, when ok
  std::string where;           // get_original: the folder now holding the game's files, when ok
  u::Game game = u::Game::kMw3;
};

// INSTALL and UPDATE: the release's file for this platform and its SHA256SUMS downloaded, the file checked
// against its line (nothing installed without it), unpacked, and put in place of the game's folder.
Done install_release(const std::string& root, u::Game g, const u::Release& r, const Progress& progress, const Meter* meter = nullptr);

// INSTALL FROM FILE: what a SHA256SUMS beside the file says of it.
struct FileCheck {
  enum Kind { kMatches, kNoSums, kMismatch } kind = kNoSums;
  std::string sha256, words;
};
FileCheck check_file(const std::string& file);
// The file unpacked; which game it is found from the program inside (one game's package, not the full one), then
// put in place of that game's folder. `sha256` is the file's (check_file's).
Done install_file(const std::string& root, const std::string& file, const std::string& sha256, const Progress& progress);

// The player's own folder for a game (2026-11-22: "everything can be installed in the same location the user
// picks"): its program (installed under it as under the games folder) and its game files (install/); pilots, saves,
// settings and mods stay in the user folder. A picked folder is the game's when it is empty or already carries this
// game's mark (rebuild-game-folder.txt), else a folder named for the game inside it ("MechWarrior 3", "Pirate's Moon",
// "MechWarrior 1 Remake"), so a drive's top or someone's folder is never filled loose.
std::string game_folder_in(const std::string& picked, u::Game g);
// The folder made the game's before anything goes in: created when missing, marked as the launcher's games folder
// (its program can go in) and as this game's. One holding anything else without the marks is never written to.
bool claim_game_folder(const std::string& dir, u::Game g, std::string* error);

// A kit game's answer to `bin --status` (re-setup-kit's launcher contract: one JSON object, exit 0, nothing on
// screen; MechWarrior 1 Rebuild first): false when it did not answer in time or its answer is not one.
// more: after --status. False with `why` (for the log: "no answer in 10 s", "exited 1: ...") when it gave none.
bool kit_status(const std::string& bin, rekit::Status* out, const std::vector<std::string>& more = {}, int seconds = 10,
                std::string* why = nullptr);
// A game's id in its --status ("mech3", "mech3-pm", ...), "" when the launcher knows none (b5: --with names them).
const char* kit_id(upgrades::update::Game g);

// A kit game's setup on the launcher's screens (its status says `setup`; re-setup-kit's rekit/sdl/setup_serve.h): runs
// `bin --setup <args>` and follows its lines. `seconds` 0: as long as it takes. False: no answer (why in `out`'s
// summary). A stopped one is told to stop and, after a wait, ended. `each` (optional) gets every line before the
// answer, in order: --copies' rows (progress sees only the latest line of a read).
bool kit_setup(const std::string& bin, const std::vector<std::string>& args, int seconds,
               const std::function<bool(const rekit::SetupMessage&)>& progress, rekit::SetupMessage* out,
               const std::function<void(const rekit::SetupMessage&)>& each = nullptr);

// Ours (2026-11-26, docs/launcher.md, Games on demand): a game's original files from its catalog entry's "original",
// then its "compact" (u::original_sources, 2026-11-27), after its DOWNLOAD, when its status says they are missing. Each source in turn is downloaded into the launcher's own
// marked folder (temp/MW3-Rebuild-originals, kOriginalMark), its size and SHA-256 checked, then handed to the game's
// served setup as a pick (`--setup --look`, then `--install`, `more` after each, `--folder` when given), as PICK A
// FILE does: the game's own import puts its files where it expects them. The file is removed afterwards, whatever
// happened. Not ok: `words` says why (the launcher's fallback is the usual GAME FILES).
constexpr const char* kOriginalMark = "rebuild-original-download.txt";
std::string originals_dir();
Done get_original(const std::string& bin, const std::vector<u::CatalogGame::Original>& sources, const std::vector<std::string>& more,
                  const std::string& folder, const Progress& progress, const Meter* meter = nullptr);

// Whether `bin` is inside the AppImage this program runs from ($APPDIR), so goes away with it.
bool in_own_mount(const std::string& bin);

// Ours (2026-11-24, docs/launcher.md, Copies installed without the launcher): a game of ours installed by its own
// Setup, disk image or AppImage is found and linked, and a player can link one by hand (a panel's gear).
// Each of our programs keeps this file in its user folder: where it is and its version (rebuild-program.txt).
using rekit::kProgramRecord;  // "rebuild-program.txt", the kit's (rekit/program_record.h)
// This program's file as a player starts it ($APPIMAGE when it runs from that AppImage's mount, not one it inherited
// from the launcher's); empty when it runs from somewhere that goes
// away (the Mac's App Translocation, a disk image).
std::string self_path();
// Whether `prog` is in a game's folder the launcher made (its rebuild-game.txt in a folder above): never a copy
// installed without it.
bool launchers_own(const std::string& prog);
// The record written (only when it changed) for a released build not the launcher's own copy: a development one never
// links itself, and the launcher's copy never takes the place of one installed without it.
void record_self(const std::string& user_folder, u::Game g, const std::string& version);
// What `prog` is as game g's program: its version from its stamp, else (when `may_run`) from its --status (an
// AppImage's stamp is compressed); empty when it is not g's.
std::string identify(const std::string& prog, u::Game g, bool may_run);
// The program a player picked for g (the program, its folder, a .app, an AppImage); empty when none is there.
std::string program_of_pick(const std::string& pick, u::Game g);
struct Found {
  std::string program, version, how;  // how: "its record", "its Setup", "Applications"
};
// g's released copies installed without the launcher, newest first: the record in `user_folder`, Windows' uninstall
// entry for its Setup, the Mac's Applications folders (those two only when `system`: a check run reads its own folder
// alone). Each checked (identify): present and g's.
std::vector<Found> found_elsewhere(u::Game g, const std::string& user_folder, bool system);
// The launcher's own copy of g under `root` removed: only a folder carrying the launcher's record. False with why.
bool uninstall(const std::string& root, u::Game g, std::string* error);
// How a linked copy (not the launcher's) can be uninstalled from the launcher (2026-11-24): its own Setup's uninstaller
// (Windows: unins*.exe beside it), or its app or AppImage moved to the trash, so it can be taken back. kNone, with why:
// a program loose in a folder (what else is in there is not ours to say), one whose package holds another game of ours,
// or this launcher.
struct Removal {
  enum How { kNone, kSetup, kTrash } how = kNone;
  std::string path, why;  // the uninstaller; the .app or AppImage
};
Removal removal_of(const std::string& prog, u::Game g);
// It, done (blocks: on a worker). `prog` gone is the answer, whatever the uninstaller said. A check run moves to
// MW3_LAUNCHER_TRASH instead of the trash, and runs nothing.
bool remove_linked(const std::string& prog, u::Game g, const Removal& r, std::string* error);

}  // namespace sdlfe::games
