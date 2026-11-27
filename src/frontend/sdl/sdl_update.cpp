#include "frontend/sdl/sdl_update.h"
#include "frontend/sdl/launcher_settings.h"
#include "frontend/sdl/sdl_games.h"  // detach_soon

#include "rekit/hash.h"
#include "rekit/sign.h"
#include "upgrades/update/sources.h"

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <SDL3/SDL.h>

#include <cstdio>
#include <cctype>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace fs = std::filesystem;

namespace sdlfe {
namespace {

// A name part no other run of ours picks: the clock's nanoseconds and a count (SDL's ticks are 0 before its init).
std::string unique_part() {
  static std::atomic<int> n{0};  // asked from several threads at once: two once had one name, and each other's answer
  return std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(++n);
}

// The system's curl: Windows' own in System32 (never one a folder on PATH puts first), the usual places elsewhere.
std::string curl_path() {
#if defined(_WIN32)
  if (const char* root = std::getenv("SystemRoot")) {
    const fs::path p = fs::path(root) / "System32" / "curl.exe";
    std::error_code ec;
    if (fs::is_regular_file(p, ec)) return p.string();
  }
  return {};
#else
  for (const char* p : {"/usr/bin/curl", "/bin/curl", "/usr/local/bin/curl"}) {
    std::error_code ec;
    if (fs::is_regular_file(p, ec)) return p;
  }
  return {};
#endif
}

}  // namespace

namespace {
std::atomic<bool> g_stop_asking{false};  // stop_asking(): the launcher closing
}

void stop_asking(bool stop) { g_stop_asking = stop; }

namespace {
#if defined(_WIN32)
const char* const kExitHandle = "mw3.exit_handle";  // start_quiet's own handle on the process
void SDLCALL close_exit_handle(void*, void* h) { CloseHandle(static_cast<HANDLE>(h)); }
#endif
}  // namespace

SDL_Process* start_quiet(SDL_PropertiesID props) {
#if defined(_WIN32)
  SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);  // CREATE_NO_WINDOW
#endif
  SDL_Process* p = SDL_CreateProcessWithProperties(props);
#if defined(_WIN32)
  if (p) {
    const SDL_PropertiesID pp = SDL_GetProcessProperties(p);
    const DWORD pid = static_cast<DWORD>(SDL_GetNumberProperty(pp, SDL_PROP_PROCESS_PID_NUMBER, 0));
    if (HANDLE h = pid ? OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid) : nullptr)
      SDL_SetPointerPropertyWithCleanup(pp, kExitHandle, h, close_exit_handle, nullptr);
  }
#endif
  return p;
}

bool wait_quiet(SDL_Process* p, bool block, int* code) {
  int c = -1;
  if (!SDL_WaitProcess(p, block, &c)) return false;
#if defined(_WIN32)
  DWORD rc = 0;
  const HANDLE h = static_cast<HANDLE>(SDL_GetPointerProperty(SDL_GetProcessProperties(p), kExitHandle, nullptr));
  c = h && GetExitCodeProcess(h, &rc) ? static_cast<int>(rc) : -1;
#endif
  if (code) *code = c;
  return true;
}

bool live_check() {
  const char* v = std::getenv("MW3_LIVE_CHECK");
  return v && std::string(v) == "1";
}

