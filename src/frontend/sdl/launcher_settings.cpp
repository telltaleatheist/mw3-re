#include "frontend/sdl/launcher_settings.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "upgrades/update/json_reader.h"

namespace sdlfe::settings {
namespace fs = std::filesystem;
using upgrades::update::detail::Json;

namespace {
// Today's values as defaults, the ranges from docs/launcher-config.md. Servers' floors: the catalog's 4 h, the counts.
const std::vector<Key> kKeys = {
    {kCatalogEveryHours, "catalog_every_hours", 4, 4, 168, false, true, "hours", "the games catalog fetched again while the launcher stays open"},
    {kCatalogAtStart, "catalog_at_start", 1, 0, 1, true, false, "1 on, 0 off", "the games catalog fetched at the start (off: new games only on CHECK FOR UPDATES)"},
    {kCatalogSeconds, "catalog_seconds", 15, 5, 120, true, false, "seconds", "time allowed for the catalog, and again for its signature"},
    {kArtSeconds, "art_seconds", 15, 5, 120, true, false, "seconds", "time allowed for a badge or card the catalog names"},
    {kReleasesSeconds, "releases_seconds", 15, 5, 120, true, false, "seconds", "time allowed for a game's releases list"},
    {kReleasesCount, "releases_count", 20, 10, 40, true, false, "releases", "releases asked for in each game's list"},
    {kUpdateSeconds, "update_check_seconds", 20, 5, 120, true, false, "seconds", "time allowed for the launcher's own releases list"},
    {kUpdateCount, "update_check_count", 40, 20, 100, true, false, "releases", "releases asked for in the launcher's list"},
    {kSumsSeconds, "sums_seconds", 60, 15, 300, true, false, "seconds", "time allowed for a package's SHA256SUMS"},
    {kSumsSigSeconds, "sums_signature_seconds", 30, 10, 300, true, false, "seconds", "time allowed for its signature"},
    {kConnectSeconds, "download_connect_seconds", 20, 5, 120, true, false, "seconds", "a download gives up when it cannot connect in this long"},
    {kSlowBytesPerSecond, "download_slow_bytes_per_second", 1024, 1, 65536, true, false, "bytes a second", "a download slower than this..."},
    {kSlowSeconds, "download_slow_seconds", 60, 15, 600, true, false, "seconds", "...for this long is given up"},
    {kFirstFrameSeconds, "game_first_frame_seconds", 20, 5, 300, false, false, "seconds", "after PLAY, how long before the status line says the game is slow to start"},
    {kStatusSecondsStart, "game_status_seconds", 10, 3, 120, true, false, "seconds", "time allowed for a game's status at the start"},
    {kStatusSecondsPress, "game_status_press_seconds", 30, 5, 300, true, false, "seconds", "the same, asked again when PLAY finds it unknown"},
    {kDescribeSeconds, "game_setup_describe_seconds", 10, 3, 60, true, false, "seconds", "time allowed for a game to say what its setup offers"},
    {kSearchSeconds, "game_setup_search_seconds", 120, 30, 600, true, false, "seconds", "time allowed for a game's search for its files"},
    {kDmgMountSeconds, "mac_disk_image_seconds", 120, 30, 600, true, false, "seconds", "Mac: time allowed to open an update's or a game's disk image"},
    {kAppImageSeconds, "linux_appimage_seconds", 600, 60, 3600, true, false, "seconds", "Linux: time allowed to unpack a game's AppImage"},
    {kUninstallerSeconds, "windows_uninstaller_seconds", 300, 60, 1800, true, false, "seconds", "Windows: time allowed for a game's own uninstaller"},
    {kSetupShare, "bar_setup_share", 0.15, 0, 0.5, false, false, "fraction", "the setting-up step's share of one progress bar"},
    {kSpeedWindowSeconds, "bar_speed_seconds", 5, 1, 30, false, false, "seconds", "the speed shown is the average over this long"},
    {kSettleSeconds, "bar_starting_seconds", 2, 0, 10, false, false, "seconds", "\"starting...\" until the bytes have come this long"},
    {kStallSeconds, "bar_waiting_seconds", 10, 3, 120, false, false, "seconds", "\"waiting for the server...\" after this long without a byte"},
    {kProgressLogSeconds, "log_progress_seconds", 1, 1, 60, false, false, "seconds", "a running download's log line at most this often"},
    {kWindowWidth, "window_width", 1280, 640, 7680, true, false, "pixels", "the window's width at the start (--window WxH wins)"},
    {kWindowHeight, "window_height", 720, 360, 4320, true, false, "pixels", "its height"},
    {kVsync, "vsync", 1, 0, 1, true, false, "1 on, 0 off", "frames paced to the display"},
    {kFramesWithoutVsync, "frames_without_vsync", 60, 30, 240, true, false, "a second", "frames a second when vsync is off"},
    {kHiddenWakeMs, "hidden_wake_ms", 100, 50, 500, true, false, "milliseconds", "while covered, minimized or behind a game, the launcher wakes this often"},
    {kHoverGain, "sound_hover_volume", 0.7, 0, 1, false, false, "0 to 1", "the menu's rollover sound"},
    {kSelectGain, "sound_select_volume", 1.0, 0, 1, false, false, "0 to 1", "the menu's select sound"},
    {kSendUpdateCounts, "send_update_counts", 1, 0, 1, true, false, "1 on, 0 off (or true, false)",
     "after an update, tell reverse-engineered.com which game and versions (counts only, nothing about you)"},
};

double g_values[kCount];
bool g_set = false;

void defaults() {
  for (const Key& k : kKeys) g_values[k.id] = k.def;
  g_set = true;
}

std::string num(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%g", v);
  return b;
}
}  // namespace

const std::vector<Key>& keys() { return kKeys; }

double get(Id id) {
  if (!g_set) defaults();
  return id >= 0 && id < kCount ? g_values[id] : 0;
}

void reset() { defaults(); }

void apply(const std::string& text, const std::string& from, std::vector<std::string>* lines) {
  if (!g_set) defaults();
  Json root;
  upgrades::update::detail::Reader r(text);
  if (!r.value(&root) || !r.end() || root.type != Json::kObject) {
    lines->push_back("settings: " + from + " does not read (" + (r.error.empty() ? "not an object" : r.error) + "): nothing in it used");
    return;
  }
  if (const Json* v = root.at("version"); v && (v->type != Json::kNumber || std::atof(v->s.c_str()) > kFormatVersion))
    lines->push_back("settings: " + from + ": version " + v->s + " is newer than this launcher's " + std::to_string(kFormatVersion) +
                     ": the keys it knows are read");
  int used = 0;
  for (const auto& [name, v] : root.fields) {
    if (name == "version" || (!name.empty() && name[0] == '_')) continue;
    const Key* k = nullptr;
    for (const Key& x : kKeys)
      if (name == x.name) k = &x;
    if (!k) {
      lines->push_back("settings: " + from + ": " + name + " is not a setting: ignored");
      continue;
    }
    double x;
    if (v.type == Json::kNumber) x = std::atof(v.s.c_str());
    else if (v.type == Json::kBool && k->max == 1 && k->min == 0 && k->whole) x = v.b ? 1 : 0;
    else {
      lines->push_back("settings: " + from + ": " + name + " is not a number: " + num(k->def) + " used");
      g_values[k->id] = k->def;
      continue;
    }
    double use = std::isfinite(x) ? x : k->def;
    if (k->whole) use = std::round(use);
    const double rounded = use;
    if (!(k->off_zero && use == 0)) use = use < k->min ? k->min : use > k->max ? k->max : use;
    if (use == rounded && rounded != x) lines->push_back("settings: " + from + ": " + name + " " + v.s + " is a whole number: " + num(use) + " used");
    if (use != rounded) lines->push_back("settings: " + from + ": " + name + " " + v.s + " is outside " + num(k->min) + " to " + num(k->max) +
                                   (k->off_zero ? " (or 0)" : "") + ": " + num(use) + " used");
    g_values[k->id] = use;
    ++used;
  }
  lines->push_back("settings: " + from + ": " + std::to_string(used) + " read");
}

std::vector<std::string> load(const std::string& shipped_dir, const std::string& data_dir) {
  defaults();
  std::vector<std::string> lines;
  for (const std::string& dir : {shipped_dir, data_dir}) {
    if (dir.empty()) continue;
    const fs::path p = fs::path(dir) / kFileName;
    std::ifstream f(p, std::ios::binary);
    if (!f) continue;
    const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (text.size() > (1 << 20)) {
      lines.push_back("settings: " + p.string() + " is over 1 MB: not read");
      continue;
    }
    apply(text, p.string(), &lines);
  }
  for (const Key& k : kKeys)
    if (g_values[k.id] != k.def) lines.push_back("settings: " + std::string(k.name) + " " + num(g_values[k.id]) + " (default " + num(k.def) + ")");
  return lines;
}

std::string shipped_text() {
  std::string s = "{\n  \"version\": " + std::to_string(kFormatVersion) + ",\n";
  s += "  \"_read_me\": \"The launcher's settings, each at its default. To change one, copy this file into the launcher's "
       "data folder and edit that copy (this one is replaced by every update). Out of range: the nearest end is used; "
       "anything else wrong: the default. The log says which.\",\n";
  for (size_t i = 0; i < kKeys.size(); ++i) {
    const Key& k = kKeys[i];
    std::string what = k.what;
    for (size_t at = 0; (at = what.find('"', at)) != std::string::npos; at += 2) what.insert(at, "\\");
    s += "  \"" + std::string(k.name) + "\": " + num(k.def) + ",\n";
    s += "  \"_" + std::string(k.name) + "\": \"" + num(k.min) + " to " + num(k.max) + (k.off_zero ? " (or 0, off)" : "") + ", " +
         k.unit + ": " + what + "\"" + (i + 1 < kKeys.size() ? ",\n" : "\n");
  }
  return s + "}\n";
}

}  // namespace sdlfe::settings
