#include "frontend/sdl/sdl_update.h"

#include "rekit/hash.h"

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

bool curl_get(const std::string& url, int seconds, std::string* out, std::string* error, bool allow_file) {
  out->clear();
  if (g_stop_asking) return *error = "not asked: the launcher is closing", false;
  const std::string curl = curl_path();
  if (curl.empty()) return *error = "this computer has no curl to ask for releases with", false;
  const bool file = url.rfind("file://", 0) == 0;
  if (file && !allow_file) return *error = "not an https address", false;
  std::error_code ec;
  const fs::path body = fs::temp_directory_path(ec) / ("mw3-update-" + unique_part() + ".json");
  const std::string limit = std::to_string(seconds);
  const std::vector<std::string> args = {curl, "--silent", "--show-error", "--fail", "--location", "--max-time", limit,
                                         "--proto", file ? "=file" : "=https", "--proto-redir", "=https",
                                         "--header", "Accept: application/vnd.github+json", "--user-agent", "MW3-Rebuild-launcher",
                                         "--output", body.string(), url};
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

bool curl_download(const std::string& url, const std::string& to, uint64_t size, const std::function<bool(double)>& progress,
                   std::string* error, bool* cancelled, bool allow_file) {
  *cancelled = false;
  const std::string curl = curl_path();
  if (curl.empty()) return *error = "this computer has no curl to download with", false;
  const bool file = url.rfind("file://", 0) == 0;
  if (file && !allow_file) return *error = "not an https address", false;
  std::error_code ec;
  const std::string part = to + ".part";
  fs::remove(part, ec);
  const std::vector<std::string> args = {curl, "--silent", "--show-error", "--fail", "--location", "--connect-timeout", "20",
                                         "--speed-limit", "1024", "--speed-time", "60", "--proto", file ? "=file" : "=https",
                                         "--proto-redir", "=https", "--user-agent", "MW3-Rebuild-launcher", "--output", part, url};
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
  if (!p) return *error = std::string("curl could not be started (") + SDL_GetError() + ")", false;
  std::string said;
  int code = -1;
  SDL_IOStream* io = SDL_GetProcessOutput(p);
  for (;;) {  // the bar (and a cancel) asked before each wait, so even a quick copy can be stopped
    const uint64_t got = fs::exists(part, ec) ? static_cast<uint64_t>(fs::file_size(part, ec)) : 0;
    if (g_stop_asking || (progress && !progress(size ? std::min(1.0, static_cast<double>(got) / static_cast<double>(size)) : 0.0))) {  // a cancel, or the launcher closing
      SDL_KillProcess(p, true);
      wait_quiet(p, true, &code);
      SDL_DestroyProcess(p);
      fs::remove(part, ec);
      *cancelled = true;
      return *error = "canceled", false;
    }
    if (wait_quiet(p, false, &code)) break;
    char buf[256];
    const size_t n = io ? SDL_ReadIO(io, buf, sizeof buf) : 0;
    said.append(buf, n);
    if (n == 0) SDL_Delay(50);
  }
  if (io)
    for (char buf[256]; const size_t n = SDL_ReadIO(io, buf, sizeof buf);) said.append(buf, n);
  SDL_DestroyProcess(p);
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
                         const std::function<bool(double)>& progress, bool allow_file, const char* log) {
  namespace u = upgrades::update;
  std::vector<const std::vector<u::Asset>*> homes = {&r.assets};
  for (const std::vector<u::Asset>& h : r.other_homes) homes.push_back(&h);
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
    else if (!curl_get(sums->url, 60, &list, &l.error, allow_file)) l.why = Fetched::kSumsFailed;
    else if ((l.line = u::sums_line(list, name)).empty()) l.why = Fetched::kNoLine;
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
    const std::string got = rekit::sha256_file_hex(path);
    if (got != want) {
      fs::remove(path, ec);
      f.why = Fetched::kMismatch, f.want = want, f.got = got;
      fail(f, k, "it is " + got + ", SHA256SUMS says " + want);
      continue;
    }
    if (k > 0) std::printf("mw3: launcher: %s: %s downloaded from %s\n", log, name.c_str(), host_of(file->url).c_str());
    Fetched ok;
    ok.why = Fetched::kOk, ok.sha256 = got;
    return ok;
  }
  fs::remove(path, ec);
  return first;
}

