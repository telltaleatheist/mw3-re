// The updater's logic (src/upgrades/update; docs/launcher.md, Updates, sitting 1): GitHub's releases list read, the
// newest package release picked, versions compared, the check's words.
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "check.h"
#include "upgrades/update/games.h"
#include "upgrades/update/update.h"
#include "upgrades/update/sources.h"

namespace u = upgrades::update;

namespace {
// A releases list as GitHub gives it (the fields the updater reads, among others it skips).
std::string release(const std::string& tag, bool draft = false, bool pre = false, const std::string& body = "Fixes.") {
  return "{\"url\":\"https://api.github.com/x\",\"id\":1,\"tag_name\":\"" + tag + "\",\"draft\":" + (draft ? "true" : "false") +
         ",\"prerelease\":" + (pre ? "true" : "false") + ",\"body\":\"" + body + "\",\"author\":{\"login\":\"a\",\"id\":2},\"assets\":[" +
         "{\"name\":\"MW3-Rebuild-" + tag + "-mac.dmg\",\"size\":10,\"browser_download_url\":\"https://github.com/r/" + tag + "/mac.dmg\"}," +
         "{\"name\":\"MW3-Rebuild-Setup-" + tag + ".exe\",\"size\":10,\"browser_download_url\":\"https://github.com/r/" + tag + "/setup.exe\"}," +
         "{\"name\":\"MW3-Rebuild-" + tag + "-x86_64.AppImage\",\"size\":10,\"browser_download_url\":\"https://github.com/r/" + tag + "/a.AppImage\"}," +
         "{\"name\":\"SHA256SUMS\",\"browser_download_url\":\"https://github.com/r/" + tag + "/SHA256SUMS\"}]}";
}
std::string list(const std::vector<std::string>& items) {
  std::string s = "[";
  for (size_t i = 0; i < items.size(); ++i) s += (i ? ",\n  " : "") + items[i];
  return s + "]";
}
}  // namespace

TEST(update_reads_the_releases_list) {
  std::vector<u::Release> all;
  std::string error;
  EXPECT(u::parse_releases(list({release("v1.1.0", false, false, "## Fixes\\n- \\\"quoted\\\" \\u00e9 \\ud83d\\ude00 \\\\ done")}), &all, &error));
  EXPECT(all.size() == 1 && all[0].tag == "v1.1.0" && !all[0].draft && !all[0].prerelease);
  EXPECT(all[0].notes == "## Fixes\n- \"quoted\" \xc3\xa9 \xf0\x9f\x98\x80 \\ done");  // escapes, \u, a surrogate pair
  EXPECT(all[0].assets.size() == 4 && all[0].assets[0].name == "MW3-Rebuild-v1.1.0-mac.dmg" &&
         all[0].assets[3].url == "https://github.com/r/v1.1.0/SHA256SUMS");
  // Not a list, cut short, not JSON, text after it: each refused with where.
  for (const char* bad : {"{\"tag_name\":\"v1\"}", "[{\"tag_name\":\"v1.1.0\"", "<html>rate limited</html>", "[] x"}) {
    EXPECT(!u::parse_releases(bad, &all, &error) && !error.empty());
    std::printf("  %s -> %s\n", bad, error.c_str());
    error.clear();
  }
}

TEST(update_picks_the_newest_package_release) {
  EXPECT(u::package_tag("v1.1.0") && u::package_tag("v10.20.30"));
  for (const char* t : {"pm-v1.0.1", "v1.1", "v1.1.0-rc1", "1.1.0", "v1.1.0.1", "v1..0", "latest"}) EXPECT(!u::package_tag(t));
  std::vector<u::Release> all;
  std::string error;
  EXPECT(u::parse_releases(list({release("pm-v9.0.0"), release("v1.9.0"), release("v1.10.0"), release("v2.0.0", true),
                                 release("v1.11.0", false, true), release("v1.10.1-rc1"), release("v1.2.0")}),
                           &all, &error));
  const u::Release* newest = u::newest_package(all, u::Platform::kMac);
  EXPECT(newest && newest->tag == "v1.10.0");  // not the draft, the prerelease, the rc or PM's; 1.10 above 1.9
  EXPECT(!u::newest_package({}, u::Platform::kMac));
}

TEST(update_compares_versions) {
  EXPECT(u::compare_versions("v1.2.0", "v1.10.0") < 0 && u::compare_versions("v1.10.0", "v1.9.9") > 0);
  EXPECT(u::compare_versions("v1.1.0", "v1.1.0") == 0 && u::compare_versions("v1.1", "v1.1.0") == 0);
  EXPECT(u::compare_versions("v1.0.5-264-g1bb39620-dirty", "v1.0.5") == 0);  // a build between releases: its last one
  EXPECT(u::compare_versions("v1.0.5-264-g1bb39620-dirty", "v1.1.0") < 0);
  EXPECT(u::compare_versions("unknown", "v0.0.1") < 0);
  EXPECT(u::plain_version("v1.2.0") == "1.2.0" && u::plain_version("pm-v1.0.1") == "pm-v1.0.1");
}

TEST(update_says_what_it_found) {
  const std::string json = list({release("v1.1.0"), release("v1.2.0"), release("pm-v1.0.1")});
  const u::Verdict newer = u::judge("v1.1.0", json, "");
  std::printf("  %s\n", newer.words.c_str());
  EXPECT(newer.kind == u::Verdict::kNewer && newer.release.tag == "v1.2.0" && newer.words == "MechWarrior 3 Rebuild 1.2.0 is out (this is 1.1.0).");
  const u::Verdict same = u::judge("v1.2.0", json, "");
  EXPECT(same.kind == u::Verdict::kUpToDate && same.words == "This is the newest MechWarrior 3 Rebuild (1.2.0).");
  EXPECT(u::judge("v1.3.0", json, "").kind == u::Verdict::kUpToDate);  // a newer build than any release
  EXPECT(u::judge("v1.2.0-5-gabc", json, "").kind == u::Verdict::kUpToDate);
  EXPECT(u::judge("unknown", json, "").kind == u::Verdict::kNewer);
  const u::Verdict offline = u::judge("v1.1.0", "", "could not resolve host");
  EXPECT(offline.kind == u::Verdict::kFailed && offline.words == "Could not check for updates: could not resolve host. Is this computer online?");
  const u::Verdict broken = u::judge("v1.1.0", "<html>", "");
  EXPECT(broken.kind == u::Verdict::kFailed && broken.words.find("could not be read") != std::string::npos);
  const u::Verdict none = u::judge("v1.1.0", list({release("pm-v1.0.1"), release("v1.3.0", true)}), "");
  EXPECT(none.kind == u::Verdict::kFailed && none.words.find("no release") != std::string::npos);
}

// Two homes (2026-11-21): the hub mw3-re and mech3-re; the newest package on either wins, one failing is fine.
TEST(update_takes_the_newest_from_either_repo) {
  const std::string hub = list({release("v1.2.2"), release("pm-v1.1.1")}), game = list({release("v1.3.0"), release("v1.4.0", true)});
  const u::Verdict both = u::judge("v1.2.2", {{hub, ""}, {game, ""}});
  std::printf("  %s\n", both.words.c_str());
  EXPECT(both.kind == u::Verdict::kNewer && both.release.tag == "v1.3.0");
  EXPECT(u::judge("v1.2.2", {{game, ""}, {hub, ""}}).release.tag == "v1.3.0");     // either order
  EXPECT(u::judge("v1.2.2", {{hub, ""}, {"", "404"}}).kind == u::Verdict::kUpToDate);  // mech3-re not answering
  EXPECT(u::judge("v1.2.1", {{"", "timed out"}, {hub, ""}}).release.tag == "v1.2.2");
  EXPECT(u::judge("v1.2.2", {{"<html>", ""}, {game, ""}}).release.tag == "v1.3.0");     // one unreadable
  const u::Verdict none = u::judge("v1.2.2", {{"", "could not resolve host"}, {"", "timed out"}});
  EXPECT(none.kind == u::Verdict::kFailed && none.words == "Could not check for updates: could not resolve host. Is this computer online?");
}

namespace {
// The same list as git.owenmorgan.com's Forgejo gives it: its own download addresses.
std::string on_backup(std::string json) {
  for (size_t at; (at = json.find("https://github.com/")) != std::string::npos;) json.replace(at, 19, "https://git.owenmorgan.com/");
  return json;
}
}  // namespace