bool curl_get(const std::string& url, int seconds, std::string* out, std::string* error, bool allow_file, bool fresh) {
  out->clear();
  if (g_stop_asking) return *error = "not asked: the launcher is closing", false;
  const std::string curl = curl_path();
  if (curl.empty()) return *error = "this computer has no curl to ask for releases with", false;
  const bool file = url.rfind("file://", 0) == 0;
  if (file && !allow_file) return *error = "not an https address", false;
  // MW3_DOWNLOAD_LOCAL_HTTP (checks): plain http to this computer only, a stand-in server (curl_download's rule).
  const bool local = SDL_getenv("MW3_DOWNLOAD_LOCAL_HTTP") && url.rfind("http://127.0.0.1:", 0) == 0;
  std::error_code ec;
  const fs::path body = fs::temp_directory_path(ec) / ("mw3-update-" + unique_part() + ".json");
  const std::string limit = std::to_string(seconds);
  std::vector<std::string> args = {curl, "--silent", "--show-error", "--fail", "--location", "--max-time", limit,
                                   "--proto", file ? "=file" : local ? "=http" : "=https", "--proto-redir", "=https",
                                   "--header", "Accept: application/vnd.github+json", "--user-agent", user_agent()};
  if (fresh) args.insert(args.end(), {"--header", "Cache-Control: no-cache", "--header", "Pragma: no-cache"});
  args.insert(args.end(), {"--output", body.string(), url});
  if (const char* to = std::getenv("MW3_CURL_ARGS_TO"))  // checks: the arguments written, a line each
    if (std::ofstream f(to, std::ios::app); f)
      for (const std::string& a : args) f << a << "\n";
  std::vector<const char*> argv;
  for (const std::string& a : args) argv.push_back(a.c_str());
  argv.push_back(nullptr);
  const SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
  SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);  // curl's reason
  SDL_Process* p = start_quiet(props);
  SDL_DestroyProperties(props);
  if (!p) return *error = std::string("curl could not be started (") + SDL_GetError() + ")", false;
  // Its few lines read as they come (a full pipe would stall it), until it ends or our own limit passes.
  std::string said;
  int code = -1;
  bool ended = false;
  const Uint64 until = SDL_GetTicks() + static_cast<Uint64>(seconds + 5) * 1000;
  SDL_IOStream* io = SDL_GetProcessOutput(p);
  while (!(ended = wait_quiet(p, false, &code)) && SDL_GetTicks() < until && !g_stop_asking) {
    char buf[256];
    if (io) {
      const size_t n = SDL_ReadIO(io, buf, sizeof buf);
      said.append(buf, n);
      if (n == 0) SDL_Delay(20);
    } else {
      SDL_Delay(20);
    }
  }
  if (!ended) {  // past our limit, or the launcher closing: curl stopped, never waited out
    SDL_KillProcess(p, true);
    wait_quiet(p, true, &code);
    SDL_DestroyProcess(p);
    fs::remove(body, ec);
    return *error = g_stop_asking ? "stopped: the launcher is closing" : "the server did not answer in time", false;
  }
  if (io)
    for (char buf[256]; const size_t n = SDL_ReadIO(io, buf, sizeof buf);) said.append(buf, n);
  SDL_DestroyProcess(p);
  {
    std::ifstream f(body, std::ios::binary);
    out->assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  }
  fs::remove(body, ec);
  if (code != 0) {
    while (!said.empty() && (said.back() == '\n' || said.back() == '\r')) said.pop_back();
    if (said.rfind("curl: ", 0) == 0) said.erase(0, 6);
    if (const size_t paren = said.find(") "); said.rfind("(", 0) == 0 && paren != std::string::npos) said.erase(0, paren + 2);
    out->clear();
    return *error = said.empty() ? "curl stopped (" + std::to_string(code) + ")" : said, false;
  }
  return true;
}

