// The mouse (docs/input.md section 2). Original file unknown: it follows
// zin_init.cpp in the exe (0x0051b3c0..0x0051baf0).
#pragma STDC FP_CONTRACT OFF
#include "gamez/zinput/zinput.h"

namespace zin {

extern Host* g_host;

namespace {
// The integrated position's floats (0x0051b910's tail).
void scale_all(State& s) {
  MouseInfo& m = s.mouse;
  m.fx = static_cast<float>(m.x - s.centre_x) * s.scale_x;
  m.fy = static_cast<float>(m.y - s.centre_y) * s.scale_y;
  m.fz = static_cast<float>(m.z - s.centre_z) * s.scale_z;
  m.fdx = static_cast<float>(m.dx) * s.scale_x;
  m.fdy = static_cast<float>(m.dy) * s.scale_y;
  m.fdz = static_cast<float>(m.dz) * s.scale_z;
}
}  // namespace

// Mech3.exe 1.2 @ 0x0051b3c0
// FIX (A): ClientToScreen and SetCursorPos are the host's.
void mouse_to_cursor() {
  int32_t x = state().mouse.x, y = state().mouse.y;
  g_host->client_to_screen(&x, &y);
  g_host->set_cursor_pos(x, y);
}

// Mech3.exe 1.2 @ 0x0051b400
void mouse_size_from_window() {
  int32_t w = 0, h = 0;
  g_host->client_size(&w, &h);
  state().width = w;
  state().height = h;
  mouse_set_screen(w, h);
}

// Mech3.exe 1.2 @ 0x0051b440
// The mouse put at a point given from the centre, -1..1 each way.
void mouse_set(float fx, float fy) {
  State& s = state();
  if (fx > 1.0f) fx = 1.0f;
  if (!(fx >= -1.0f)) fx = -1.0f;
  if (fy > 1.0f) fy = 1.0f;
  if (!(fy >= -1.0f)) fy = -1.0f;
  s.mouse.fy = fy;
  s.mouse.fx = fx;
  s.mouse.x = static_cast<int32_t>(static_cast<float>(s.centre_x) * fx) + s.centre_x;
  s.mouse.y = static_cast<int32_t>(static_cast<float>(s.centre_y) * fy) + s.centre_y;
  mouse_to_cursor();
}

// Mech3.exe 1.2 @ 0x0051b4f0
// The mouse put where Windows' cursor is (the missions in a window, input mode 8).
void mouse_from_cursor(int32_t x, int32_t y) {
  State& s = state();
  g_host->screen_to_client(&x, &y);
  s.mouse.x = x;
  s.mouse.y = y;
  s.mouse.fx = static_cast<float>(x - s.centre_x) * s.scale_x;
  s.mouse.fy = static_cast<float>(y - s.centre_y) * s.scale_y;
}

// Mech3.exe 1.2 @ 0x0051b560
void mouse_centre() {
  State& s = state();
  s.mouse.x = s.centre_x;
  s.mouse.y = s.centre_y;
  s.mouse.fx = 0.0f;
  s.mouse.fy = 0.0f;
  mouse_to_cursor();
}

// Mech3.exe 1.2 @ 0x0051b590
// The screen the mouse moves over (the options' screen mode, 0x0040d810).
void mouse_set_screen(int32_t w, int32_t h) {
  State& s = state();
  s.width = w;
  s.height = h;
  s.centre_x = w / 2;
  s.centre_z = 500;
  s.centre_y = h / 2;
  s.scale_z = 0.002f;  // 0x3b03126f
  s.scale_x = 1.0f / static_cast<float>(w / 2);
  s.scale_y = 1.0f / static_cast<float>(h / 2);
}

// Mech3.exe 1.2 @ 0x0051b5f0
// FIX (A): CreateDevice(GUID_SysMouse) and the IDirectInputDevice2 query,
// c_dfDIMouse, DIPROP_BUFFERSIZE 16 are the platform's open(); the results
// are ignored, as written, so a mouse always counts as present.
uint32_t mouse_init() {
  State& s = state();
  s.mouse_open = s.di->open(platform::Input::kMouse);
  s.di->set_cooperative_level(platform::Input::kMouse, s.mode);
  s.mouse.left = s.mouse.right = s.mouse.middle = 0;
  s.mouse_ready = 1;
  s.mouse_acquired = 1;
  mouse_toggle_acquire();
  if (s.width < 1) mouse_size_from_window();
  mouse_centre();
  s.mouse_ready = 1;
  return 1;
}

// Mech3.exe 1.2 @ 0x0051b6e0
uint32_t mouse_button(int32_t b) {
  const State& s = state();
  const bool now = s.mouse_cur.buttons[b - 1] != 0, before = s.mouse_prev.buttons[b - 1] != 0;
  if (now) return before ? kHeld : kPressed;
  return before ? kReleased : 0;
}

// Mech3.exe 1.2 @ 0x0051b710
// Makes the device what the flag says: acquired when set (cleared when that
// fails), unacquired when clear (set when that fails).
void mouse_toggle_acquire() {
  State& s = state();
  if (!s.mouse_open) return;
  if (s.mouse_acquired == 0) {
    if (s.di->unacquire(platform::Input::kMouse) != 0) s.mouse_acquired = 1;
  } else if (s.di->acquire(platform::Input::kMouse) != 0) {
    s.mouse_acquired = 0;
  }
}

// Mech3.exe 1.2 @ 0x0051b750
uint32_t mouse_close() {
  State& s = state();
  s.mouse_acquired = 0;
  mouse_toggle_acquire();
  if (s.mouse_open) s.di->close(platform::Input::kMouse);
  s.mouse_open = false;
  s.mouse_ready = 0;
  return 1;
}

// Mech3.exe 1.2 @ 0x0051b790
void mouse_set_accel(float x, float y) {
  state().accel_x = x;
  state().accel_y = y;
}

// Mech3.exe 1.2 @ 0x0051b7b0
MouseInfo* mouse_info() { return &state().mouse; }

// Mech3.exe 1.2 @ 0x0051b7c0
void mouse_poll(bool callbacks) { state().mouse_result = mouse_read(callbacks); }

// Mech3.exe 1.2 @ 0x0051b7d0
// FIX (A): Poll on the mouse does nothing on the platform.
int32_t mouse_read(bool callbacks) {
  State& s = state();
  s.mouse.dx = 0;
  s.mouse.dy = 0;
  if (s.mouse_acquired == 0) {
    s.mouse_acquired = 1;
    mouse_toggle_acquire();
    if (s.mouse_acquired == 0) return platform::Input::kInputLost;
  }
  const int32_t hr = s.mouse_open ? s.di->mouse_state(&s.mouse_read) : platform::Input::kNotAcquired;
  if (hr == platform::Input::kInputLost) {
    mouse_toggle_acquire();
    return hr;
  }
  if (hr != 0) return hr;
  s.mouse_prev = s.mouse_cur;
  s.mouse_cur = s.mouse_read;
  s.mouse.dx = s.mouse_read.x;
  s.mouse.dy = s.mouse_read.y;
  s.mouse.dz = s.mouse_read.z;
  mouse_integrate();
  s.mouse.left = mouse_button(1);
  s.mouse.right = mouse_button(2);
  s.mouse.middle = mouse_button(3);
  if (s.map && callbacks) mouse_callbacks();
  return hr;
}

// Mech3.exe 1.2 @ 0x0051b910
// The read's counts times the acceleration (cut toward zero) moved onto the
// position, kept on the screen; the wheel unscaled and unclamped.
void mouse_integrate() {
  State& s = state();
  MouseInfo& m = s.mouse;
  m.dx = static_cast<int32_t>(static_cast<float>(m.dx) * s.accel_x);
  m.dy = static_cast<int32_t>(static_cast<float>(m.dy) * s.accel_y);
  m.x += m.dx;
  m.y += m.dy;
  m.z += m.dz;
  if (s.no_clamp == 0) {
    if (m.x < 0) m.x = 0;
    if (m.x >= s.width) m.x = s.width - 1;
    if (m.y < 0) m.y = 0;
    if (m.y >= s.height) m.y = s.height - 1;
  }
  scale_all(s);
}

// Mech3.exe 1.2 @ 0x0051ba60
int32_t mouse_copy(MouseInfo* out) {
  if (out) *out = state().mouse;
  return state().mouse_result;
}

// Mech3.exe 1.2 @ 0x0051ba80
// The buttons forgotten (the last state becomes the previous one) and the
// motion zeroed. Quirk kept: the wheel's last count is not zeroed, so the
// recount adds it to the wheel position again.
void mouse_reset() {
  State& s = state();
  if (s.mouse_ready != 1) return;
  s.mouse_prev = s.mouse_cur;
  s.mouse.dx = 0;
  s.mouse.dy = 0;
  s.mouse.left = s.mouse.right = s.mouse.middle = 0;
  mouse_integrate();
}

// Mech3.exe 1.2 @ 0x0051bae0
uint32_t set_mode(uint32_t mode) {
  const uint32_t old = state().mode;
  state().mode = mode;
  return old;
}

// Mech3.exe 1.2 @ 0x0051baf0
// The first button held (down in two reads running), 1..3; with wait, reads
// until there is one.
int32_t mouse_wait_button(bool wait) {
  int32_t found = 0;
  do {
    if (wait) mouse_read(false);
    for (int32_t b = 1; b < 4; ++b) {
      if (mouse_button(b) == kHeld) {
        found = b;
        break;
      }
    }
    mouse_reset();
  } while (found == 0 && wait);
  return found;
}

}  // namespace zin