// Both homes (2026-11-22): git.owenmorgan.com asked first, then GitHub (download_first); the newest release on
// either wins, one home down is fine, and a release both have downloads from GitHub with the backup's copy behind it.
TEST(update_reads_both_homes) {
  const std::string gh = list({release("v1.3.0"), release("v1.2.0")});
  const auto backup = [](const std::string& json) { return u::Answer{on_backup(json), "", false}; };
  const auto github = [](const std::string& json) { return u::Answer{json, "", true}; };
  const auto down = [](bool first) { return u::Answer{"", "Could not resolve host", first}; };
  // The same newest on both: GitHub's file first, the backup's behind it, whatever order they answered in.
  for (const auto& answers : {std::vector<u::Answer>{backup(gh), github(gh)}, std::vector<u::Answer>{github(gh), backup(gh)}}) {
    const u::Verdict v = u::judge("v1.2.0", answers, u::Platform::kMac);
    EXPECT(v.kind == u::Verdict::kNewer && v.release.tag == "v1.3.0");
    EXPECT(v.release.assets.size() == 4 && v.release.assets[0].url == "https://github.com/r/v1.3.0/mac.dmg");
    EXPECT(v.release.other_homes.size() == 1 && v.release.other_homes[0][0].url == "https://git.owenmorgan.com/r/v1.3.0/mac.dmg");
  }
  // Newest on GitHub only (the backup not synced yet): GitHub's, nothing behind it.
  const u::Verdict gh_only = u::judge("v1.2.0", {backup(list({release("v1.2.0")})), github(gh)}, u::Platform::kMac);
  EXPECT(gh_only.kind == u::Verdict::kNewer && gh_only.release.tag == "v1.3.0" && gh_only.release.other_homes.empty() &&
         gh_only.release.assets[0].url == "https://github.com/r/v1.3.0/mac.dmg");
  // Newest on the backup only (forgotten on GitHub, or GitHub gone): the backup's.
  const u::Verdict b_only = u::judge("v1.2.0", {backup(list({release("v1.4.0"), release("v1.3.0")})), github(gh)}, u::Platform::kMac);
  std::printf("  %s\n", b_only.words.c_str());
  EXPECT(b_only.kind == u::Verdict::kNewer && b_only.release.tag == "v1.4.0" && b_only.release.other_homes.empty() &&
         b_only.release.assets[0].url == "https://git.owenmorgan.com/r/v1.4.0/mac.dmg");
  // One home down, either one: the other's answer is enough.
  EXPECT(u::judge("v1.2.0", {down(false), github(gh)}, u::Platform::kMac).release.tag == "v1.3.0");
  const u::Verdict gh_down = u::judge("v1.2.0", {backup(gh), down(true)}, u::Platform::kMac);
  EXPECT(gh_down.kind == u::Verdict::kNewer && gh_down.release.assets[0].url == "https://git.owenmorgan.com/r/v1.3.0/mac.dmg");
  EXPECT(u::judge("v1.3.0", {backup("<html>"), github(gh)}, u::Platform::kMac).kind == u::Verdict::kUpToDate);
  // Both down: the first one's reason.
  const u::Verdict none = u::judge("v1.2.0", {down(false), down(true)}, u::Platform::kMac);
  EXPECT(none.kind == u::Verdict::kFailed && none.words == "Could not check for updates: Could not resolve host. Is this computer online?");
  // GitHub's copy without this platform's file: the backup's is the release, GitHub's not behind it as a file.
  const std::string no_mac =
      "[{\"tag_name\":\"v1.3.0\",\"draft\":false,\"prerelease\":false,\"body\":\"x\",\"assets\":["
      "{\"name\":\"SHA256SUMS\",\"browser_download_url\":\"https://github.com/r/v1.3.0/SHA256SUMS\"}]}]";
  const u::Verdict partial = u::judge("v1.2.0", {backup(gh), github(no_mac)}, u::Platform::kMac);
  EXPECT(partial.release.tag == "v1.3.0" && partial.release.assets[0].url == "https://git.owenmorgan.com/r/v1.3.0/mac.dmg" &&
         partial.release.other_homes.size() == 1 && partial.release.other_homes[0].size() == 1);
  // A draft on one home is no copy of the release.
  const u::Verdict drafted = u::judge("v1.2.0", {backup(list({release("v1.3.0", true)})), github(gh)}, u::Platform::kMac);
  EXPECT(drafted.release.tag == "v1.3.0" && drafted.release.other_homes.empty());
}

// 2026-11-21: each game is released on its own repo too (mech3-re's v1.3.0 with MW3-alone downloads only),
// the launcher's bundle on mw3-re's. A release without this platform's bundle is never the update, whatever its tag.
TEST(update_never_takes_a_games_own_release) {
  const std::string alone =
      "[{\"tag_name\":\"v1.3.0\",\"draft\":false,\"prerelease\":false,\"body\":\"MW3 alone.\",\"assets\":["
      "{\"name\":\"MW3-alone-Setup-v1.3.0.exe\",\"browser_download_url\":\"https://x/a.exe\"},"
      "{\"name\":\"MW3-alone-v1.3.0-mac.dmg\",\"browser_download_url\":\"https://x/a.dmg\"},"
      "{\"name\":\"MW3-alone-v1.3.0-x86_64.AppImage\",\"browser_download_url\":\"https://x/a.AppImage\"}]}]";
  const std::string hub = list({release("v1.2.3")});
  for (const u::Platform p : {u::Platform::kWindows, u::Platform::kMac, u::Platform::kLinux}) {
    const u::Verdict v = u::judge("v1.2.2", {{hub, ""}, {alone, ""}}, p);
    EXPECT(v.kind == u::Verdict::kNewer && v.release.tag == "v1.2.3");  // the hub's bundle, not mech3-re's v1.3.0
    EXPECT(u::judge("v1.2.3", {{hub, ""}, {alone, ""}}, p).kind == u::Verdict::kUpToDate);
  }
  // Only a game's release answering: no bundle anywhere, said as such (never a download of the wrong thing).
  const u::Verdict none = u::judge("v1.2.2", {{alone, ""}}, u::Platform::kMac);
  std::printf("  %s\n", none.words.c_str());
  EXPECT(none.kind == u::Verdict::kFailed);
  // The Mac's bundle alone on a release: the Mac takes it, Windows does not.
  const std::string mac_only =
      "[{\"tag_name\":\"v1.2.4\",\"draft\":false,\"prerelease\":false,\"body\":\"x\",\"assets\":["
      "{\"name\":\"MW3-Rebuild-v1.2.4-mac.dmg\",\"browser_download_url\":\"https://x/m.dmg\"}]}]";
  EXPECT(u::judge("v1.2.3", {{hub, ""}, {mac_only, ""}}, u::Platform::kMac).release.tag == "v1.2.4");
  EXPECT(u::judge("v1.2.3", {{hub, ""}, {mac_only, ""}}, u::Platform::kWindows).kind == u::Verdict::kUpToDate);
}

TEST(update_notes_as_plain_lines) {
  const std::string md = "## MechWarrior 3\r\n\r\n- **Fixed:** the `c1m2` crash, see [the report](https://example.com/x).\n"
                         "  - A *nested* item with a file_name\n\n\n### Pirate's Moon\n* Lancemates follow again\n\n";
  const std::vector<std::string> lines = u::notes_lines(md);
  for (const std::string& l : lines) std::printf("  |%s|\n", l.c_str());
  const std::vector<std::string> want = {"MechWarrior 3", "", "\xe2\x80\xa2 Fixed: the c1m2 crash, see the report.",
                                         "    \xe2\x80\xa2 A nested item with a file_name", "", "Pirate's Moon", "\xe2\x80\xa2 Lancemates follow again"};
  EXPECT(lines == want);
  EXPECT(u::notes_lines("").empty() && u::notes_lines("\n\n").empty());
}

TEST(update_picks_the_platforms_package_and_its_sum) {
  u::Release r;
  r.tag = "v1.2.0";
  // The games alone (release.md, The combined package) first, so a pick that took them would show.
  for (const char* n : {"MW3-alone-Setup-1.2.0.exe", "MW3-alone-1.2.0-mac.dmg", "MW3-alone-1.2.0-x86_64.AppImage", "PM-Rebuild-Setup-pm-v1.0.2.exe",
                        "PM-Rebuild-pm-v1.0.2-mac.dmg", "PM-Rebuild-pm-v1.0.2-x86_64.AppImage", "mw3pm-pm-v1.0.2-win64.zip",
                        "mw3-1.2.0-win64.zip", "MW3-Rebuild-Setup-1.2.0.exe", "MW3-Rebuild-1.2.0-mac.dmg", "MW3-Rebuild-1.2.0-x86_64.AppImage",
                        "mw3pm-1.0.2-mac.zip", "SHA256SUMS"})
    r.assets.push_back({n, std::string("https://x/") + n, 100});
  EXPECT(u::platform_asset(r, u::Platform::kWindows)->name == "MW3-Rebuild-Setup-1.2.0.exe");  // not the game alone's zip
  EXPECT(u::platform_asset(r, u::Platform::kMac)->name == "MW3-Rebuild-1.2.0-mac.dmg");
  EXPECT(u::platform_asset(r, u::Platform::kLinux)->name == "MW3-Rebuild-1.2.0-x86_64.AppImage");
  r.assets.erase(r.assets.begin() + 8, r.assets.begin() + 11);  // the combined ones gone: the games alone never taken
  EXPECT(!u::platform_asset(r, u::Platform::kMac) && !u::platform_asset(r, u::Platform::kWindows));
  const std::string h1(64, 'a'), h2 = "0123456789ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef";
  const std::string sums = h1 + "  MW3-Rebuild-1.2.0-mac.dmg\r\n" + h2 + " *MW3-Rebuild-Setup-1.2.0.exe\nnot a line\n" + std::string(63, 'b') + "  short.dmg\n";
  EXPECT(u::sums_line(sums, "MW3-Rebuild-1.2.0-mac.dmg") == h1);
  EXPECT(u::sums_line(sums, "MW3-Rebuild-Setup-1.2.0.exe") == "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");  // binary mode, lower case
  EXPECT(u::sums_line(sums, "MW3-Rebuild-1.2.0-x86_64.AppImage").empty() && u::sums_line(sums, "short.dmg").empty());
}

