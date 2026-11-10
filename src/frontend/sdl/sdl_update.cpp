#include "frontend/sdl/sdl_update.h"

#include "mech3/patch_import.h"

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include <SDL3/SDL.h>

#include <cstdio>
#include <algorithm>
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
  static int n = 0;
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

bool curl_get(const std::string& url, int seconds, std::string* out, std::string* error, bool allow_file) {
  out->clear();
  const std::string curl = curl_path();
  if (curl.empty()) return *error = "this computer has no curl to ask GitHub with", false;
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
  SDL_Process* p = SDL_CreateProcessWithProperties(props);
  SDL_DestroyProperties(props);
  if (!p) return *error = std::string("curl could not be started (") + SDL_GetError() + ")", false;
  // Its few lines read as they come (a full pipe would stall it), until it ends or our own limit passes.
  std::string said;
  int code = -1;
  bool ended = false;
  const Uint64 until = SDL_GetTicks() + static_cast<Uint64>(seconds + 5) * 1000;
  SDL_IOStream* io = SDL_GetProcessOutput(p);
  while (!(ended = SDL_WaitProcess(p, false, &code)) && SDL_GetTicks() < until) {
    char buf[256];
    if (io) {
      const size_t n = SDL_ReadIO(io, buf, sizeof buf);
      said.append(buf, n);
      if (n == 0) SDL_Delay(20);
    } else {
      SDL_Delay(20);
    }
  }
  if (!ended) {
    SDL_KillProcess(p, true);
    SDL_WaitProcess(p, true, &code);
    SDL_DestroyProcess(p);
    fs::remove(body, ec);
    return *error = "GitHub did not answer in time", false;
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
  SDL_Process* p = SDL_CreateProcessWithProperties(props);
  SDL_DestroyProperties(props);
  if (!p) return *error = std::string("curl could not be started (") + SDL_GetError() + ")", false;
  std::string said;
  int code = -1;
  SDL_IOStream* io = SDL_GetProcessOutput(p);
  for (;;) {  // the bar (and a cancel) asked before each wait, so even a quick copy can be stopped
    const uint64_t got = fs::exists(part, ec) ? static_cast<uint64_t>(fs::file_size(part, ec)) : 0;
    if (progress && !progress(size ? std::min(1.0, static_cast<double>(got) / static_cast<double>(size)) : 0.0)) {
      SDL_KillProcess(p, true);
      SDL_WaitProcess(p, true, &code);
      SDL_DestroyProcess(p);
      fs::remove(part, ec);
      *cancelled = true;
      return *error = "cancelled", false;
    }
    if (SDL_WaitProcess(p, false, &code)) break;
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

Downloaded download_update(const upgrades::update::Release& r, const std::string& dir, const std::function<bool(double)>& progress) {
  namespace u = upgrades::update;
  Downloaded d;
  const bool allow_file = std::getenv("MW3_UPDATE_API") != nullptr;  // checks: a fixture's file:// assets
  const u::Platform platform = u::this_platform();
  const u::Asset* package = u::platform_asset(r, platform);
  if (!package) return d.words = std::string("This release has no ") + u::platform_package_words(platform) + " to download.", d;
  const u::Asset* sums = nullptr;
  for (const u::Asset& a : r.assets)
    if (a.name == "SHA256SUMS") sums = &a;
  if (!sums) return d.words = "This release has no SHA256SUMS to check the download against, so nothing was downloaded.", d;
  std::string list, error;
  if (!curl_get(sums->url, 60, &list, &error, allow_file)) return d.words = "Could not download the release's SHA256SUMS: " + error + ".", d;
  const std::string want = u::sums_line(list, package->name);
  if (want.empty()) return d.words = "The release's SHA256SUMS has no line for " + package->name + ", so nothing was downloaded.", d;
  std::error_code ec;
  fs::create_directories(dir, ec);
  const std::string path = (fs::path(dir) / package->name).string();
  bool cancelled = false;
  if (!curl_download(package->url, path, package->size, progress, &error, &cancelled, allow_file)) {
    d.cancelled = cancelled;
    return d.words = cancelled ? "The download was cancelled. Nothing was installed." : "The download stopped: " + error + ". Nothing was installed.", d;
  }
  const std::string got = mech3::sha256_file_hex(path);
  if (got != want) {
    fs::remove(path, ec);
    std::printf("mw3: launcher: update: %s is %s, SHA256SUMS says %s\n", package->name.c_str(), got.c_str(), want.c_str());
    return d.words = "The download did not match its checksum, so nothing was installed. Try again; if it happens again, download it from the release's page.", d;
  }
  d.ok = true, d.path = path;
  d.words = "Downloaded and checked: " + package->name + ".";
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
  SDL_Process* p = SDL_CreateProcessWithProperties(props);
  SDL_DestroyProperties(props);
  if (!p) return -1;
  int code = -1;
  bool ended = false;
  SDL_IOStream* io = SDL_GetProcessOutput(p);
  const Uint64 until = SDL_GetTicks() + static_cast<Uint64>(seconds) * 1000;
  while (!(ended = SDL_WaitProcess(p, false, &code)) && SDL_GetTicks() < until) {
    char buf[256];
    const size_t n = io ? SDL_ReadIO(io, buf, sizeof buf) : 0;
    said->append(buf, n);
    if (n == 0) SDL_Delay(20);
  }
  if (!ended) SDL_KillProcess(p, true), SDL_WaitProcess(p, true, &code), code = -1;
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
  in.words = "The new Setup is running: follow it, then start MechWarrior 3 Rebuild again.";
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
  in.words = "Installed. MechWarrior 3 Rebuild starts again now.";
  return in;
#endif
}

upgrades::update::Verdict check_for_updates(const std::string& current) {
  const char* api = std::getenv("MW3_UPDATE_API");
  const std::string url = api ? api : "https://api.github.com/repos/telltaleatheist/mw3-re/releases?per_page=40";
  std::string json, error;
  curl_get(url, 20, &json, &error, api != nullptr);
  std::printf("mw3: launcher: update check at %s: %s\n", url.c_str(), json.empty() ? error.c_str() : "answered");
  return upgrades::update::judge(current, json, error);
}

}  // namespace sdlfe
