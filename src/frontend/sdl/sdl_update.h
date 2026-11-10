#pragma once
// Ours: the launcher's CHECK FOR UPDATES, the fetching part (docs/launcher.md, Updates). The system's curl, run as
// a child with no shell, its arguments an array and a time limit: macOS and Linux have it, Windows 10 1803 and later
// ship curl.exe, so no HTTP library is linked and HTTPS is the system's own.
#include <functional>
#include <string>

#include "upgrades/update/update.h"

namespace sdlfe {

// `url`'s body into `out` (HTTPS only; file:// as well when `allow_file`, for checks); false with `error` in plain
// words (curl's own reason, or that curl is missing or timed out).
bool curl_get(const std::string& url, int seconds, std::string* out, std::string* error, bool allow_file = false);

// The releases list from GitHub (MW3_UPDATE_API names another, a file:// JSON in checks) judged against `current`.
upgrades::update::Verdict check_for_updates(const std::string& current);

// `url` into the file `to` (written as `to`.part, renamed once complete), `progress` told the fraction of `size`
// bytes (0 when unknown) and stopping it with false; no overall time limit, but a minute without data ends it.
bool curl_download(const std::string& url, const std::string& to, uint64_t size, const std::function<bool(double)>& progress,
                   std::string* error, bool* cancelled, bool allow_file = false);

// Sitting 3: the release's package for this platform and its SHA256SUMS into `dir`, the package checked against its
// line. `path` the checked file; on any failure nothing is left in `dir` and `words` say why in plain words.
struct Downloaded {
  bool ok = false, cancelled = false;
  std::string path, words;
};
Downloaded download_update(const upgrades::update::Release& r, const std::string& dir, const std::function<bool(double)>& progress);

}  // namespace sdlfe