#include <filesystem>
#include <fstream>
#include <iterator>
#if defined(__APPLE__)
#include <sys/stat.h>
#endif

// Sitting 4's swap: a folder (a Mac app) and a file (an AppImage, its execute bit kept) put in place; the old kept
// whenever it cannot be done; an earlier try's leftovers cleared; nothing beside touched.
TEST(update_swaps_the_program_in_or_keeps_the_old) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path root = fs::path(MW3_TEST_TMP) / "update_swap";
  fs::remove_all(root, ec);
  const auto put = [&](const fs::path& p, const std::string& t) {
    fs::create_directories(p.parent_path(), ec);
    std::ofstream(p) << t;
  };
  const auto text = [](const fs::path& p) {
    std::ifstream f(p);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  };
  // A Mac app: the old one's files all replaced by the new one's.
  put(root / "Apps" / "MW3 Rebuild.app" / "Contents" / "MacOS" / "mw3launcher", "old");
  put(root / "Apps" / "MW3 Rebuild.app" / "Contents" / "MacOS" / "only-in-old", "x");
  put(root / "Apps" / "Something Else.app" / "keep", "theirs");
  put(root / "dmg" / "MW3 Rebuild.app" / "Contents" / "MacOS" / "mw3launcher", "new");
  put(root / "Apps" / "MW3 Rebuild.app.old" / "Contents" / "MacOS" / "mw3launcher", "an earlier try's");
  std::string error;
  // A folder of the player's where a leftover would be: kept, and the update stops (never-delete-foreign-folders).
  put(root / "Apps" / "MW3 Rebuild.app.new" / "notes.txt", "theirs");
  EXPECT(!u::swap_in((root / "dmg" / "MW3 Rebuild.app").string(), (root / "Apps" / "MW3 Rebuild.app").string(), &error) &&
         text(root / "Apps" / "MW3 Rebuild.app.new" / "notes.txt") == "theirs" &&
         text(root / "Apps" / "MW3 Rebuild.app" / "Contents" / "MacOS" / "mw3launcher") == "old");
  fs::remove_all(root / "Apps" / "MW3 Rebuild.app.new", ec);
#if defined(__APPLE__)
  const auto inode = [](const fs::path& p) {
    struct stat st = {};
    return stat(p.c_str(), &st) == 0 ? st.st_ino : 0;
  };
  const ino_t was = inode(root / "Apps" / "MW3 Rebuild.app");
#endif
  EXPECT(u::swap_in((root / "dmg" / "MW3 Rebuild.app").string(), (root / "Apps" / "MW3 Rebuild.app").string(), &error));
#if defined(__APPLE__)
  // The Dock follows the app's own folder: the same one after the update, never renamed to .old.
  EXPECT(was != 0 && inode(root / "Apps" / "MW3 Rebuild.app") == was);
#endif
  EXPECT(text(root / "Apps" / "MW3 Rebuild.app" / "Contents" / "MacOS" / "mw3launcher") == "new" &&
         !fs::exists(root / "Apps" / "MW3 Rebuild.app" / "Contents" / "MacOS" / "only-in-old"));
  EXPECT(!fs::exists(root / "Apps" / "MW3 Rebuild.app.old") && !fs::exists(root / "Apps" / "MW3 Rebuild.app.new"));
  EXPECT(text(root / "Apps" / "Something Else.app" / "keep") == "theirs");
  // An AppImage file, executable.
  put(root / "bin" / "MW3-Rebuild-x86_64.AppImage", "old image");
  put(root / "dl" / "MW3-Rebuild-1.2.0-x86_64.AppImage", "new image");
  fs::permissions(root / "dl" / "MW3-Rebuild-1.2.0-x86_64.AppImage", fs::perms::owner_all, ec);
  EXPECT(u::swap_in((root / "dl" / "MW3-Rebuild-1.2.0-x86_64.AppImage").string(), (root / "bin" / "MW3-Rebuild-x86_64.AppImage").string(), &error));
  EXPECT(text(root / "bin" / "MW3-Rebuild-x86_64.AppImage") == "new image" &&
         (fs::status(root / "bin" / "MW3-Rebuild-x86_64.AppImage").permissions() & fs::perms::owner_exec) != fs::perms::none);
  // Cannot be done: the old kept, nothing left beside it.
  EXPECT(!u::swap_in((root / "nothing").string(), (root / "bin" / "MW3-Rebuild-x86_64.AppImage").string(), &error) &&
         text(root / "bin" / "MW3-Rebuild-x86_64.AppImage") == "new image");
  EXPECT(!u::swap_in((root / "dl" / "MW3-Rebuild-1.2.0-x86_64.AppImage").string(), (root / "bin" / "gone").string(), &error));
  put(root / "ro" / "MW3-Rebuild-x86_64.AppImage", "kept");
  error.clear();
#if defined(_WIN32)
  // Windows ignores a folder's read-only mark; what stops it is the program held open (no delete sharing), as a
  // running one is.
  std::ifstream held(root / "ro" / "MW3-Rebuild-x86_64.AppImage");
#else
  fs::permissions(root / "ro", fs::perms::owner_read | fs::perms::owner_exec, ec);  // a folder it may not write in
#endif
  const bool done = u::swap_in((root / "dl" / "MW3-Rebuild-1.2.0-x86_64.AppImage").string(), (root / "ro" / "MW3-Rebuild-x86_64.AppImage").string(), &error);
  std::printf("  cannot be replaced: %s\n", error.c_str());
#if defined(_WIN32)
  held.close();
#else
  fs::permissions(root / "ro", fs::perms::owner_all, ec);
#endif
  EXPECT(!done && text(root / "ro" / "MW3-Rebuild-x86_64.AppImage") == "kept" && !fs::exists(root / "ro" / "MW3-Rebuild-x86_64.AppImage.new"));
  fs::remove_all(root, ec);
}

// Games on demand (src/upgrades/update/games.h): each game's tags and files, the newest for a platform, the record.
namespace {
std::string game_release(const std::string& tag, const std::vector<std::string>& files, bool draft = false) {
  std::string s = "{\"tag_name\":\"" + tag + "\",\"draft\":" + (draft ? "true" : "false") + ",\"prerelease\":false,\"body\":\"\",\"assets\":[";
  for (size_t i = 0; i < files.size(); ++i)
    s += std::string(i ? "," : "") + "{\"name\":\"" + files[i] + "\",\"size\":31457280,\"browser_download_url\":\"https://github.com/x/" + files[i] + "\"}";
  return s + "]}";
}
}  // namespace

TEST(games_tags_and_files) {
  using G = u::Game;
  EXPECT(u::game_tag(G::kMw3, "v1.2.4") && !u::game_tag(G::kMw3, "pm-v1.2.3") && !u::game_tag(G::kMw3, "v1.2.4-rc1"));
  EXPECT(u::game_tag(G::kPm, "pm-v1.2.3") && !u::game_tag(G::kPm, "v1.2.3") && !u::game_tag(G::kPm, "mw1-v1.0.6"));
  EXPECT(u::game_tag(G::kMw1, "mw1-v1.0.7") && !u::game_tag(G::kMw1, "mw1-v1.0"));
  // The names the packaging gives them (docs/release.md); Windows' zip, never its Setup.
  EXPECT(u::game_asset_name(G::kMw3, "v1.2.4", u::Platform::kWindows) == "MW3-alone-v1.2.4-win64.zip");
  EXPECT(u::game_asset_name(G::kPm, "pm-v1.2.3", u::Platform::kWindows) == "mw3pm-pm-v1.2.3-win64.zip");
  EXPECT(u::game_asset_name(G::kPm, "pm-v1.2.3", u::Platform::kMac) == "PM-Rebuild-pm-v1.2.3-mac.dmg");
  EXPECT(u::game_asset_name(G::kMw1, "mw1-v1.0.7", u::Platform::kLinux) == "MW1-Remake-mw1-v1.0.7-x86_64.AppImage");
  EXPECT(u::game_version("pm-v1.2.3") == "1.2.3" && u::game_version("v1.2.4") == "1.2.4");
  // MW1's prefix has a digit of its own (found on the PC: "1-v1.0.7", and every mw1 tag compared equal).
  EXPECT(u::game_version("mw1-v1.0.7") == "1.0.7" && u::compare_game("mw1-v1.0.7", "mw1-v1.0.6") > 0 &&
         u::compare_game("mw1-v1.0.10", "mw1-v1.0.9") > 0 && u::compare_game("mw1-v1.0.7", "mw1-v1.0.7") == 0);
  EXPECT(u::size_words(31457280) == "30 MB" && u::size_words(1000) == "under 1 MB");
}