Downloaded download_update(const upgrades::update::Release& r, const std::string& dir, const std::function<bool(double)>& progress) {
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
      d.ok = true, d.path = path;
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
  }
  return d;
}

namespace {
// A tool run to its end with its arguments as an array (hdiutil): its exit code, its output in `said`.
int run_tool(const std::vector<std::string>& args, int seconds, std::string* said) {
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
  if (!p) return -1;
  int code = -1;
  bool ended = false;
  SDL_IOStream* io = SDL_GetProcessOutput(p);
  const Uint64 until = SDL_GetTicks() + static_cast<Uint64>(seconds) * 1000;
  while (!(ended = wait_quiet(p, false, &code)) && SDL_GetTicks() < until) {
    char buf[256];
    const size_t n = io ? SDL_ReadIO(io, buf, sizeof buf) : 0;
    said->append(buf, n);
    if (n == 0) SDL_Delay(20);
  }
  if (!ended) SDL_KillProcess(p, true), wait_quiet(p, true, &code), code = -1;
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

int run_program(const std::vector<std::string>& args, int seconds, std::string* said) { return run_tool(args, seconds, said); }

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
  if (run_tool({"/usr/bin/hdiutil", "attach", "-nobrowse", "-readonly", "-noautoopen", "-mountpoint", mnt.string(), download}, 120, &said) != 0) {
    fs::remove(mnt, ec);
    return in.words = "The downloaded disk image could not be opened, so nothing was installed.", in;
  }
  fs::path fresh;
  for (const auto& e : fs::directory_iterator(mnt, ec))
    if (is_our_app(e.path())) fresh = e.path();
  std::string error;
  const bool swapped = !fresh.empty() && upgrades::update::swap_in(fresh.string(), target, &error);
  std::string detach_said;
  run_tool({"/usr/bin/hdiutil", "detach", mnt.string(), "-force"}, 60, &detach_said);
  fs::remove(mnt, ec);
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
    if (bar == std::string::npos) return {{with_repo(s), true}};
    return {{with_repo(s.substr(0, bar)), false}, {with_repo(s.substr(bar + 1)), true}};
  }
  return {{"https://git.owenmorgan.com/api/v1/repos/telltaleatheist/" + repo + "/releases?limit=40", false},
          {"https://api.github.com/repos/telltaleatheist/" + repo + "/releases?per_page=" + std::to_string(github_per_page), true}};
}

std::vector<upgrades::update::Answer> ask_lists(const std::vector<ReleaseList>& lists, int seconds, bool allow_file, const std::string& log) {
  std::vector<upgrades::update::Answer> answers;
  for (const ReleaseList& l : lists) {
    upgrades::update::Answer a;
    a.download_first = l.download_first;
    curl_get(l.url, seconds, &a.json, &a.fetch_error, allow_file);
    std::printf("mw3: launcher: %s at %s: %s\n", log.c_str(), l.url.c_str(), a.json.empty() ? a.fetch_error.c_str() : "answered");
    answers.push_back(std::move(a));
  }
  return answers;
}

// Both repos of the package (2026-11-21): the hub mw3-re, where every copy up to 1.2.1 looks, and mech3-re,
// MechWarrior 3's own repo after the split; each in both homes (2026-11-22: git.owenmorgan.com, then GitHub);
// the newest package on any of the four wins. MW3_UPDATE_API: one list or two (release_lists), for checks.
upgrades::update::Verdict check_for_updates(const std::string& current) {
  const char* api = std::getenv("MW3_UPDATE_API");
  std::vector<upgrades::update::Answer> answers;
  if (api) {
    answers = ask_lists(release_lists(api, "", 40), 20, true, "update check");
  } else {
    for (const char* repo : {"mw3-re", "mech3-re"})
      for (upgrades::update::Answer& a : ask_lists(release_lists(nullptr, repo, 40), 20, false, "update check")) answers.push_back(std::move(a));
  }
  return upgrades::update::judge(current, answers);
}

}  // namespace sdlfe
