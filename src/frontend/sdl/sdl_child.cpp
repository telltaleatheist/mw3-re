#include "frontend/sdl/sdl_child.h"

#include "frontend/sdl/log_start.h"

#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#else
#include <unistd.h>
#endif

#include <cstdio>

namespace sdlfe {

SDL_Process* start_child(const char* const* args) {
#if defined(_WIN32)
  if (!GetConsoleWindow()) {
    const SDL_PropertiesID p = SDL_CreateProperties();
    SDL_SetPointerProperty(p, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, const_cast<char**>(args));
    SDL_SetNumberProperty(p, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetNumberProperty(p, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetBooleanProperty(p, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
    SDL_Process* proc = SDL_CreateProcessWithProperties(p);
    SDL_DestroyProperties(p);
    return proc;
  }
#endif
  return SDL_CreateProcess(args, true);
}

namespace {
std::string g_ready_file;
bool g_ready = false;
}  // namespace
void set_ready_file(const std::string& path) { g_ready_file = path, g_ready = false; }
void set_ready_status(const std::string& text) {
  if (g_ready_file.empty()) return;
  const std::string path = g_ready_file + ".status";
  if (text.empty()) {
    std::remove(path.c_str());
    return;
  }
  const std::string tmp = path + ".new";  // whole lines only: written aside, then moved over
  if (std::FILE* f = std::fopen(tmp.c_str(), "wb")) {
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
    std::remove(path.c_str());
    std::rename(tmp.c_str(), path.c_str());
  }
}
void mark_ready() {
  if (g_ready || g_ready_file.empty()) return;
  g_ready = true;
  set_ready_status("");
  if (std::FILE* f = std::fopen(g_ready_file.c_str(), "wb")) std::fclose(f);
  std::printf("mw3: first frame shown %llu ms after the start (the launcher told)\n",
              static_cast<unsigned long long>(SDL_GetTicks()));
}

bool log_to_file(const std::string& write_dir, const std::string& name) {
#if defined(_WIN32)
  if (GetConsoleWindow() || write_dir.empty()) return false;
  auto wide = [](const std::string& u8) {
    std::wstring w(static_cast<size_t>(MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, nullptr, 0)), L'\0');
    if (!w.empty()) MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, w.data(), static_cast<int>(w.size()));
    if (!w.empty()) w.pop_back();
    return w;
  };
  const std::wstring log = wide(write_dir + "/" + name + ".log"), old = wide(write_dir + "/" + name + ".old.log");
  // A restarted run (sdl_restart.h; restart_into names the process it comes from): that process must be gone
  // before its log can be moved, so it is waited for (2 s at most; one already gone has ended), and the move is
  // tried for about a second. The last start's log is never emptied: when it cannot be moved this start's lines
  // are appended to it, after a line that says so (log_start.h). A first start tries the move once, as before.
  wchar_t from[32] = L"";
  const bool restarted = GetEnvironmentVariableW(wide(kRestartedFromEnv).c_str(), from, 32) > 0;
  DWORD waited_ms = 0;
  bool was_alive = false;
  if (restarted) {
    SetEnvironmentVariableW(wide(kRestartedFromEnv).c_str(), nullptr);  // this start's alone, not its children's
    if (HANDLE p = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(wcstoul(from, nullptr, 10)))) {
      const DWORD t0 = GetTickCount();
      was_alive = WaitForSingleObject(p, 2000) != WAIT_FAILED;
      waited_ms = GetTickCount() - t0;
      CloseHandle(p);
    }
  }
  const LogStart s = log_start(
      GetFileAttributesW(log.c_str()) != INVALID_FILE_ATTRIBUTES, restarted ? 20 : 1,
      [&]() -> unsigned long { return MoveFileExW(log.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING) ? 0 : GetLastError(); },
      [] { Sleep(50); });
  if (!_wfreopen(log.c_str(), s.append ? L"a" : L"w", stdout)) return false;
  _dup2(_fileno(stdout), 2);  // stderr into the same file, in order
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::setvbuf(stderr, nullptr, _IONBF, 0);
  if (s.append) std::printf("\n%s\n", log_start_note(s, name + ".old.log").c_str());
  if (restarted)
    std::printf("mw3: started again from process %ls (%s, %lu ms); the last log moved at try %d\n", from,
                was_alive ? "waited for its end" : "already gone", static_cast<unsigned long>(waited_ms), s.moved ? s.tries : 0);
  return true;
#else
  (void)write_dir, (void)name;
  return false;
#endif
}

size_t resident_kb() {
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS c{};
  if (!K32GetProcessMemoryInfo(GetCurrentProcess(), &c, sizeof c)) return 0;
  return static_cast<size_t>(c.WorkingSetSize / 1024);
#elif defined(__APPLE__)
  mach_task_basic_info info{};
  mach_msg_type_number_t n = MACH_TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &n) != KERN_SUCCESS) return 0;
  return static_cast<size_t>(info.resident_size / 1024);
#else
  long pages = 0, rss = 0;
  std::FILE* f = std::fopen("/proc/self/statm", "r");
  if (!f) return 0;
  const bool ok = std::fscanf(f, "%ld %ld", &pages, &rss) == 2;
  std::fclose(f);
  return ok ? static_cast<size_t>(rss) * static_cast<size_t>(sysconf(_SC_PAGESIZE)) / 1024 : 0;
#endif
}

}  // namespace sdlfe