TEST(games_newest_for_the_platform) {
  using G = u::Game;
  // PM's repo: a newer release without the Mac's file, a draft, and the hub-style combined files are passed over.
  const std::string json = list({game_release("pm-v1.2.4", {"mw3pm-pm-v1.2.4-win64.zip", "SHA256SUMS"}),
                                 game_release("pm-v1.2.5", {"PM-Rebuild-pm-v1.2.5-mac.dmg"}, true),
                                 game_release("pm-v1.2.3", {"PM-Rebuild-pm-v1.2.3-mac.dmg", "mw3pm-pm-v1.2.3-win64.zip", "SHA256SUMS"}),
                                 game_release("v1.2.4", {"MW3-Rebuild-v1.2.4-mac.dmg"})});
  u::GameCheck c = u::judge_game(G::kPm, "", {{json, ""}}, u::Platform::kMac);
  EXPECT(c.kind == u::GameCheck::kNewer && c.release.tag == "pm-v1.2.3" && c.asset() && c.asset()->name == "PM-Rebuild-pm-v1.2.3-mac.dmg");
  std::printf("  %s\n", c.words.c_str());
  c = u::judge_game(G::kPm, "", {{json, ""}}, u::Platform::kWindows);
  EXPECT(c.kind == u::GameCheck::kNewer && c.release.tag == "pm-v1.2.4");
  EXPECT(u::judge_game(G::kPm, "pm-v1.2.3", {{json, ""}}, u::Platform::kMac).kind == u::GameCheck::kUpToDate);
  EXPECT(u::judge_game(G::kPm, "pm-v1.2.2", {{json, ""}}, u::Platform::kMac).kind == u::GameCheck::kNewer);
  EXPECT(u::judge_game(G::kPm, "pm-v1.2.4", {{json, ""}}, u::Platform::kMac).kind == u::GameCheck::kUpToDate);  // a newer one from a file
  EXPECT(u::judge_game(G::kMw1, "", {{json, ""}}, u::Platform::kMac).kind == u::GameCheck::kNoRelease);
  const std::string mw1 = list({game_release("mw1-v1.0.6", {"MW1-Remake-mw1-v1.0.6-mac.dmg"}),
                                game_release("mw1-v1.0.7", {"MW1-Remake-mw1-v1.0.7-mac.dmg"})});
  const u::GameCheck m = u::judge_game(G::kMw1, "mw1-v1.0.6", {{mw1, ""}}, u::Platform::kMac);
  EXPECT(m.kind == u::GameCheck::kNewer && m.release.tag == "mw1-v1.0.7");
  std::printf("  %s\n", m.words.c_str());
  EXPECT(u::judge_game(G::kMw1, "mw1-v1.0.7", {{mw1, ""}}, u::Platform::kMac).kind == u::GameCheck::kUpToDate);
  const u::GameCheck off = u::judge_game(G::kMw3, "v1.2.4", {{"", "Could not resolve host"}}, u::Platform::kMac);
  EXPECT(off.kind == u::GameCheck::kFailed && off.words.find("Could not resolve host") != std::string::npos);
  EXPECT(u::judge_game(G::kMw3, "", {{"<html>", ""}}, u::Platform::kMac).kind == u::GameCheck::kFailed);
}

// A game's repo in both homes (2026-11-22): the newest on either, one home down fine, GitHub's file first.
TEST(games_read_both_homes) {
  using G = u::Game;
  const std::string gh = list({game_release("pm-v1.2.3", {"PM-Rebuild-pm-v1.2.3-mac.dmg", "SHA256SUMS"})});
  const std::string newer = list({game_release("pm-v1.2.4", {"PM-Rebuild-pm-v1.2.4-mac.dmg", "SHA256SUMS"}),
                                  game_release("pm-v1.2.3", {"PM-Rebuild-pm-v1.2.3-mac.dmg", "SHA256SUMS"})});
  const auto backup = [](const std::string& json) { return u::Answer{on_backup(json), "", false}; };
  const auto github = [](const std::string& json) { return u::Answer{json, "", true}; };
  // The same on both: GitHub's file, the backup's behind it.
  u::GameCheck c = u::judge_game(G::kPm, "", {backup(gh), github(gh)}, u::Platform::kMac);
  EXPECT(c.kind == u::GameCheck::kNewer && c.asset() && c.asset()->url == "https://github.com/x/PM-Rebuild-pm-v1.2.3-mac.dmg");
  EXPECT(c.release.other_homes.size() == 1 && c.release.other_homes[0][0].url == "https://git.owenmorgan.com/x/PM-Rebuild-pm-v1.2.3-mac.dmg");
  // Newer on the backup only, then on GitHub only.
  c = u::judge_game(G::kPm, "pm-v1.2.3", {backup(newer), github(gh)}, u::Platform::kMac);
  std::printf("  %s\n", c.words.c_str());
  EXPECT(c.kind == u::GameCheck::kNewer && c.release.tag == "pm-v1.2.4" && c.asset()->url.rfind("https://git.owenmorgan.com/", 0) == 0);
  c = u::judge_game(G::kPm, "pm-v1.2.3", {backup(gh), github(newer)}, u::Platform::kMac);
  EXPECT(c.kind == u::GameCheck::kNewer && c.release.tag == "pm-v1.2.4" && c.asset()->url.rfind("https://github.com/", 0) == 0 &&
         c.release.other_homes.empty());
  // One home down, either one; both down: the first one's reason.
  EXPECT(u::judge_game(G::kPm, "", {{"", "timed out", false}, github(gh)}, u::Platform::kMac).kind == u::GameCheck::kNewer);
  EXPECT(u::judge_game(G::kPm, "", {backup(gh), {"", "404", true}}, u::Platform::kMac).kind == u::GameCheck::kNewer);
  const u::GameCheck off = u::judge_game(G::kPm, "", {{"", "timed out", false}, {"", "404", true}}, u::Platform::kMac);
  EXPECT(off.kind == u::GameCheck::kFailed && off.words.find("timed out") != std::string::npos);
}

TEST(games_installed_record) {
  const u::InstalledGame in{"pm", "pm-v1.2.3", "PM-Rebuild-pm-v1.2.3-mac.dmg", "6c6b", "github"};
  u::InstalledGame back;
  EXPECT(u::parse_installed(u::installed_text(in), &back));
  EXPECT(back.game == "pm" && back.tag == "pm-v1.2.3" && back.file == in.file && back.sha256 == "6c6b" && back.from == "github");
  EXPECT(u::parse_installed("# x\r\ngame=mw3\r\ntag=v1.2.4\r\n", &back) && back.game == "mw3" && back.tag == "v1.2.4");  // CRLF
  EXPECT(!u::parse_installed("tag=v1.2.4\n", &back));  // no game: not a record
}

// The games catalog built in (games.json; docs/launcher-split.md b0, 2026-11-26): every family and game as the code's
// tables had them before it (copied here as they were), read alone.
// The shipped built-in catalog (refreshed from catalog/games.json at each launcher release; the checks use their own,
// tests/fixtures/builtin-catalog.json): it reads, and every game it names is whole. That it is the live one is the
// release's gate (tools/package/check_catalog.py), not a check here: the live catalog may move ahead between releases.
TEST(games_catalog_shipped_reads) {
  u::Catalog c;
  std::string why;
  const bool ok = u::parse_catalog(u::shipped_catalog_text(), &c, &why);
  std::printf("  shipped: revision %d, %d games, %d families%s%s\n", c.revision, static_cast<int>(c.games.size()),
              static_cast<int>(c.families.size()), ok ? "" : ": ", why.c_str());
  EXPECT(ok && c.revision >= 1 && !c.games.empty() && !c.families.empty());
  EXPECT(u::builtin_catalog_text() != u::shipped_catalog_text() || std::getenv("MW3_BUILTIN_CATALOG_FILE") == nullptr);
}

