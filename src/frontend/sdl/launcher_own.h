#pragma once
// The launcher's own small copies of helpers it took from the game's code (launcher split b1, 2026-11-26;
// docs/launcher-split.md): the Work types, mw3.cfg's line editor, crc32, any_case and path_from_config. Each does
// what the game's does, so nothing changes; the launcher needs less of the game to build.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace sdlfe::own {

// A long operation's progress and its outcome (were mech3::setup's Progress, ProgressFn and Result).
struct Progress {
  std::string stage;    // "Reading the disc", "Copying the game files", ...
  double fraction = 0;  // of the whole operation, 0..1
  std::string message;  // the current file or step, for a second line
};
// False cancels: the operation stops as soon as it can and changes nothing.
using ProgressFn = std::function<bool(const Progress&)>;
struct Result {
  bool ok = false;
  bool cancelled = false;
  std::string summary;                // one or two sentences for the player
  std::string detail;                 // the technical reason (the log)
  std::vector<std::string> warnings;  // plain words, each a sentence
  std::string install_dir;            // the game's folder, where an operation names one
  std::string version;                // its data's version ("1.0", "1.2", ...)
  bool pirates_moon = false;          // the game is Pirate's Moon
  int files = 0, movies = 0, music_tracks = 0;
  uint64_t bytes = 0;
};

// mw3.cfg in `write_dir`: `key`=`value` as its first line, any older line of that key gone, every other line and
// its ending kept byte for byte (as sdl_install's). save_install: "install", made absolute.
bool save_config_value(const std::string& write_dir, const std::string& key, const std::string& value);
bool save_install(const std::string& write_dir, const std::string& install);

// CRC-32, MSB first (polynomial 0x04c11db7, no reflection, no final xor), as zutl_zar's: the notes' seen-once check.
uint32_t crc32(uint32_t crc, const void* data, size_t n);

// `rel` ('/' between its parts) under `root`, each part by its own name or one differing only by case; "" if none
// (as zutl::any_case).
std::string any_case(const std::string& root, const std::string& rel);

// A path from mw3.cfg, which an earlier build may have written in Windows' code page, as UTF-8; elsewhere as it is.
// narrow_paths_utf8: whether the narrow calls take UTF-8 (Windows 10 1903 and on; MW3_OLD_WINDOWS says not).
// As zutl's.
bool narrow_paths_utf8();
std::string path_from_config(const std::string& s);
// Files of `write_dir` an earlier build named wrongly on Windows (UTF-8 through the code page's calls), put right once
// per settings folder; nothing elsewhere. As zutl's.
void repair_old_names(const std::string& write_dir);

// Where a game's music and converted movies are: the folder above `install` when it is a game's own folder (the
// kit's rebuild-game-folder.txt), else `write_dir`. As sdl_install's media_dir.
std::string media_dir(const std::string& install, const std::string& write_dir);

// A game's data in words, as the game's own setup says it (mech3::setup's version_words; b6): "MechWarrior 3 1.2",
// "MechWarrior 3 (partly patched to 1.2)", "MechWarrior 3: Pirate's Moon 1.0". found_words: "Found <that> at <dir>."
// for a copy a game's --status reported; start_found_words: PLAY's line (MechWarrior 3 without its version).
std::string game_words(const std::string& version, bool pirates_moon);
std::string found_words(const std::string& version, const std::string& dir, bool pirates_moon);
std::string start_found_words(const std::string& dir, bool pirates_moon);

}  // namespace sdlfe::own