bool curl_download(const std::string& url, const std::string& to, uint64_t size, const DownloadFn& progress,
                   std::string* error, bool* cancelled, bool allow_file) {
  *cancelled = false;
  const std::string curl = curl_path();
  if (curl.empty()) return *error = "this computer has no curl to download with", false;
  const bool file = url.rfind("file://", 0) == 0;
  if (file && !allow_file) return *error = "not an https address", false;
  // MW3_DOWNLOAD_LOCAL_HTTP (checks): plain http to this computer only, a stand-in server that drops the connection.
  const bool local = SDL_getenv("MW3_DOWNLOAD_LOCAL_HTTP") && url.rfind("http://127.0.0.1:", 0) == 0;
  std::error_code ec;
  const std::string part = to + ".part";
  fs::remove(part, ec);
  const auto got_bytes = [&]() { return fs::exists(part, ec) ? static_cast<uint64_t>(fs::file_size(part, ec)) : uint64_t{0}; };
  const auto stop = [&]() {  // a cancel, or the launcher closing
    return g_stop_asking || (progress && !progress(size ? std::min(1.0, static_cast<double>(got_bytes()) / static_cast<double>(size)) : 0.0, got_bytes(), false));
  };
  // One curl run, from the part's end when `resume` (curl's --continue-at -): its exit code, -2 for a cancel.
  const auto attempt = [&](bool resume, std::string* said) {
    std::vector<std::string> args = {curl, "--silent", "--show-error", "--fail", "--location", "--connect-timeout",
                                     std::to_string(settings::whole(settings::kConnectSeconds)), "--speed-limit",
                                     std::to_string(settings::whole(settings::kSlowBytesPerSecond)), "--speed-time",
                                     std::to_string(settings::whole(settings::kSlowSeconds)), "--proto", file ? "=file" : local ? "=http" : "=https",
                                     "--proto-redir", "=https", "--user-agent", "MW3-Rebuild-launcher", "--output", part};
    if (resume) args.push_back("--continue-at"), args.push_back("-");
    args.push_back(url);
    std::vector<const char*> argv;
    for (const std::string& a : args) argv.push_back(a.c_str());
    argv.push_back(nullptr);
    const SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
    SDL_Process* p = start_quiet(props);
    SDL_DestroyProperties(props);
    if (!p) return *said = std::string("curl could not be started (") + SDL_GetError() + ")", -1;
    int code = -1;
    SDL_IOStream* io = SDL_GetProcessOutput(p);
    for (;;) {  // the bar (and a cancel) asked before each wait, so even a quick copy can be stopped
      if (stop()) {
        SDL_KillProcess(p, true);
        wait_quiet(p, true, &code);
        SDL_DestroyProcess(p);
        return -2;
      }
      if (wait_quiet(p, false, &code)) break;
      char buf[256];
      const size_t n = io ? SDL_ReadIO(io, buf, sizeof buf) : 0;
      said->append(buf, n);
      if (n == 0) SDL_Delay(50);
    }
    if (io)
      for (char buf[256]; const size_t n = SDL_ReadIO(io, buf, sizeof buf);) said->append(buf, n);
    SDL_DestroyProcess(p);
    return code;
  };
  // A dropped connection (2026-11-27): tried again after 5 s from where the part ends, while each try gets further
  // (at most 5, the first retry always); a server that will not resume (curl 33, or HTTP 416) starts it over once.
  // Whatever arrives is still checked whole by the caller (size, SHA-256), so a resume never installs a bad file.
  std::string said;
  int code = -1;
  bool resume = false, restarted = false;
  uint64_t before = 0;
  for (int tries = 1;; ++tries) {
    said.clear();
    code = attempt(resume, &said);
    if (code == -2) return fs::remove(part, ec), *cancelled = true, *error = "canceled", false;
    const bool came_short = code == 0 && size > 0 && got_bytes() < size;  // whatever curl said: as its partial file
    if (code == 0 && !came_short) break;
    if (code == -1 || file) break;
    if (came_short) code = 18;
    const bool no_resume = code == 33 || (code == 22 && said.find("416") != std::string::npos);
    const bool dropped = code == 6 || code == 7 || code == 18 || code == 28 || code == 35 || code == 52 || code == 55 || code == 56;
    if (resume && no_resume && !restarted) {  // the server will not continue it: once more from the start
      restarted = true, resume = false, fs::remove(part, ec);
      std::printf("mw3: launcher: download: %s will not resume; starting it over\n", url.c_str());
      continue;
    }
    const uint64_t now = got_bytes();
    if (!dropped || tries >= 5 || (tries > 1 && now <= before)) {
      if (came_short) code = 0;  // kept: the caller says how short (its size check)
      break;
    }
    before = now, resume = now > 0;
    std::printf("mw3: launcher: download: %s stopped at %llu bytes (curl %d); trying again in 5 s\n", url.c_str(),
                static_cast<unsigned long long>(now), code);
    for (int w = 0; w < 50; ++w) {  // 5 s, a cancel still heard
      if (stop()) return fs::remove(part, ec), *cancelled = true, *error = "canceled", false;
      SDL_Delay(100);
    }
  }
  if (code != 0) {
    fs::remove(part, ec);
    while (!said.empty() && (said.back() == '\n' || said.back() == '\r')) said.pop_back();
    if (said.rfind("curl: ", 0) == 0) said.erase(0, 6);
    if (const size_t paren = said.find(") "); said.rfind("(", 0) == 0 && paren != std::string::npos) said.erase(0, paren + 2);
    return *error = said.empty() ? "curl stopped (" + std::to_string(code) + ")" : said, false;
  }
  fs::remove(to, ec);
  fs::rename(part, to, ec);
  if (ec) {
    fs::remove(part, ec);
    return *error = "the download could not be kept: " + ec.message(), false;
  }
  return true;
}