TEST(games_catalog_built_in_as_the_tables_were) {
  u::Catalog c;
  std::string why;
  EXPECT(u::parse_catalog(u::builtin_catalog_text(), &c, &why) && why.empty());
  std::printf("  catalog %d: %zu families, %zu games%s%s\n", c.version, c.families.size(), c.games.size(), why.empty() ? "" : ": ", why.c_str());
  EXPECT(c.version == 1 && c.families.size() == 3 && c.games.size() == 6);
  if (c.families.size() == 3)
    EXPECT(c.families[0].key == "mw3" && c.families[0].name == "MechWarrior 3" && c.families[1].key == "mw1" &&
           c.families[1].name == "MechWarrior 1" && c.families[2].key == "inception" && c.families[2].name == "The Crescent Hawk's Inception");
  // games.cpp's game_info table, launcher.cpp's kHeaderNames and the own setup's badges, by Game.
  struct Was {
    const char *key, *name, *repo, *prefix, *program, *heading, *heading_badge;
  };
  const Was was[] = {{"mw3", "MechWarrior 3", "mech3-re", "v", "mw3", "MECHWARRIOR 3", "badge_mw3.png"},
                     {"pm", "Pirate's Moon", "mech3-pm-re", "pm-v", "mw3pm", "MECHWARRIOR 3: PIRATE'S MOON", "badge_pm.png"},
                     {"mw1", "MechWarrior 1 Remake", "mech1-remake-re", "mw1-v", "mw1", "MECHWARRIOR 1 REMAKE", "badge_mw1.png"},
                     {"mw1r", "MechWarrior 1 Rebuild", "mech1-re", "mw1r-v", "mw1r", "MECHWARRIOR 1", "badge_mw1o.png"},
                     {"chi", "Inception Rebuild", "inception-re", "chi-v", "chi", "THE CRESCENT HAWK'S INCEPTION", "badge_inception.png"},
                     {"chirm", "Inception Remake", "inception-remake-re", "chirm-v", "chirm", "INCEPTION REMAKE", "badge_inception.png"}};
  for (int i = 0; i < u::kGameCount; ++i) {
    const u::GameInfo& g = u::game_info(u::kGames[i]);
    const u::CatalogGame& e = u::catalog_game(u::kGames[i]);
    EXPECT(std::string(g.key) == was[i].key && std::string(g.name) == was[i].name && std::string(g.repo) == was[i].repo &&
           std::string(g.prefix) == was[i].prefix && std::string(g.program) == was[i].program);
    EXPECT(e.heading == was[i].heading && (e.heading_badge.empty() ? e.badge : e.heading_badge) == was[i].heading_badge);
    u::Game back;
    EXPECT(u::game_of_key(was[i].key, &back) && back == u::kGames[i]);
  }
  // launcher.cpp's panel rows (top to bottom), their badges and its panel buttons' names.
  struct Row {
    const char *key, *title, *blurb, *badge, *button;
    bool before_release;
  };
  const Row rows[] = {
      {"mw3", "MECHWARRIOR 3", "Zipper and MicroProse, 1999, rebuilt from the original Mech3.exe.", "badge_mw3.png", "MECHWARRIOR 3", true},
      {"pm", "PIRATE'S MOON", "The 1999 expansion, rebuilt from its own exe. Plays without MW3.", "badge_pm.png", "PIRATE'S MOON", true},
      {"mw1r", "MECHWARRIOR 1: REVERSE ENGINEERED", "MechWarrior (1989), reverse engineered and rebuilt. Built on Elthial's SDL port. Bring your own DOS copy.",
       "badge_mw1o.png", "MECHWARRIOR 1 ORIGINAL", true},
      {"mw1", "MECHWARRIOR 1 REMAKE", "MechWarrior (1989)'s campaign, fought with MechWarrior 3's combat.", "badge_mw1.png", "MECHWARRIOR 1", true},
      {"chi", "THE CRESCENT HAWK'S INCEPTION", "The 1988 game, rebuilt. Bring your own copy: DOS 1.03 or Amiga 2.3.", "badge_inception.png",
       "INCEPTION ORIGINAL", true},
      {"chirm", "INCEPTION REMAKE", "The Crescent Hawk's Inception (1988), remade.", "badge_inception.png", "INCEPTION REMAKE", false}};
  for (size_t i = 0; i < c.games.size() && i < 6; ++i) {
    const u::CatalogGame& e = c.games[i];
    EXPECT(e.key == rows[i].key && e.title == rows[i].title && e.description == rows[i].blurb && e.badge == rows[i].badge &&
           e.button == rows[i].button && e.panel_before_release == rows[i].before_release && e.min_launcher == "1.5.2");
  }
  EXPECT(c.games.size() == 6 && c.games[3].needs == std::vector<std::string>({"mw3", "pm"}) && c.games[0].needs.empty());
  EXPECT(c.games.size() == 6 && c.games[5].state == u::CatalogGame::kComingSoon && c.games[4].state == u::CatalogGame::kReleased);
  // The files of a release, as game_asset_name made them before the catalog.
  auto was_asset = [](u::Game g, const std::string& tag, u::Platform p) {
    const char* stem = g == u::Game::kMw3    ? "MW3-alone"
                       : g == u::Game::kPm   ? (p == u::Platform::kWindows ? "mw3pm" : "PM-Rebuild")
                       : g == u::Game::kMw1  ? "MW1-Remake"
                       : g == u::Game::kMw1r ? "MW1-Rebuild"
                       : g == u::Game::kChi  ? "CHI-Rebuild"
                                             : "CHI-Remake";
    const char* tail = p == u::Platform::kWindows ? "-win64.zip" : p == u::Platform::kMac ? "-mac.dmg" : "-x86_64.AppImage";
    return std::string(stem) + "-" + tag + tail;
  };
  for (u::Game g : u::kGames)
    for (u::Platform p : {u::Platform::kWindows, u::Platform::kMac, u::Platform::kLinux})
      EXPECT(u::game_asset_name(g, "x-v1.2.3", p) == was_asset(g, "x-v1.2.3", p));
}

// A catalog that is not one the launcher can use is refused, saying which entry and what.
TEST(games_catalog_refuses_bad_entries) {
  const std::string good = u::builtin_catalog_text();
  auto refused = [&](const std::string& from, const std::string& to, const char* says) {
    std::string text = good;
    const size_t at = text.find(from);
    EXPECT(at != std::string::npos);
    if (at == std::string::npos) return;
    text.replace(at, from.size(), to);
    u::Catalog c;
    std::string why;
    const bool ok = u::parse_catalog(text, &c, &why);
    std::printf("  %s -> %s\n", says, why.c_str());
    EXPECT(!ok && why.find(says) != std::string::npos);
  };
  refused("\"state\": \"coming_soon\"", "\"state\": \"soon\"", "games[5] (chirm): state soon is none of");
  refused("\"family\": \"inception\", \"order\": 6", "\"family\": \"inceptoin\", \"order\": 6", "its family inceptoin is not listed");
  refused("\"key\": \"chirm\"", "\"key\": \"chi\"", "games[5] (chi): the key twice");
  refused("\"title\": \"PIRATE'S MOON\", ", "", "games[1] (pm): no title");
  refused("\"mac\": \"PM-Rebuild-{tag}-mac.dmg\"", "\"mac\": \"PM-Rebuild-mac.dmg\"", "(pm) assets: mac has no {tag}");
  refused("\"needs\": [\"mw3\", \"pm\"]", "\"needs\": [\"mw3\", \"mw5\"]", "games (mw1): it needs mw5, which is not listed");
  refused("\"panel_before_release\": false", "\"panel_before_release\": \"no\"", "panel_before_release is not true or false");
  refused("{\"key\": \"mw1\", \"name\": \"MechWarrior 1\", \"order\": 2}", "{\"key\": \"mw1\", \"name\": \"MechWarrior 1\"}",
          "families[1]: no order number");
  refused("\"catalog\": 1", "\"catalog\": 0", "catalog is not a version");
  refused("\"games\": [", "\"games\": [,", "not JSON at byte");
  u::Catalog c;
  std::string why;
  EXPECT(!u::parse_catalog("[]", &c, &why) && why == "not a catalog (an object)");
}

// A fetched catalog (b7): a bad or untrusted entry left out and the rest kept; merged over the built-in one only
// when its revision is higher; the catalog in use never changes once a game is looked up.
namespace {
std::string catalog_entry(const std::string& key, const std::string& family, int order, const std::string& state, const std::string& repo,
                          const std::string& min = "1.5.2", const std::string& badge = "badge_inception.png") {
  return "{\"key\": \"" + key + "\", \"family\": \"" + family + "\", \"order\": " + std::to_string(order) + ", \"state\": \"" + state +
         "\", \"name\": \"N " + key + "\", \"title\": \"T " + key + "\", \"heading\": \"H " + key + "\", \"button\": \"B " + key +
         "\", \"description\": \"D " + key + "\", \"repo\": \"" + repo + "\", \"tag_prefix\": \"" + key + "-v\", \"program\": \"" + key +
         "\", \"assets\": {\"windows\": \"" + key + "-{tag}-win64.zip\", \"mac\": \"" + key + "-{tag}-mac.dmg\", \"linux\": \"" + key +
         "-{tag}-x86_64.AppImage\"}, \"badge\": \"" + badge + "\", \"min_launcher\": \"" + min + "\"}";
}
std::string fetched_catalog(int revision, int format = 1) {
  return "{\"catalog\": " + std::to_string(format) + ", \"revision\": " + std::to_string(revision) +
         ", \"families\": [{\"key\": \"inception\", \"name\": \"Inception\", \"order\": 3}, {\"key\": \"bt\", \"name\": \"BattleTech\", \"order\": 4}], "
         "\"games\": [" +
         catalog_entry("chi", "inception", 5, "removed", "inception-re") + ", " + catalog_entry("chirm", "inception", 6, "released", "inception-remake-re") +
         ", " + catalog_entry("btech", "bt", 7, "released", "battletech-re", "1.5.2", "badge_btech.png") + ", " +
         catalog_entry("owned", "bt", 8, "coming_soon", "telltaleatheist/owned-re") + ", " +
         catalog_entry("evil", "bt", 9, "released", "someoneelse/evil-re") + ", " + catalog_entry("future", "bt", 10, "released", "future-re", "99.0.0") +
         ", " + catalog_entry("art", "bt", 11, "released", "art-re", "1.5.2", "../badge.png") + ", " +
         catalog_entry("lost", "nowhere", 12, "released", "lost-re") + ", " + catalog_entry("odd", "bt", 13, "gone", "odd-re") + "]}";
}
}  // namespace

