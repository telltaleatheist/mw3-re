#pragma once
// Ours, not the original's: the launcher's CHECK FOR UPDATES (docs/launcher.md, Updates; Owen 2026-11-09, no
// signing). This part is the logic only: GitHub's releases list read, the newest package release picked, versions
// compared, what to say. The fetching (the system's curl) is the front end's (frontend/sdl/sdl_update.h).
#include <cstdint>
#include <string>
#include <vector>

namespace upgrades::update {

struct Asset {
  std::string name, url;  // assets[].name, assets[].browser_download_url
  uint64_t size = 0;      // assets[].size
};
struct Release {
  std::string tag;    // tag_name: "v1.2.0"
  std::string notes;  // body: the release's notes (markdown)
  bool draft = false, prerelease = false;
  std::vector<Asset> assets;
};

// GitHub's JSON for the releases list (an array of releases): false with `error` when it is not one.
bool parse_releases(const std::string& json, std::vector<Release>* out, std::string* error);

// A package release's tag: a bare vX.Y.Z (pm-v*, v1.2.0-rc1 and the like are not).
bool package_tag(const std::string& tag);

// The newest package release: not a draft or a prerelease, the highest package_tag; null when none.
const Release* newest_package(const std::vector<Release>& releases);

// Two versions by their numbers part by part ("v1.2.0" < "v1.10.0"); a build between releases
// ("v1.0.5-264-gHASH-dirty") counts as the release it follows. <0, 0, >0; a version without numbers is lowest.
int compare_versions(const std::string& a, const std::string& b);

// The check's verdict, in words for the launcher's status line.
struct Verdict {
  enum Kind { kNewer, kUpToDate, kFailed } kind = kFailed;
  Release release;
  std::string words;
};
// From what the fetch gave: `json` (empty when it failed, `fetch_error` saying why) against `current` (MW3_VERSION).
Verdict judge(const std::string& current, const std::string& json, const std::string& fetch_error);

// A release's notes (markdown) as plain lines for the launcher's screen: headings without their #, "- " and "* "
// items as "• ", **bold**, *italic* and `code` marks dropped, [text](url) as its text, blank runs as one.
std::vector<std::string> notes_lines(const std::string& markdown);

// The package each platform installs (docs/release.md, the packages' names): Windows' MW3-Rebuild-Setup-<v>.exe,
// the Mac's MW3-Rebuild-<v>-mac.dmg, Linux's MW3-Rebuild-<v>-x86_64.AppImage. The games' own downloads on the same
// release are other names, never taken.
enum class Platform { kWindows, kMac, kLinux };
Platform this_platform();
const Asset* platform_asset(const Release& r, Platform p);
const char* platform_package_words(Platform p);  // "the Windows Setup", for the words when it is missing

// The SHA-256 SHA256SUMS gives `name` ("<64 hex>  <name>" or "<hex> *<name>"), lower case; empty when none.
std::string sums_line(const std::string& sums, const std::string& name);

// The version as a player reads it: "1.2.0" from "v1.2.0".
std::string plain_version(const std::string& tag);

}  // namespace upgrades::update
