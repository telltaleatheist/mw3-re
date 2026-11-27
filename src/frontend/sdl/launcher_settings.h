#pragma once
// Ours (2026-11-27, docs/launcher-config.md): the launcher's tunable numbers in a settings file. One table (kKeys) names
// each key, its default (today's compiled-in value), its range and unit; the loader, the shipped file and the checks all
// read it. Read once at the start: assets/launcher/launcher-settings.json (shipped, every key at its default), then the
// player's launcher-settings.json in the launcher's data folder on top. Each value clamped to its range; a wrong type,
// an unknown key or a file that does not read falls back to the default, with a log line. Nothing that touches trust
// (keys, https, checksums, signatures, where the launcher asks, the folder marks: launcher-config.md, F) is a key.
#include <string>
#include <vector>

namespace sdlfe::settings {

enum Id {
  // A. Asking our servers and GitHub
  kCatalogEveryHours,     // the catalog fetched again while the launcher stays open; 0 off
  kCatalogAtStart,        // the start's catalog fetch (1 on, 0 off)
  kCatalogSeconds,        // a catalog fetch and its .sig, each
  kArtSeconds,            // a badge or card the catalog names
  kReleasesSeconds,       // a game's releases list at a home
  kReleasesCount,         // releases asked per game list
  kUpdateSeconds,         // the launcher's own releases list
  kUpdateCount,           // releases asked per launcher list
  kSumsSeconds,           // a package's SHA256SUMS
  kSumsSigSeconds,        // its .sig
  kConnectSeconds,        // a download's connect
  kSlowBytesPerSecond,    // a download slower than this...
  kSlowSeconds,           // ...for this long is given up
  // B. Waiting on programs
  kFirstFrameSeconds,     // after PLAY, before the status line says the game is slow to start
  kStatusSecondsStart,    // a kit game's --status at the start
  kStatusSecondsPress,    // the same, asked again on a press
  kDescribeSeconds,       // a kit game's --setup --describe
  kSearchSeconds,         // a kit game's --setup --search
  kDmgMountSeconds,       // Mac: hdiutil attach
  kAppImageSeconds,       // Linux: --appimage-extract
  kUninstallerSeconds,    // Windows: a game's own uninstaller
  // C. The progress bar
  kSetupShare,            // a setup's share of one bar, 0 to 0.5
  kSpeedWindowSeconds,    // the speed averaged over this long
  kSettleSeconds,         // "starting..." until the bytes have come this long
  kStallSeconds,          // "waiting for the server..." after this long without a byte
  kProgressLogSeconds,    // a running job's log line, at most this often
  // D. The window, pacing and sounds
  kWindowWidth,           // the window at the start (--window wins)
  kWindowHeight,
  kVsync,                 // 1 on, 0 off
  kFramesWithoutVsync,    // the timed sleep's frames a second
  kHiddenWakeMs,          // covered, minimized or behind a game: woken this often
  kHoverGain,             // the menu's rollover sound, 0 to 1
  kSelectGain,            // its select sound
  kCount
};

struct Key {
  Id id;
  const char* name;   // the file's key
  double def, min, max;
  bool whole;         // a whole number (a fraction is rounded)
  bool off_zero;      // 0 is allowed below min, meaning off
  const char* unit;   // "seconds", "on/off", ...
  const char* what;   // one line for the shipped file
};
const std::vector<Key>& keys();

double get(Id id);  // the value in use (the default until load)
inline bool on(Id id) { return get(id) != 0; }
inline int whole(Id id) { return static_cast<int>(get(id)); }

constexpr const char* kFileName = "launcher-settings.json";
constexpr int kFormatVersion = 1;

// The shipped file (`shipped_dir`'s, may be "") then the player's (`data_dir`'s, may be ""), each over the defaults
// and clamped. Returns the log lines (each without the "mw3: launcher: " prefix): what was read, what was clamped or
// ignored. A missing file says nothing.
std::vector<std::string> load(const std::string& shipped_dir, const std::string& data_dir);
// One file's text over the values in use (load's step, and the checks'); `from` names it in the lines.
void apply(const std::string& text, const std::string& from, std::vector<std::string>* lines);
// Every value back to its default (the checks).
void reset();
// The shipped file's text: every key at its default, its range and unit beside it.
std::string shipped_text();

}  // namespace sdlfe::settings