namespace {
// An address's host for the log: "github.com" from "https://github.com/...", "file" for a fixture.
std::string host_of(const std::string& url) {
  if (url.rfind("file://", 0) == 0) return "file";
  const size_t at = url.find("://");
  if (at == std::string::npos) return url;
  const size_t end = url.find('/', at + 3);
  return url.substr(at + 3, end == std::string::npos ? std::string::npos : end - at - 3);
}
}  // namespace

Fetched download_checked(const upgrades::update::Release& r, const std::string& name, const std::string& path,
                         const DownloadFn& progress, bool allow_file, const char* log) {
  namespace u = upgrades::update;
  std::vector<const std::vector<u::Asset>*> homes = {&r.assets};
  std::vector<bool> unsigned_ok = {r.unsigned_sums};
  for (size_t i = 0; i < r.other_homes.size(); ++i)
    homes.push_back(&r.other_homes[i]), unsigned_ok.push_back(i < r.other_unsigned_sums.size() && r.other_unsigned_sums[i]);
  const auto find = [](const std::vector<u::Asset>& assets, const std::string& n) -> const u::Asset* {
    for (const u::Asset& a : assets)
      if (a.name == n && !a.url.empty()) return &a;
    return nullptr;
  };
  // Each home's SHA256SUMS line for `name`, asked once and only when needed (why: what stopped it).
  struct Line {
    bool asked = false;
    Fetched::Why why = Fetched::kOk;
    std::string line, error;
  };
  std::vector<Line> lines(homes.size());
  const auto line_of = [&](size_t k) -> Line& {
    Line& l = lines[k];
    if (l.asked) return l;
    l.asked = true;
    const u::Asset* sums = find(*homes[k], "SHA256SUMS");
    std::string list;
    if (!sums) l.why = Fetched::kNoSums;
    else if (!curl_get(sums->url, sdlfe::settings::whole(sdlfe::settings::kSumsSeconds), &list, &l.error, allow_file)) l.why = Fetched::kSumsFailed;
    else if ((l.line = u::sums_line(list, name)).empty()) l.why = Fetched::kNoLine;
    if (!l.line.empty()) {  // signed (SHA256SUMS.sig, any home), or unsigned from a home sources.json allows that of
      const u::Asset* sig = find(*homes[k], "SHA256SUMS.sig");
      std::string sig_text, sig_error;
      const bool signed_ok = sig && curl_get(sig->url, sdlfe::settings::whole(sdlfe::settings::kSumsSigSeconds), &sig_text, &sig_error, allow_file) && !rekit::signed_by(list, sig_text, u::signing_keys()).empty();
      if (!signed_ok && !unsigned_ok[k]) {
        l.why = Fetched::kUnsigned, l.line.clear();
        l.error = sig ? "its SHA256SUMS.sig does not check against the launcher's keys" : "its SHA256SUMS is not signed";
        std::printf("mw3: launcher: %s: %s from %s: %s\n", log, name.c_str(), host_of(sums->url).c_str(), l.error.c_str());
      } else if (!signed_ok) {
        std::printf("mw3: launcher: %s: %s's SHA256SUMS from %s: unsigned, taken (a home sources.json allows that of)\n", log, name.c_str(), host_of(sums->url).c_str());
      }
    }
    return l;
  };
  std::error_code ec;
  Fetched first;
  bool failed_once = false;
  const auto fail = [&](const Fetched& f, size_t k, const std::string& what) {
    std::printf("mw3: launcher: %s: %s from %s: %s%s\n", log, name.c_str(), host_of(find(*homes[k], name)->url).c_str(), what.c_str(),
                k + 1 < homes.size() ? "; trying the next home" : "");
    if (!failed_once) first = f, failed_once = true;
  };
  for (size_t k = 0; k < homes.size(); ++k) {
    const u::Asset* file = find(*homes[k], name);
    if (!file) continue;
    // This home's line, else the others' when every one that has one agrees.
    Fetched f;
    std::string want = line_of(k).line;
    if (want.empty()) {
      f.why = lines[k].why, f.error = lines[k].error;
      std::string agreed;
      bool clash = false;
      for (size_t j = 0; j < homes.size(); ++j) {
        if (j == k) continue;
        const std::string& o = line_of(j).line;
        if (o.empty()) continue;
        if (!agreed.empty() && o != agreed) clash = true;
        agreed = o;
      }
      if (agreed.empty() || clash) {
        fail(f, k, clash ? "the other homes' SHA256SUMS disagree" : "no SHA256SUMS line for it");
        continue;
      }
      want = agreed;
    }
    std::string error;
    bool cancelled = false;
    if (!curl_download(file->url, path, file->size, progress, &error, &cancelled, allow_file)) {
      if (cancelled) {
        Fetched c;
        c.why = Fetched::kCancelled;
        return c;
      }
      f.why = Fetched::kStopped, f.error = error;
      fail(f, k, "the download stopped: " + error);
      continue;
    }
    if (progress && !progress(1.0, file->size, true)) {  // its bytes in: checked next (a cancel here keeps nothing)
      fs::remove(path, ec);
      Fetched c;
      c.why = Fetched::kCancelled;
      return c;
    }
    const std::string got = rekit::sha256_file_hex(path);
    if (got != want) {
      fs::remove(path, ec);
      f.why = Fetched::kMismatch, f.want = want, f.got = got;
      fail(f, k, "it is " + got + ", SHA256SUMS says " + want);
      continue;
    }
    if (k > 0) std::printf("mw3: launcher: %s: %s downloaded from %s\n", log, name.c_str(), host_of(file->url).c_str());
    Fetched ok;
    ok.why = Fetched::kOk, ok.sha256 = got, ok.github = k == 0 && r.first_home;
    return ok;
  }
  fs::remove(path, ec);
  return first;
}

