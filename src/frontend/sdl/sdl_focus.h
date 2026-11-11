// Ours, not original code. FIX (A), playtest W4 (docs/input.md, The focus is
// checked): on Windows a game started by the launcher could come up with the
// cursor stuck at the centre. SDL's relative mouse (raw input) reaches only
// the window SDL holds as its keyboard focus, and SDL's Windows code notes
// that focus only when the window is the system's foreground window at the
// moment an activation message arrives (SDL 3.4.12, SDL_windowsevents.c,
// WIN_UpdateFocus; its own comment: "it's possible to get WM_ACTIVATE and
// WM_SETFOCUS without actually being the foreground window"). A window
// activated while the launcher was still the foreground one was never noted,
// no focus event came, and the game held a mouse that gave no motion.
// So the game asks the real state (SdlInput::has_focus) and, when Windows
// says the window is in front and SDL has not noted it, has SDL look again.
#pragma once

#include <string>

#include <SDL3/SDL.h>

namespace sdlfe {

// The system's foreground window is this one (Windows); elsewhere SDL's
// input-focus flag.
bool window_is_foreground(SDL_Window* w);
// The window really has the keyboard: SDL's keyboard focus is this window
// and the system agrees.
bool window_has_focus(SDL_Window* w);
// In front for the system and not noted by SDL: an activation message is
// posted to the window, on which SDL looks again and sends its focus event.
// True when one was posted. Never brings the window to the front itself.
bool window_focus_nudge(SDL_Window* w);
// The focus for SdlInput::has_focus: window_has_focus, and, should SDL not
// note a window the system has had in front for kFrontNs (the nudge did
// nothing), the system's word alone, so the keyboard is never kept waiting on
// SDL's. The mouse is not cured by that: SDL gives motion only to the window
// it holds as its focus, so the nudge goes on (SdlInput::check_focus).
struct FocusWatch {
  static constexpr Uint64 kFrontNs = 2000000000ull;
  SDL_Window* window = nullptr;
  Uint64 front_since = 0;
  bool said = false;
  bool operator()();
};
// Another window is the foreground one and the cursor is still clipped to a
// point on ours (SDL's relative mode, kept when SDL missed the loss of the focus):
// the clip is let go, so the cursor is free where the player now is. True
// when it was.
bool window_unclip_behind(SDL_Window* w);
// One line for the log: SDL's keyboard and mouse focus, the window's flags,
// relative mode, the foreground window ours or not and the cursor's clip
// rectangle (Windows).
std::string window_focus_report(SDL_Window* w);
// The launcher's (Windows): the game it started may bring its window to the
// front (AllowSetForegroundWindow), as a start-again's child may
// (sdl_restart.cpp). False elsewhere or when it could not be granted.
bool allow_foreground(SDL_Process* child);

}  // namespace sdlfe