TEST(games_catalog_fetched_leaves_out_bad_entries) {
  u::Catalog c;
  std::vector<std::string> ignored;
  std::string why;
  EXPECT(u::read_fetched_catalog(fetched_catalog(2), "v1.5.2-16-g273545ba4-dirty", &c, &ignored, &why));
  for (const std::string& i : ignored) std::printf("  left out: %s\n", i.c_str());
  std::string keys;
  for (const u::CatalogGame& g : c.games) keys += g.key + " ";
  std::printf("  kept: %s\n", keys.c_str());
  EXPECT(keys == "chi chirm btech owned lost " && c.revision == 2 && c.families.size() == 2);  // lost: its family at the merge
  EXPECT(c.games.size() == 5 && c.games[3].repo == "owned-re");  // the owner's name taken off
  EXPECT(ignored.size() == 4);
  auto said = [&](const char* s) {
    for (const std::string& i : ignored)
      if (i.find(s) != std::string::npos) return true;
    return false;
  };
  EXPECT(said("games (evil): its repo someoneelse/evil-re is not under telltaleatheist"));
  EXPECT(said("games (future): it needs launcher 99.0.0 or newer (this is v1.5.2-16-g273545ba4-dirty)"));
  EXPECT(said("games (art): its art is not a plain .png name"));
  EXPECT(said("games[8] (odd): state gone is none of"));
  // A build between releases' "unknown": every min_launcher taken.
  EXPECT(u::read_fetched_catalog(fetched_catalog(2), "unknown", &c, &ignored, &why) && c.games.size() == 6 && ignored.size() == 3);
  // Not a catalog this launcher reads: nothing taken.
  EXPECT(!u::read_fetched_catalog(fetched_catalog(2, 2), "v1.5.2", &c, &ignored, &why) && why.find("a newer catalog format (2)") != std::string::npos);
  EXPECT(!u::read_fetched_catalog("<html>", "v1.5.2", &c, &ignored, &why) && why.find("not JSON") != std::string::npos);
}

// A fetched entry's names go into paths (the program run, the install and removal folders, the downloaded file):
// one that is not a plain name is left out.
TEST(games_catalog_fetched_leaves_out_names_that_are_paths) {
  auto with = [](std::string e, const std::string& from, const std::string& to) { return e.replace(e.find(from), from.size(), to); };
  const std::string text = "{\"catalog\": 1, \"revision\": 2, \"families\": [{\"key\": \"bt\", \"name\": \"BT\", \"order\": 4}], \"games\": [" +
                           catalog_entry("fine", "bt", 7, "released", "fine-re") + ", " + catalog_entry("up/key", "bt", 8, "released", "key-re") + ", " +
                           with(catalog_entry("prog", "bt", 9, "released", "prog-re"), "\"program\": \"prog\"", "\"program\": \"../prog\"") + ", " +
                           with(catalog_entry("asset", "bt", 10, "released", "asset-re"), "asset-{tag}-mac.dmg", "../asset-{tag}-mac.dmg") + "]}";
  u::Catalog c;
  std::vector<std::string> ignored;
  std::string why;
  EXPECT(u::read_fetched_catalog(text, "v1.5.2", &c, &ignored, &why));
  for (const std::string& i : ignored) std::printf("  left out: %s\n", i.c_str());
  EXPECT(c.games.size() == 1 && c.games[0].key == "fine" && ignored.size() == 3);
  for (const std::string& i : ignored) EXPECT(i.find("is not a plain name") != std::string::npos);
}

TEST(games_catalog_fetched_merges_over_the_built_in) {
  const u::Catalog& builtin = u::builtin_catalog();
  u::Catalog f;
  std::vector<std::string> ignored;
  std::string why;
  EXPECT(u::read_fetched_catalog(fetched_catalog(2), "v1.5.2", &f, &ignored, &why));
  std::vector<std::string> left;
  const u::Catalog m = u::merged_catalog(builtin, f, &left);
  std::string keys;
  for (const u::CatalogGame& g : m.games) keys += g.key + " ";
  std::printf("  merged: %s(revision %d, %d families)\n", keys.c_str(), m.revision, static_cast<int>(m.families.size()));
  EXPECT(keys == "mw3 pm mw1r mw1 chi chirm btech owned " && m.revision == 2 && m.families.size() == 4);
  EXPECT(left.size() == 1 && left[0] == "games (lost): its family nowhere is not listed");
  EXPECT(m.games.size() == 8 && m.games[4].state == u::CatalogGame::kRemoved && m.games[5].state == u::CatalogGame::kReleased);
  EXPECT(m.games.size() == 8 && m.games[0].title == "MECHWARRIOR 3" && m.games[4].title == "T chi" && m.families[2].name == "Inception");
  // As old as the built-in, or older: the built-in as it is.
  EXPECT(u::read_fetched_catalog(fetched_catalog(1), "v1.5.2", &f, &ignored, &why));
  const u::Catalog same = u::merged_catalog(builtin, f);
  EXPECT(same.games.size() == 6 && same.revision == builtin.revision && same.games[4].state == u::CatalogGame::kReleased);
  // Once the games are looked up, the catalog in use stays for the run (GameInfo points into it).
  EXPECT(u::game_count() == 6 && !u::use_catalog(m) && u::game_count() == 6 && u::active_catalog().revision == builtin.revision);
}

// The homes (2026-11-26, docs/launcher.md, Where the launcher looks): where every repo's releases list is asked, an
// address with {repo} in it. The built-in list is GitHub's then the mirror's; a fetched catalog's is in force only when
// its revision is higher, by order; one without the list keeps the built-in's.
namespace {
std::string with_homes(int revision, const std::string& homes) {
  const std::string c = fetched_catalog(revision);
  return "{\"homes\": " + homes + ", " + c.substr(1);
}
std::string home(const char* key, const std::string& releases, int order) {
  return std::string("{\"key\": \"") + key + "\", \"releases\": \"" + releases + "\", \"order\": " + std::to_string(order) + "}";
}
}  // namespace

TEST(games_catalog_homes_built_in_and_fetched) {
  const u::Catalog& builtin = u::builtin_catalog();
  EXPECT(builtin.has_homes && builtin.homes.size() == 2 && builtin.homes[0].key == "gh" && builtin.homes[1].key == "om");
  EXPECT(builtin.homes[0].releases.rfind("https://api.github.com/repos/", 0) == 0 && builtin.homes[1].releases.rfind("https://git.owenmorgan.com/", 0) == 0);
  EXPECT(u::active_homes().size() == 2 && u::active_homes()[0].key == "gh");
  EXPECT(builtin.catalog_sources.empty() && builtin.next_keys.empty());
  u::Catalog f;
  std::vector<std::string> ignored;
  std::string why;
  const std::string a = "https://releases.reverse-engineered.com/{repo}.json", b = "https://git.example.org/api/v1/repos/x/{repo}/releases?limit={count}";
  EXPECT(u::read_fetched_catalog(with_homes(2, "[" + home("b", b, 2) + ", " + home("a", a, 1) + "]"), "v1.5.2", &f, &ignored, &why));
  for (const std::string& i : ignored) std::printf("  left out: %s\n", i.c_str());
  EXPECT(f.has_homes && f.homes.size() == 2 && f.homes[0].releases == a && f.homes[1].key == "b");
  EXPECT(ignored.size() == 4);  // fetched_catalog's own four bad games, no more
  u::Catalog m = u::merged_catalog(builtin, f);
  EXPECT(m.homes.size() == 2 && m.homes[0].releases == a && m.homes[1].releases == b);
  // Not newer: the built-in's.
  EXPECT(u::read_fetched_catalog(with_homes(1, "[" + home("a", a, 1) + "]"), "v1.5.2", &f, &ignored, &why));
  m = u::merged_catalog(builtin, f);
  EXPECT(m.homes.size() == 2 && m.homes[0].key == "gh");
  // No list: the built-in's.
  EXPECT(u::read_fetched_catalog(fetched_catalog(2), "v1.5.2", &f, &ignored, &why) && !f.has_homes);
  m = u::merged_catalog(builtin, f);
  EXPECT(m.homes.size() == 2 && m.homes[0].key == "gh");
}

// A bad homes list is left out whole, said once, and the built-in's stays in force; the games read as before. The
// built-in catalog's own would fail it (a build's data).
TEST(games_catalog_bad_homes_leave_the_built_in) {
  const std::string ok = "https://git.example.org/{repo}";
  const std::string bad[] = {
      "{\"key\": \"a\"}",
      "[]",
      "[" + home("a", "http://git.example.org/{repo}", 1) + "]",
      "[" + home("a", "https://git.example.org/releases", 1) + "]",
      "[" + home("a", "https://git.example.org/{repo}/{repo}", 1) + "]",
      "[" + home("a", "https://git.example.org:8080/{repo}", 1) + "]",
      "[" + home("a", "https://me@git.example.org/{repo}", 1) + "]",
      "[" + home("a", "https://localhost/{repo}", 1) + "]",
      "[" + home("a", "https://git..example.org/{repo}", 1) + "]",
      "[" + home("a", "https://git.example.org/../{repo}", 1) + "]",
      "[" + home("a", "https://git.example.org/{repo} x", 1) + "]",
      "[" + home("a", "file:///t/{repo}.json", 1) + "]",
      "[{\"key\": \"a\", \"releases\": \"" + ok + "\"}]",
      "[{\"key\": \"a\", \"url\": \"https://git.example.org\", \"order\": 1}]",  // the older shape
      "[" + home("a", ok, 1) + ", " + home("b", ok, 2) + "]",
      "[" + home("a", "https://a.example.org/{repo}", 1) + ", " + home("b", "https://b.example.org/{repo}", 2) + ", " +
          home("c", "https://c.example.org/{repo}", 3) + ", " + home("d", "https://d.example.org/{repo}", 4) + ", " +
          home("e", "https://e.example.org/{repo}", 5) + "]",
  };
  for (const std::string& homes : bad) {
    u::Catalog f;
    std::vector<std::string> ignored;
    std::string why;
    const bool read = u::read_fetched_catalog(with_homes(2, homes), "v1.5.2", &f, &ignored, &why);
    std::string said;
    for (const std::string& i : ignored)
      if (i.find("(the built-in homes kept)") != std::string::npos) said += i;
    std::printf("  %s\n", said.c_str());
    EXPECT(read && !f.has_homes && f.homes.empty() && ignored.size() == 5 && !said.empty() && f.games.size() == 5);
    const u::Catalog m = u::merged_catalog(u::builtin_catalog(), f);
    EXPECT(m.homes.size() == 2 && m.homes[0].key == "gh" && m.games.size() == 8);
  }
  {  // four are taken
    u::Catalog f;
    std::vector<std::string> ignored;
    std::string why;
    EXPECT(u::read_fetched_catalog(with_homes(2, "[" + home("a", "https://a.example.org/{repo}", 1) + ", " + home("b", "https://b.example.org/{repo}", 2) +
                                                     ", " + home("c", "https://c.example.org/{repo}", 3) + ", " + home("d", "https://d.example.org/{repo}", 4) + "]"),
                                   "v1.5.2", &f, &ignored, &why) &&
           f.has_homes && f.homes.size() == 4);
  }
  std::string text = u::builtin_catalog_text(), why;
  text.replace(text.find("https://git.owenmorgan.com"), 26, "http://git.owenmorgan.com");
  u::Catalog c;
  EXPECT(!u::parse_catalog(text, &c, &why) && why.find("homes[1]") != std::string::npos);
}