Downloaded download_update(const upgrades::update::Release& r, const std::string& dir, const DownloadFn& progress) {
  namespace u = upgrades::update;
  Downloaded d;
  const bool allow_file = std::getenv("MW3_UPDATE_API") != nullptr;  // checks: a fixture's file:// assets
  const u::Platform platform = u::this_platform();
  const u::Asset* package = u::platform_asset(r, platform);
  if (!package) return d.words = std::string("This release has no ") + u::platform_package_words(platform) + " to download.", d;
  std::error_code ec;
  fs::create_directories(dir, ec);
  const std::string path = (fs::path(dir) / package->name).string();
  const Fetched f = download_checked(r, package->name, path, progress, allow_file, "update download");
  switch (f.why) {
    case Fetched::kOk:
      d.ok = true, d.path = path, d.github = f.github;
      d.words = "Downloaded and checked: " + package->name + ".";
      break;
    case Fetched::kCancelled: d.cancelled = true, d.words = "The download was canceled. Nothing was installed."; break;
    case Fetched::kNoSums: d.words = "This release has no SHA256SUMS to check the download against, so nothing was downloaded."; break;
    case Fetched::kSumsFailed: d.words = "Could not download the release's SHA256SUMS: " + f.error + "."; break;
    case Fetched::kNoLine: d.words = "The release's SHA256SUMS has no line for " + package->name + ", so nothing was downloaded."; break;
    case Fetched::kStopped: d.words = "The download stopped: " + f.error + ". Nothing was installed."; break;
    case Fetched::kMismatch:
      d.words = "The download did not match its checksum, so nothing was installed. Try again; if it happens again, download it from the release's page.";
      break;
    case Fetched::kUnsigned: d.words = "The release's SHA256SUMS could not be trusted (" + f.error + "), so nothing was downloaded."; break;
  }
  return d;
}

