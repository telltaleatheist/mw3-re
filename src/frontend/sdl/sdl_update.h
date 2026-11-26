#pragma once
// Ours: the launcher's CHECK FOR UPDATES, the fetching part (docs/launcher.md, Updates). The system's curl, run as
// a child with no shell, its arguments an array and a time limit: macOS and Linux have it, Windows 10 1803 and later
// ship curl.exe, so no HTTP library is linked and HTTPS is the system's own.
#include <SDL3/SDL_process.h>

#include <functional>
#include <string>
#include <vector>

#include "upgrades/update/update.h"

namespace sdlfe {

// `url`'s body into `out` (HTTPS only; file:// as well when `allow_file`, for checks); false with `error` in plain
// words (curl's own reason, or that curl is missing or timed out).
bool curl_get(const std::string& url, int seconds, std::string* out, std::string* error, bool allow_file = false);

// The launcher closing (lead 2026-11-23: a quit never waits on the network): every curl_get or curl_download running
// is stopped at once (curl killed, the call failing with "the launcher is closing") and any later one fails at once,
// so the checks' threads end and are joined in moments. `false` asks again: each Launcher's start (a process may
// show the launcher's screens more than once, mw3's setup screen after another).
void stop_asking(bool stop = true);

// A helper program (curl, a game's --status) started with no console window (2026-11-23: consoles flashing at
// the launcher's start look like an attack; none may show, not even for a moment). On Windows that is SDL's
// background flag (CREATE_NO_WINDOW), which reports every exit as 0, so wait_quiet reads the real code from our own
// handle on the process (SDL's open handle keeps its id from being reused). Elsewhere, SDL's own calls.
SDL_Process* start_quiet(SDL_PropertiesID props);
bool wait_quiet(SDL_Process* p, bool block, int* code);

// A repo's releases lists in both homes, in the order they are asked (2026-11-22): git.owenmorgan.com's
// (Forgejo, `limit`) then GitHub's (`per_page`, download_first: a download comes from it first). `env` replaces them
// in checks (MW3_UPDATE_API, MW3_GAMES_API; {repo} in it becomes `repo`): one list, taken as GitHub's, or two
// as "BACKUP|GITHUB".
struct ReleaseList {
  std::string url;
  bool download_first = false;
};
std::vector<ReleaseList> release_lists(const char* env, const std::string& repo, int github_per_page);
// Each list asked in turn (`seconds` each), its answer logged as "mw3: launcher: <log> at <url>: answered".
std::vector<upgrades::update::Answer> ask_lists(const std::vector<ReleaseList>& lists, int seconds, bool allow_file, const std::string& log);

// The package's releases lists (both repos, both homes; MW3_UPDATE_API names others, file:// JSON in checks)
// judged against `current`.
upgrades::update::Verdict check_for_updates(const std::string& current);

// `url` into the file `to` (written as `to`.part, renamed once complete), `progress` told the fraction of `size`
// bytes (0 when unknown) and stopping it with false; no overall time limit, but a minute without data ends it.
bool curl_download(const std::string& url, const std::string& to, uint64_t size, const std::function<bool(double)>& progress,
                   std::string* error, bool* cancelled, bool allow_file = false);

// A release's file `name` into `path`, from its homes in order (2026-11-22): `r.assets` (GitHub's when it has
// it), then each of `r.other_homes` (git.owenmorgan.com) only when the one before failed, so the other home carries
// only what GitHub cannot. Each home's file is checked against that home's SHA256SUMS line, or, when that home has
// none (no SHA256SUMS, not fetched, no line), against the other homes' when they agree. Nothing is left at `path`
// unless `ok`. `why` is the first home's failure (the one a player would have met with one home); `error` its
// reason (curl's words), `want`/`got` a mismatch's sums. `log` heads the log's lines ("update", "games").
struct Fetched {
  enum Why { kOk, kCancelled, kNoSums, kSumsFailed, kNoLine, kStopped, kMismatch } why = kStopped;
  std::string sha256, error, want, got;
};
Fetched download_checked(const upgrades::update::Release& r, const std::string& name, const std::string& path,
                         const std::function<bool(double)>& progress, bool allow_file, const char* log);

// Sitting 3: the release's package for this platform and its SHA256SUMS into `dir`, the package checked against its
// line (download_checked: GitHub first, git.owenmorgan.com when it fails). `path` the checked file; on any failure
// nothing is left in `dir` and `words` say why in plain words.
struct Downloaded {
  bool ok = false, cancelled = false;
  std::string path, words;
};
Downloaded download_update(const upgrades::update::Release& r, const std::string& dir, const std::function<bool(double)>& progress);

// Sitting 4: the checked download installed the platform's normal way. Windows: the new Setup started (it upgrades
// in place; the launcher then quits). The Mac: the DMG attached read-only, its app swapped in for the running one
// (upgrades::update::swap_in), the DMG detached. Linux: the AppImage swapped in for $APPIMAGE. Only a target that is
// our program is replaced (an .app with Contents/MacOS/mw3launcher; a file *.AppImage); MW3_UPDATE_TARGET names it
// in checks. `restart` what to start next (empty: nothing). Never a settings folder.
struct Installed {
  bool ok = false;
  std::string words, restart;
};
Installed install_update(const std::string& download);
// The new launcher started on its own (no stdio of ours); false when it could not be.
bool start_detached(const std::string& program);
// A program run to its end, its arguments as an array: its exit code (-1 not
// started or past `seconds`), its output and errors in `said`.
int run_program(const std::vector<std::string>& args, int seconds, std::string* said);

}  // namespace sdlfe