// The catalog's own addresses (catalog_sources) and the keys it vouches for (next_keys): read from a fetched catalog,
// taken by the merge from it; a bad list left out whole, the rest read.
TEST(games_catalog_sources_and_next_keys) {
  const std::string k1(64, 'A'), k2(64, 'b');
  const auto with = [](const std::string& extra) { return "{" + extra + ", " + fetched_catalog(2).substr(1); };
  u::Catalog f;
  std::vector<std::string> ignored;
  std::string why;
  EXPECT(u::read_fetched_catalog(with("\"catalog_sources\": [\"https://a.example.org/c.json\", \"https://b.example.org/x/c.json\"], \"next_keys\": [\"" + k1 +
                                      "\", \"" + k2 + "\"]"),
                                 "v1.5.2", &f, &ignored, &why));
  EXPECT(f.catalog_sources.size() == 2 && f.catalog_sources[1] == "https://b.example.org/x/c.json" && ignored.size() == 4);
  EXPECT(f.next_keys.size() == 2 && f.next_keys[0] == std::string(64, 'a') && f.next_keys[1] == k2);
  EXPECT(u::read_fetched_catalog(with("\"revoked_keys\": [\"" + k2 + "\"], \"next_keys\": [\"" + k1 + "\"]"), "v1.5.2", &f, &ignored, &why));
  EXPECT(f.revoked_keys.size() == 1 && f.revoked_keys[0] == k2 && f.next_keys.size() == 1 && ignored.size() == 4);
  const u::Catalog m = u::merged_catalog(u::builtin_catalog(), f);
  EXPECT(m.catalog_sources == f.catalog_sources && m.next_keys == f.next_keys && m.revoked_keys == f.revoked_keys);
  const std::string bad[] = {
      "\"catalog_sources\": \"https://a.example.org/c.json\"",
      "\"catalog_sources\": [\"http://a.example.org/c.json\"]",
      "\"catalog_sources\": [\"https://a.example.org/c.json\", \"https://a.example.org/c.json\"]",
      "\"catalog_sources\": [\"https://a.example.org/../c.json\"]",
      "\"catalog_sources\": [3]",
      "\"catalog_sources\": [\"https://a.example.org/1\", \"https://a.example.org/2\", \"https://a.example.org/3\", \"https://a.example.org/4\", "
      "\"https://a.example.org/5\", \"https://a.example.org/6\", \"https://a.example.org/7\"]",
      "\"next_keys\": [\"" + k1.substr(1) + "\"]",
      "\"next_keys\": [\"" + std::string(64, 'g') + "\"]",
      "\"next_keys\": [\"" + k1 + "\", \"" + k1 + "\"]",
      "\"next_keys\": [\"" + k1 + "\", \"" + k2 + "\", \"" + std::string(64, 'c') + "\", \"" + std::string(64, 'd') + "\", \"" + std::string(64, 'e') + "\"]",
      "\"revoked_keys\": [\"" + k1.substr(2) + "\"]",
      "\"revoked_keys\": \"" + k1 + "\"",
  };
  for (const std::string& b : bad) {
    EXPECT(u::read_fetched_catalog(with(b), "v1.5.2", &f, &ignored, &why) && f.catalog_sources.empty() && f.next_keys.empty() &&
           f.revoked_keys.empty() && f.games.size() == 5);
    const bool said = std::any_of(ignored.begin(), ignored.end(), [](const std::string& i) {
      return (i.rfind("catalog_sources", 0) == 0 || i.rfind("next_keys", 0) == 0 || i.rfind("revoked_keys", 0) == 0) &&
             i.find("(left out)") != std::string::npos;
    });
    EXPECT(ignored.size() == 5 && said);
    std::printf("  %s\n", ignored.front().c_str());
  }
  std::string text = u::builtin_catalog_text();
  text.insert(1, "\"catalog_sources\": [\"http://a.example.org/c.json\"],");
  u::Catalog c;
  EXPECT(!u::parse_catalog(text, &c, &why) && why.find("catalog_sources") != std::string::npos);
}

// The art's sums (2026-11-26, docs/launcher.md, Signed data): badge_sha256 (and heading_badge_sha256, card_sha256)
// read and lower-cased; one not 64 hex digits, or a sum for art the entry does not name, leaves the entry out.
TEST(games_catalog_art_sums) {
  const auto with = [](const std::string& fields) {
    std::string c = fetched_catalog(2);
    const std::string at = "\"badge\": \"badge_btech.png\"";
    return c.replace(c.find(at), at.size(), at + ", " + fields);
  };
  const auto btech = [](const u::Catalog& c) -> const u::CatalogGame* {
    for (const u::CatalogGame& g : c.games)
      if (g.key == "btech") return &g;
    return nullptr;
  };
  u::Catalog f;
  std::vector<std::string> ignored;
  std::string why;
  EXPECT(u::read_fetched_catalog(with("\"badge_sha256\": \"" + std::string(64, 'E') + "\""), "v1.5.2", &f, &ignored, &why));
  EXPECT(btech(f) && btech(f)->badge_sha256 == std::string(64, 'e') && btech(f)->heading_badge_sha256.empty() && ignored.size() == 4);
  for (const std::string& bad : {"\"badge_sha256\": \"" + std::string(63, 'e') + "\"", "\"badge_sha256\": \"" + std::string(64, 'g') + "\"",
                                 "\"heading_badge_sha256\": \"" + std::string(64, 'e') + "\"", "\"card_sha256\": \"" + std::string(64, 'e') + "\""}) {
    EXPECT(u::read_fetched_catalog(with(bad), "v1.5.2", &f, &ignored, &why) && !btech(f) && ignored.size() == 5);
    for (const std::string& i : ignored)
      if (i.rfind("games (btech)", 0) == 0) std::printf("  %s\n", i.c_str());
  }
  for (const u::CatalogGame& g : u::builtin_catalog().games) EXPECT(g.badge_sha256.empty());  // carried art: no sum needed
}

// "original" (2026-11-26): a list of sources of the original game's files. Read and checked (https, a 64-hex sha256,
// lower-cased, a size, a plain file name); a bad one left out with its game kept.
namespace {
void set_env(const char* k, const char* v) {  // set, or unset with nullptr
#if defined(_WIN32)
  _putenv_s(k, v ? v : "");
#else
  if (v) setenv(k, v, 1);
  else unsetenv(k);
#endif
}
}  // namespace

