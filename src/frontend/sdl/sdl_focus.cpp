#include "frontend/sdl/sdl_focus.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <cstdio>

namespace sdlfe {

#if defined(_WIN32)
namespace {
HWND hwnd_of(SDL_Window* w) {
  return w ? static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(w), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr)) : nullptr;
}
}  // namespace
#endif

bool window_is_foreground(SDL_Window* w) {
  if (!w) return false;
#if defined(_WIN32)
  const HWND h = hwnd_of(w);
  return h && GetForegroundWindow() == h;
#else
  return (SDL_GetWindowFlags(w) & SDL_WINDOW_INPUT_FOCUS) != 0;
#endif
}

bool window_has_focus(SDL_Window* w) { return w && SDL_GetKeyboardFocus() == w && window_is_foreground(w); }

bool FocusWatch::operator()() {
  if (window_has_focus(window)) return front_since = 0, true;
  if (!window_is_foreground(window)) return front_since = 0, false;
  const Uint64 now = SDL_GetTicksNS();
  if (!front_since) front_since = now ? now : 1;
  if (now - front_since < kFrontNs) return false;
  if (!said) std::printf("mw3: focus: the window has been in front 2 s and SDL still has not noted it: taken as focused (%s)\n", window_focus_report(window).c_str());
  said = true;
  return true;
}

bool window_focus_nudge(SDL_Window* w) {
#if defined(_WIN32)
  const HWND h = hwnd_of(w);
  if (!h || GetForegroundWindow() != h || SDL_GetKeyboardFocus() == w) return false;
  return PostMessageW(h, WM_ACTIVATE, WA_ACTIVE, 0) != 0;
#else
  (void)w;
  return false;
#endif
}

bool window_unclip_behind(SDL_Window* w) {
#if defined(_WIN32)
  const HWND h = hwnd_of(w);
  RECT clip{};
  if (!h || GetForegroundWindow() == h || !GetClipCursor(&clip)) return false;
  if (clip.right - clip.left > 2 || clip.bottom - clip.top > 2) return false;  // not a point: not relative mode's
  return ClipCursor(nullptr) != 0;
#else
  (void)w;
  return false;
#endif
}

std::string window_focus_report(SDL_Window* w) {
  if (!w) return "no window";
  const SDL_WindowFlags f = SDL_GetWindowFlags(w);
  char line[256];
  std::snprintf(line, sizeof line, "SDL's keyboard focus %s, mouse focus %s, input-focus flag %d, mouse-focus flag %d, relative %d",
                SDL_GetKeyboardFocus() == w ? "ours" : SDL_GetKeyboardFocus() ? "another window" : "none",
                SDL_GetMouseFocus() == w ? "ours" : SDL_GetMouseFocus() ? "another window" : "none", (f & SDL_WINDOW_INPUT_FOCUS) != 0,
                (f & SDL_WINDOW_MOUSE_FOCUS) != 0, SDL_GetWindowRelativeMouseMode(w));
  std::string out = line;
#if defined(_WIN32)
  const HWND h = hwnd_of(w);
  const HWND fg = GetForegroundWindow();
  RECT clip{};
  const BOOL got = GetClipCursor(&clip);
  std::snprintf(line, sizeof line, ", the foreground window %s, the cursor's clip %s%ld,%ld-%ld,%ld", fg == h ? "ours" : fg ? "another" : "none",
                got ? "" : "(unread) ", static_cast<long>(clip.left), static_cast<long>(clip.top), static_cast<long>(clip.right),
                static_cast<long>(clip.bottom));
  out += line;
#endif
  return out;
}

bool allow_foreground(SDL_Process* child) {
#if defined(_WIN32)
  const Sint64 pid = child ? SDL_GetNumberProperty(SDL_GetProcessProperties(child), SDL_PROP_PROCESS_PID_NUMBER, 0) : 0;
  return pid > 0 && AllowSetForegroundWindow(static_cast<DWORD>(pid)) != 0;
#else
  (void)child;
  return false;
#endif
}

}  // namespace sdlfe