namespace {
// A tool run to its end with its arguments as an array (hdiutil): its exit code, its output in `said`.
int run_tool(const std::vector<std::string>& args, int seconds, std::string* said, Ran* ran = nullptr, bool quits = false) {
  std::vector<const char*> argv;
  for (const std::string& a : args) argv.push_back(a.c_str());
  argv.push_back(nullptr);
  const SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data());
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
  SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
  SDL_Process* p = start_quiet(props);
  SDL_DestroyProperties(props);
  if (ran) *ran = p ? Ran::kEnded : Ran::kNotStarted;
  if (!p) return -1;
  int code = -1;
  bool ended = false;
  SDL_IOStream* io = SDL_GetProcessOutput(p);
  const Uint64 until = SDL_GetTicks() + static_cast<Uint64>(seconds) * 1000;
  while (!(ended = wait_quiet(p, false, &code)) && SDL_GetTicks() < until && !(quits && g_stop_asking)) {
    char buf[256];
    const size_t n = io ? SDL_ReadIO(io, buf, sizeof buf) : 0;
    said->append(buf, n);
    if (n == 0) SDL_Delay(20);
  }
  if (!ended) SDL_KillProcess(p, true), wait_quiet(p, true, &code), code = -1;
  if (!ended && ran) *ran = Ran::kTimedOut;
  if (io)
    for (char buf[256]; const size_t n = SDL_ReadIO(io, buf, sizeof buf);) said->append(buf, n);
  SDL_DestroyProcess(p);
  return code;
}

// Our running program's package: the Mac app holding this executable, Linux's $APPIMAGE; empty when not one.
std::string running_package() {
  if (const char* t = std::getenv("MW3_UPDATE_TARGET")) return t;  // checks
#if defined(__APPLE__)
  char buf[4096];
  uint32_t n = sizeof buf;
  if (_NSGetExecutablePath(buf, &n) != 0) return {};
  std::error_code ec;
  const fs::path exe = fs::weakly_canonical(fs::path(buf), ec);
  const fs::path app = exe.parent_path().parent_path().parent_path();  // X.app/Contents/MacOS/mw3launcher
  return app.extension() == ".app" ? app.string() : std::string();
#elif defined(_WIN32)
  return {};
#else
  const char* a = std::getenv("APPIMAGE");
  return a ? a : std::string();
#endif
}

bool is_our_app(const fs::path& app) {
  std::error_code ec;
  return app.extension() == ".app" && fs::is_regular_file(app / "Contents" / "MacOS" / "mw3launcher", ec);
}
}  // namespace

int run_program(const std::vector<std::string>& args, int seconds, std::string* said, Ran* ran, bool quits) {
  return run_tool(args, seconds, said, ran, quits);
}

bool start_detached(const std::string& program) {
  const char* args[] = {program.c_str(), nullptr};
  const SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, const_cast<char**>(args));
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_NULL);
  SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_NULL);
  SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
  SDL_Process* p = SDL_CreateProcessWithProperties(props);
  SDL_DestroyProperties(props);
  if (!p) return false;
  SDL_DestroyProcess(p);
  return true;
}