TEST(games_catalog_original_read_and_unused) {
  auto with_original = [](const std::string& original) {
    std::string c = fetched_catalog(2);
    const std::string at = "\"key\": \"btech\"";
    return c.replace(c.find(at), at.size(), at + ", \"original\": " + original);
  };
  const std::string sha(64, 'A');
  u::Catalog f;
  std::vector<std::string> ignored;
  std::string why;
  const auto src = [&](const std::string& url, const std::string& more = "") {
    return "{\"url\": \"" + url + "\", \"sha256\": \"" + sha + "\", \"size\": 1234, \"name\": \"MechWarrior 3 (USA).zip\"" + more + "}";
  };
  EXPECT(u::read_fetched_catalog(with_original("[" + src("https://files.example.org/x/y.zip", ", \"note\": \"A note.\"") + ", " +
                                               src("https://other.example.org/y.zip") + "]"),
                                 "v1.5.2", &f, &ignored, &why));
  const u::CatalogGame* bt = nullptr;
  for (const u::CatalogGame& g : f.games)
    if (g.key == "btech") bt = &g;
  EXPECT(bt && bt->original.size() == 2 && bt->original[0].url == "https://files.example.org/x/y.zip" &&
         bt->original[0].sha256 == std::string(64, 'a') && bt->original[0].size == 1234 && bt->original[0].name == "MechWarrior 3 (USA).zip" &&
         bt->original[0].note == "A note." && bt->original[1].url == "https://other.example.org/y.zip" && ignored.size() == 4);
  for (const u::CatalogGame& g : u::builtin_catalog().games) EXPECT(g.original.empty());
  const std::string u0 = "https://files.example.org/x.zip";
  const std::string bad[] = {
      src(u0),  // not a list
      "[]",
      "[" + src(u0) + "," + src(u0) + "," + src(u0) + "," + src(u0) + "," + src(u0) + "]",
      "[" + src("http://files.example.org/x.zip") + "]",
      "[" + src("file:///x.zip") + "]",
      "[" + src("https://files.example.org/../x.zip") + "]",
      "[" + src("https://files.example.org/a b.zip") + "]",
      "[{\"url\": \"" + u0 + "\", \"sha256\": \"" + sha.substr(1) + "\", \"size\": 1, \"name\": \"x.zip\"}]",
      "[{\"url\": \"" + u0 + "\", \"sha256\": \"" + sha.substr(1) + "g\", \"size\": 1, \"name\": \"x.zip\"}]",
      "[{\"url\": \"" + u0 + "\", \"sha256\": \"" + sha + "\", \"name\": \"x.zip\"}]",
      "[{\"url\": \"" + u0 + "\", \"sha256\": \"" + sha + "\", \"size\": -5, \"name\": \"x.zip\"}]",
      "[{\"url\": \"" + u0 + "\", \"sha256\": \"" + sha + "\", \"size\": 1, \"name\": \"../x.zip\"}]",
      "[{\"url\": \"" + u0 + "\", \"sha256\": \"" + sha + "\", \"size\": 1}]",
      "[" + src(u0, ", \"note\": 3") + "]",
  };
  // file:// only in checks (MW3_GAMES_API set, as the games' stand-in downloads).
  EXPECT(u::read_fetched_catalog(with_original("[" + src("file:///t/x.zip") + "]"), "v1.5.2", &f, &ignored, &why) && ignored.size() == 5);
  set_env("MW3_GAMES_API", "file:///t/{repo}.json");
  EXPECT(u::read_fetched_catalog(with_original("[" + src("file:///t/x.zip") + "]"), "v1.5.2", &f, &ignored, &why) && ignored.size() == 4);
  EXPECT(u::read_fetched_catalog(with_original("[" + src("file:///t/../x.zip") + "]"), "v1.5.2", &f, &ignored, &why) && ignored.size() == 5);
  set_env("MW3_GAMES_API", nullptr);
  for (const std::string& o : bad) {
    EXPECT(u::read_fetched_catalog(with_original(o), "v1.5.2", &f, &ignored, &why));
    bt = nullptr;
    for (const u::CatalogGame& g : f.games)
      if (g.key == "btech") bt = &g;
    std::string said;
    for (const std::string& i : ignored)
      if (i.find("(left out, the game kept)") != std::string::npos) said = i;
    std::printf("  %s\n", said.c_str());
    EXPECT(bt && bt->original.empty() && ignored.size() == 5 && said.find("games[2] (btech): its original") == 0);
  }
}

// Where the launcher looks (2026-11-26, docs/launcher.md, Where the launcher looks): sources.json read and checked;
// the shipped file used when it reads, the built-in copy (the same file, built in) when it is missing or bad; the keys
// learned from signed catalogs added after the shipped ones; the releases lists whose unsigned SHA256SUMS are taken.
TEST(sources_read_shipped_and_built_in) {
  namespace fs = std::filesystem;
  const u::Sources keep = u::active_sources();
  u::Sources s;
  std::string why, said;
  EXPECT(u::read_sources(u::builtin_sources_text(), false, &s, &why));
  EXPECT(s.catalog.size() == 3 && s.catalog[0] == "https://reverse-engineered.com/launcher/catalog.json" &&
         s.catalog[1].rfind("https://raw.githubusercontent.com/", 0) == 0 && s.catalog[2].rfind("https://git.owenmorgan.com/", 0) == 0);
  EXPECT(s.unsigned_sums.size() == 1 && s.unsigned_sums[0] == "https://api.github.com/repos/telltaleatheist/");
  EXPECT(s.keys.size() == 1 && s.keys[0] == "4e2d26a76ab9f92876ca86aa915b9d61de1ed93b854610be07c70179136bfced");  // the release key
  {  // the built-in copy is the shipped file, byte for byte
    std::ifstream f(fs::path(MW3_ASSETS_FOR_TESTS) / "launcher" / "sources.json", std::ios::binary);
    EXPECT(std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()) == u::builtin_sources_text());
  }
  const std::string k(64, 'C');
  const auto doc = [&](const std::string& catalog, const std::string& keys, const std::string& more = "") {
    return "{\"sources\": 1, \"catalog\": " + catalog + ", \"keys\": " + keys + more + "}";
  };
  EXPECT(u::read_sources(doc("[\"https://a.example.org/c.json\"]", "[\"" + k + "\"]", ", \"unsigned_sums\": [\"https://b.example.org/r/\"]"), false, &s, &why));
  EXPECT(s.catalog.size() == 1 && s.keys.size() == 1 && s.keys[0] == std::string(64, 'c') && s.unsigned_sums[0] == "https://b.example.org/r/");
  const std::string bad[] = {
      "[]", "{\"catalog\": [\"https://a.example.org/c\"], \"keys\": []}", "{\"sources\": 2, \"catalog\": [\"https://a.example.org/c\"], \"keys\": []}",
      doc("[]", "[]"), doc("[\"http://a.example.org/c\"]", "[]"), doc("[\"https://a.example.org/../c\"]", "[]"),
      doc("[\"file:///t/c.json\"]", "[]"), doc("[\"https://a.example.org/c\", \"https://a.example.org/c\"]", "[]"),
      doc("[\"https://a.example.org/c\"]", "[\"" + k.substr(1) + "\"]"), doc("[\"https://a.example.org/c\"]", "\"" + k + "\""),
      doc("[\"https://a.example.org/c\"]", "[]", ", \"unsigned_sums\": [\"http://b.example.org/\"]"),
      doc("[\"https://1.example.org/c\", \"https://2.example.org/c\", \"https://3.example.org/c\", \"https://4.example.org/c\", \"https://5.example.org/c\", "
          "\"https://6.example.org/c\", \"https://7.example.org/c\"]", "[]"),
  };
  for (const std::string& b : bad) {
    EXPECT(!u::read_sources(b, false, &s, &why) && !why.empty());
    std::printf("  %s\n", why.c_str());
  }
  EXPECT(u::read_sources(doc("[\"file:///t/c.json\"]", "[]"), true, &s, &why) && s.catalog[0] == "file:///t/c.json");  // checks only
  // The shipped file, then the built-in copy when it is missing or bad.
  const fs::path dir = fs::path(MW3_TEST_TMP) / "sources";
  std::error_code ec;
  fs::create_directories(dir, ec);
  std::ofstream(dir / "good.json") << doc("[\"https://a.example.org/c.json\"]", "[\"" + k + "\"]");
  std::ofstream(dir / "bad.json") << "{\"sources\": 1";
  s = u::load_sources((dir / "good.json").string(), false, &said);
  EXPECT(s.catalog.size() == 1 && s.catalog[0] == "https://a.example.org/c.json" && said == "sources: " + (dir / "good.json").string());
  s = u::load_sources((dir / "none.json").string(), false, &said);
  EXPECT(s.catalog.size() == 3 && said == "sources: the built-in copy (" + (dir / "none.json").string() + ": it is not there)");
  s = u::load_sources((dir / "bad.json").string(), false, &said);
  EXPECT(s.catalog.size() == 3 && said.rfind("sources: the built-in copy (" + (dir / "bad.json").string() + ": ", 0) == 0);
  std::printf("  %s\n", said.c_str());
  // Keys: the shipped ones, then those learned (each once, 64 hex digits, lower case).
  u::Sources t2;
  t2.catalog = {"https://a.example.org/c.json"}, t2.keys = {std::string(64, 'a')}, t2.unsigned_sums = {"https://gh.example.org/r/"};
  u::use_sources(t2);
  u::learn_keys({std::string(64, 'B'), std::string(64, 'a'), "not a key"});
  const std::vector<std::string> ks = u::signing_keys();
  EXPECT(ks.size() == 2 && ks[0] == std::string(64, 'a') && ks[1] == std::string(64, 'b'));
  EXPECT(u::takes_unsigned_sums("https://gh.example.org/r/mech3-re/releases") && !u::takes_unsigned_sums("https://other.example.org/r/"));
  // Revoked (a signed catalog's revoked_keys): a shipped key and a learned one gone from the keys, the learned one
  // dropped, and neither learned again.
  u::revoke_keys({std::string(64, 'A'), std::string(64, 'b')});
  u::learn_keys({std::string(64, 'b'), std::string(64, 'd')});
  const std::vector<std::string> left = u::signing_keys();
  EXPECT(left.size() == 1 && left[0] == std::string(64, 'd'));
  EXPECT(u::learned_keys() == std::vector<std::string>{std::string(64, 'd')} && u::revoked_keys().size() == 2 && u::revoked_keys()[0] == std::string(64, 'a'));
  u::forget_keys();
  EXPECT(u::signing_keys() == std::vector<std::string>{std::string(64, 'a')});
  u::use_sources(keep);
}
