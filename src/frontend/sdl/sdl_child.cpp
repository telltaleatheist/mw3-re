#include "frontend/sdl/sdl_child.h"

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

bool log_to_file(const std::string& write_dir) {
#if defined(_WIN32)
  if (GetConsoleWindow() || write_dir.empty()) return false;
  auto wide = [](const std::string& u8) {
    std::wstring w(static_cast<size_t>(MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, nullptr, 0)), L'\0');
    if (!w.empty()) MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, w.data(), static_cast<int>(w.size()));
    if (!w.empty()) w.pop_back();
    return w;
  };
  const std::wstring log = wide(write_dir + "/mw3.log"), old = wide(write_dir + "/mw3.old.log");
  MoveFileExW(log.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING);
  if (!_wfreopen(log.c_str(), L"w", stdout)) return false;
  _dup2(_fileno(stdout), 2);  // stderr into the same file, in order
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::setvbuf(stderr, nullptr, _IONBF, 0);
  return true;
#else
  (void)write_dir;
  return false;
#endif
}

}  // namespace sdlfe
