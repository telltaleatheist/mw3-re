#include "frontend/sdl/sdl_update.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace fs = std::filesystem;

namespace sdlfe {
namespace {

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
  const fs::path body = fs::temp_directory_path(ec) / ("mw3-update-" + std::to_string(SDL_GetTicksNS()) + ".json");
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

upgrades::update::Verdict check_for_updates(const std::string& current) {
  const char* api = std::getenv("MW3_UPDATE_API");
  const std::string url = api ? api : "https://api.github.com/repos/telltaleatheist/mw3-re/releases?per_page=40";
  std::string json, error;
  curl_get(url, 20, &json, &error, api != nullptr);
  std::printf("mw3: launcher: update check at %s: %s\n", url.c_str(), json.empty() ? error.c_str() : "answered");
  return upgrades::update::judge(current, json, error);
}

}  // namespace sdlfe