Installed install_update(const std::string& download) {
  Installed in;
  std::error_code ec;
  if (!fs::is_regular_file(download, ec)) return in.words = "The downloaded file is gone: nothing was installed.", in;
#if defined(_WIN32)
  // The new Setup, as a player would run it (Inno Setup, the same AppId: an upgrade in place); the launcher quits.
  if (!start_detached(download)) return in.words = "The new Setup could not be started: run it yourself from " + download + ".", in;
  in.ok = true;
  in.words = "The new Setup is running: follow it, and the BattleTech launcher opens again at its end (its last page's Launch box).";
  return in;
#else
  const std::string target = running_package();
#if defined(__APPLE__)
  if (target.empty() || !is_our_app(target))
    return in.words = "This copy is not running from the MW3 Rebuild app, so it cannot replace itself: open the downloaded " +
                      fs::path(download).filename().string() + " and drag the app over the old one.", in;
  // An app opened where it was downloaded runs from a read-only copy macOS makes (App Translocation).
  if (target.find("/AppTranslocation/") != std::string::npos)
    return in.words = "This copy runs from a read-only copy macOS made of the download, so it cannot replace itself: drag the app into Applications first, or open the downloaded " +
                      fs::path(download).filename().string() + " and drag the app into Applications.", in;
  const fs::path mnt = fs::temp_directory_path(ec) / ("MW3-Rebuild-update-mount-" + unique_part());
  fs::create_directories(mnt, ec);
  std::string said;
  if (run_tool({"/usr/bin/hdiutil", "attach", "-nobrowse", "-readonly", "-noautoopen", "-mountpoint", mnt.string(), download}, sdlfe::settings::whole(sdlfe::settings::kDmgMountSeconds), &said) != 0) {
    fs::remove(mnt, ec);
    return in.words = "The downloaded disk image could not be opened, so nothing was installed.", in;
  }
  fs::path fresh;
  for (const auto& e : fs::directory_iterator(mnt, ec))
    if (is_our_app(e.path())) fresh = e.path();
  std::string error;
  const bool swapped = !fresh.empty() && upgrades::update::swap_in(fresh.string(), target, &error);
  sdlfe::games::detach_soon(mnt.string());  // swapped in: the restart does not wait for the detach (2.0.1)
  if (fresh.empty()) return in.words = "The downloaded disk image holds no MW3 Rebuild app, so nothing was installed.", in;
  if (!swapped) return in.words = "The new app could not be put in place: " + error + ".", in;
  in.restart = (fs::path(target) / "Contents" / "MacOS" / "mw3launcher").string();
#else
  if (target.empty() || fs::path(target).extension() != ".AppImage" || !fs::is_regular_file(target, ec))
    return in.words = "This copy is not running as an AppImage, so it cannot replace itself: use the downloaded " + download + " instead.", in;
  fs::permissions(download, fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec, fs::perm_options::add, ec);
  std::string error;
  if (!upgrades::update::swap_in(download, target, &error)) return in.words = "The new AppImage could not be put in place: " + error + ".", in;
  in.restart = target;
#endif
  fs::remove(download, ec);
  in.ok = true;
  in.words = "Installed. The BattleTech launcher starts again now.";
  return in;
#endif
}

std::vector<ReleaseList> release_lists(const char* env, const std::string& repo, int github_per_page) {
  std::vector<ReleaseList> out;
  const auto with_repo = [&](std::string url) {
    if (const size_t at = url.find("{repo}"); at != std::string::npos) url.replace(at, 6, repo);
    return url;
  };
  if (env) {
    const std::string s = env;
    const size_t bar = s.find('|');
    // Checks: one list, taken as a trusted first home; "UNTRUSTED|TRUSTED", the second downloaded from first.
    if (bar == std::string::npos) return {{with_repo(s), true, true}};
    return {{with_repo(s.substr(0, bar)), false, false}, {with_repo(s.substr(bar + 1)), true, true}};
  }
  return home_lists(upgrades::update::active_homes(), repo, github_per_page);
}

std::vector<ReleaseList> home_lists(const std::vector<upgrades::update::CatalogHome>& homes, const std::string& repo, int count) {
  std::vector<ReleaseList> out;
  for (const upgrades::update::CatalogHome& h : homes) {
    std::string url = h.releases;
    for (const auto& [mark, value] : {std::pair<std::string, std::string>{"{repo}", repo}, {"{count}", std::to_string(count)}})
      for (size_t at; (at = url.find(mark)) != std::string::npos;) url.replace(at, mark.size(), value);
    const bool first = out.empty();
    out.push_back({url, first, upgrades::update::takes_unsigned_sums(url)});
  }
  return out;
}

std::vector<upgrades::update::Answer> ask_lists(const std::vector<ReleaseList>& lists, int seconds, bool allow_file, const std::string& log) {
  std::vector<upgrades::update::Answer> answers;
  for (const ReleaseList& l : lists) {
    upgrades::update::Answer a;
    a.download_first = l.download_first, a.unsigned_sums = l.unsigned_sums;
    curl_get(l.url, seconds, &a.json, &a.fetch_error, allow_file);
    std::printf("mw3: launcher: %s at %s: %s\n", log.c_str(), l.url.c_str(), a.json.empty() ? a.fetch_error.c_str() : "answered");
    answers.push_back(std::move(a));
  }
  return answers;
}

