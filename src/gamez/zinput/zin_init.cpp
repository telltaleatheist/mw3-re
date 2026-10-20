// zInput's start, stop and the devices' bookkeeping (zin_init.cpp, docs/input.md).
#include <cstdio>

#include "gamez/zerror/zerr.h"
#include "gamez/zinput/zinput.h"

namespace zin {

namespace {
State g_state;
Host g_default_host;
platform::Input* g_di = nullptr;
}  // namespace

Host* g_host = &g_default_host;

State& state() { return g_state; }

void reset_state() {
  g_state.~State();
  new (&g_state) State();
}

void set_input(platform::Input* di) { g_di = di; }
void set_host(Host* host) { g_host = host ? host : &g_default_host; }

// Mech3.exe 1.2 @ 0x0051b0a0
// FIX (A): DirectInputCreateA(instance, 0x500) is the platform's input.
int32_t start(uint32_t window, uint32_t) {
  State& s = g_state;
  if (s.window != 0) return 1;
  s.window = 0;
  s.kbd_flags = s.joy_flags = s.mouse_flags = 0;
  s.kbd_users = s.joy_users = s.mouse_users = 0;
  s.di = g_di;
  if (!s.di) {
    report(static_cast<int32_t>(0x80004005), "C:\\zipper\\GameZMech3\\zInput\\zin_init.cpp", 0x96);
    return -1;
  }
  s.window = window;
  s.kbd_flags = kbd_init() == 0 ? 1 : 0;
  s.mouse_flags = mouse_init() != 0 ? 1 : 0;
  s.joy_flags = joy_init(window) != 0 ? 1 : 0;
  kbd_use();
  mouse_use();
  joy_use();
  s.started = 1;
  return 0;
}

// Mech3.exe 1.2 @ 0x0051b170
int32_t stop() {
  State& s = g_state;
  if (s.window == 0) return 1;
  joy_close();
  kbd_close();
  mouse_close();
  s.di = nullptr;  // IDirectInput::Release
  s.window = 0;
  s.started = 0;
  return 0;
}

// Mech3.exe 1.2 @ 0x0051b1c0
void reset() {
  kbd_reset();
  joy_reset();
  mouse_reset();
}

// Mech3.exe 1.2 @ 0x0051b1d0, 0x0051b1e0, 0x0051b1f0
uint32_t mouse_not_suspended() { return (~g_state.mouse_flags & 2) >> 1; }
uint32_t joy_not_suspended() { return (~g_state.joy_flags & 2) >> 1; }
uint32_t kbd_not_suspended() { return (~g_state.kbd_flags & 2) >> 1; }

// Mech3.exe 1.2 @ 0x0051b050
// The window lost the focus (zGameApp's deactivate): each device marked
// suspended. Nothing is unacquired here; DirectInput takes a foreground
// device away by itself.
void suspend() {
  if (joy_not_suspended()) g_state.joy_flags |= 2;      // 0x0051b270
  if (mouse_not_suspended()) g_state.mouse_flags |= 2;  // 0x0051b260
  if (kbd_not_suspended()) g_state.kbd_flags |= 2;      // 0x0051b280
}

// Mech3.exe 1.2 @ 0x0051b080
// Back in the foreground: each suspended device's state cleared
// (0x0051b220 joystick, 0x0051b240 keyboard, 0x0051b200 mouse).
void resume() {
  State& s = g_state;
  if (s.window == 0) return;
  if (s.joy_flags & 2) joy_reset();
  s.joy_flags &= 0xfd;
  if (s.kbd_flags & 2) kbd_reset();
  s.kbd_flags &= 0xfd;
  if (s.mouse_flags & 2) mouse_reset();
  s.mouse_flags &= 0xfd;
}

// Mech3.exe 1.2 @ 0x0051b290
// Use counts: a device is read by poll() only while someone uses it; the
// first user clears its state. The keyboard is never let go.
uint32_t kbd_use() {
  State& s = g_state;
  if (s.kbd_flags & 1) {
    if (s.kbd_users == 0) kbd_reset();
    ++s.kbd_users;
  }
  return s.kbd_users;
}

// Mech3.exe 1.2 @ 0x0051b2c0
uint32_t joy_use() {
  State& s = g_state;
  if (s.joy_flags & 1) {
    if (s.joy_users == 0) joy_reset();
    ++s.joy_users;
  }
  return s.joy_users;
}

// Mech3.exe 1.2 @ 0x0051b2f0
uint32_t joy_unuse() {
  if (g_state.joy_users != 0) --g_state.joy_users;
  return g_state.joy_users;
}

// Mech3.exe 1.2 @ 0x0051b310
uint32_t mouse_use() {
  State& s = g_state;
  if (s.mouse_flags & 1) {
    if (s.mouse_users == 0) mouse_reset();
    ++s.mouse_users;
  }
  return s.mouse_users;
}

// Mech3.exe 1.2 @ 0x0051b340
uint32_t mouse_unuse() {
  if (g_state.mouse_users != 0) --g_state.mouse_users;
  return g_state.mouse_users;
}

// Mech3.exe 1.2 @ 0x0051b360
// A device is read when present, not suspended (flags exactly 1) and used.
void poll(bool callbacks) {
  State& s = g_state;
  if (s.mouse_flags == 1 && s.mouse_users != 0) mouse_poll(callbacks);
  if (s.joy_flags == 1 && s.joy_users != 0) joy_read(callbacks);
  if (s.kbd_flags == 1 && s.kbd_users != 0) kbd_read(callbacks);
}

// Mech3.exe 1.2 @ 0x0051ddc0
// The DirectInput error's name to the error report (empty in the release
// build, game-loop.md).
uint32_t report(int32_t hr, const char* file, int32_t line) {
  static const struct {
    uint32_t hr;
    const char* name;
  } kNames[] = {
      {0x80040110, "DIERR_NOAGGREGATION"}, {0x80004001, "DIERR_UNSUPPORTED"},
      {0x80004002, "DIERR_NOINTERFACE"},   {0x80004005, "DIERR_GENERIC"},
      {0x80070002, "DIERR_OBJECTNOTFOUND"}, {0x80040154, "DIERR_DEVICENOTREG"},
      {0x8007000c, "DIERR_NOTACQUIRED"},   {0x80070005, "DIERR_READONLY"},
      {0x80070015, "DIERR_NOTINITIALIZED"}, {0x8007000e, "DIERR_OUTOFMEMORY"},
      {0x80070057, "DIERR_INVALIDPARAM"},  {0x8007001e, "DIERR_INPUTLOST"},
      {0x800700aa, "DIERR_ACQUIRED"},      {0x80070077, "DIERR_BADDRIVERVER"},
      {0x80070481, "DIERR_BETADIRECTINPUTVERSION"}, {0x8007047e, "DIERR_OLDDIRECTINPUTVERSION"},
      {0x800704df, "DIERR_ALREADYINITIALIZED"},
  };
  if (hr == 0) return 1;
  const char* name = "Unknown Error";
  for (const auto& n : kNames)
    if (n.hr == static_cast<uint32_t>(hr)) name = n.name;
  zerr::report(0x800, file, line, name);
  return 0;
}

}  // namespace zin
