// The updater's logic (src/upgrades/update; docs/launcher.md, Updates, sitting 1): GitHub's releases list read, the
// newest package release picked, versions compared, the check's words.
#include <string>
#include <vector>

#include "check.h"
#include "upgrades/update/update.h"

namespace u = upgrades::update;

namespace {
// A releases list as GitHub gives it (the fields the updater reads, among others it skips).
std::string release(const std::string& tag, bool draft = false, bool pre = false, const std::string& body = "Fixes.") {
  return "{\"url\":\"https://api.github.com/x\",\"id\":1,\"tag_name\":\"" + tag + "\",\"draft\":" + (draft ? "true" : "false") +
         ",\"prerelease\":" + (pre ? "true" : "false") + ",\"body\":\"" + body + "\",\"author\":{\"login\":\"a\",\"id\":2},\"assets\":[" +
         "{\"name\":\"MW3-Rebuild-" + tag + "-mac.dmg\",\"size\":10,\"browser_download_url\":\"https://github.com/r/" + tag + "/mac.dmg\"}," +
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
  EXPECT(all[0].assets.size() == 2 && all[0].assets[0].name == "MW3-Rebuild-v1.1.0-mac.dmg" &&
         all[0].assets[1].url == "https://github.com/r/v1.1.0/SHA256SUMS");
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
  const u::Release* newest = u::newest_package(all);
  EXPECT(newest && newest->tag == "v1.10.0");  // not the draft, the prerelease, the rc or PM's; 1.10 above 1.9
  EXPECT(!u::newest_package({}));
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