// Both repos of the package (2026-11-21): the hub mw3-re, where every copy up to 1.2.1 looks, and mech3-re,
// MechWarrior 3's own repo after the split; each in every home (the catalog's homes, then GitHub; 2026-11-26);
// the newest package on any of them wins. MW3_UPDATE_API: one list or two (release_lists), for checks.
upgrades::update::Verdict check_for_updates(const std::string& current) {
  const char* api = std::getenv("MW3_UPDATE_API");
  std::vector<upgrades::update::Answer> answers;
  if (api) {
    answers = ask_lists(release_lists(api, "", sdlfe::settings::whole(sdlfe::settings::kUpdateCount)), sdlfe::settings::whole(sdlfe::settings::kUpdateSeconds), true, "update check");
  } else {
    for (const char* repo : {"mw3-re", "mech3-re"})
      for (upgrades::update::Answer& a : ask_lists(release_lists(nullptr, repo, sdlfe::settings::whole(sdlfe::settings::kUpdateCount)), sdlfe::settings::whole(sdlfe::settings::kUpdateSeconds), false, "update check")) answers.push_back(std::move(a));
  }
  return upgrades::update::judge(current, answers);
}

#ifndef MW3_VERSION
#define MW3_VERSION ""
#endif
std::string own_version() { return upgrades::update::plain_version(MW3_VERSION); }
std::string user_agent() { return own_version().empty() ? std::string("MW3-Rebuild-launcher") : "MW3-Rebuild-launcher/" + own_version(); }

namespace {
// A value as the site takes it, [A-Za-z0-9._-]{1,40} (anything else is counted as rejected): another letter becomes _.
std::string query_part(const std::string& s) {
  std::string out;
  for (unsigned char c : s.substr(0, 40)) out += std::isalnum(c) || c == '-' || c == '.' || c == '_' ? static_cast<char>(c) : '_';
  return out.empty() ? std::string("unknown") : out;
}
}  // namespace

std::string count_url(const std::string& base, const std::string& kind, const std::string& game, const std::string& from,
                      const std::string& to, bool github) {
  return base + (base.find('?') == std::string::npos ? "?" : "&") + "kind=" + query_part(kind) + "&game=" + query_part(game) +
         "&from=" + query_part(from.empty() ? "unknown" : from) + "&to=" + query_part(to) + "&launcher=" + query_part(own_version()) +
         "&src=" + (github ? "github" : "mirror");
}

void send_count(const std::string& url) {
  std::string body, error;
  if (curl_get(url, 3, &body, &error, false)) std::printf("mw3: launcher: counts: sent %s\n", url.c_str());
  else std::printf("mw3: launcher: counts: not sent (%s): %s\n", error.c_str(), url.c_str());
}

void write_update_note(const std::string& dir, const std::string& from, const std::string& to, bool github) {
  if (dir.empty()) return;
  std::ofstream f(fs::path(dir) / kUpdateNote, std::ios::binary | std::ios::trunc);
  f << "# The launcher's own update, reported once by the new launcher (docs/launcher.md, Update counts)\nfrom=" << from << "\nto=" << to
    << "\nsrc=" << (github ? "github" : "mirror") << "\n";
}

bool take_update_note(const std::string& dir, std::string* from, std::string* to, bool* github) {
  if (dir.empty()) return false;
  const fs::path p = fs::path(dir) / kUpdateNote;
  std::ifstream f(p, std::ios::binary);
  if (!f) return false;
  from->clear(), to->clear(), *github = false;
  for (std::string line; std::getline(f, line);) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.rfind("from=", 0) == 0) *from = line.substr(5);
    else if (line.rfind("to=", 0) == 0) *to = line.substr(3);
    else if (line == "src=github") *github = true;
  }
  f.close();
  std::error_code ec;
  fs::remove(p, ec);  // taken: never reported twice, whatever comes of it
  return !to->empty();
}

}  // namespace sdlfe
