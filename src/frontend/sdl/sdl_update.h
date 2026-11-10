#pragma once
// Ours: the launcher's CHECK FOR UPDATES, the fetching part (docs/launcher.md, Updates). The system's curl, run as
// a child with no shell, its arguments an array and a time limit: macOS and Linux have it, Windows 10 1803 and later
// ship curl.exe, so no HTTP library is linked and HTTPS is the system's own.
#include <string>

#include "upgrades/update/update.h"

namespace sdlfe {

// `url`'s body into `out` (HTTPS only; file:// as well when `allow_file`, for checks); false with `error` in plain
// words (curl's own reason, or that curl is missing or timed out).
bool curl_get(const std::string& url, int seconds, std::string* out, std::string* error, bool allow_file = false);

// The releases list from GitHub (MW3_UPDATE_API names another, a file:// JSON in checks) judged against `current`.
upgrades::update::Verdict check_for_updates(const std::string& current);

}  // namespace sdlfe
