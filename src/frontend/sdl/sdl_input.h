#pragma once
#include <SDL3/SDL.h>

#include <deque>

#include "gamez/zinput/zinput.h"
#include "platform/platform.h"

// platform::Input on SDL3 (docs/program.md, Input): DirectInput's devices as
// zInput opens them (0x0051b0a0). FIX (A): DirectInput does not exist on
// macOS. The keyboard's events arrive as SDL scancodes (USB usages) and are
// given back as the DIK codes (the PC keyboard's set-1 scan codes) the game's
// key tables use, one key to one key; the mouse's motion is summed into
// DIMOUSESTATE's relative counts, the first joystick read into a DIJOYSTATE2
// with DirectInput's default 0..65535 axis range.
namespace sdlfe {

// The DIK code of an SDL scancode, 0 for keys a 1998 keyboard did not have.
uint8_t dik_from_scancode(SDL_Scancode s);

class SdlInput : public platform::Input {
 public:
  ~SdlInput() override;
  bool open(Device d) override;
  void close(Device d) override;
  int32_t acquire(Device d) override;
  int32_t unacquire(Device d) override;
  int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override;
  int32_t mouse_state(platform::MouseState* out) override;
  int32_t joystick_state(platform::JoyState* out) override;
  void set_cooperative_level(Device d, uint32_t flags) override;
  int32_t joystick_caps(uint32_t* flags, uint32_t* axes) override;
  int32_t joystick_object_name(uint32_t ofs, char* name, uint32_t n) override;
  int32_t joystick_set_range(uint32_t ofs, int32_t min, int32_t max) override;
  int32_t joystick_get_range(uint32_t ofs, int32_t* min, int32_t* max) override;
  int32_t joystick_set_deadzone(uint32_t ofs, uint32_t dz) override;

  // The window an exclusive mouse captures (relative mouse mode, as
  // DirectInput's DISCL_EXCLUSIVE hides and holds the cursor), and the
  // mickeys a window pixel of motion makes (the game's 640 over the window's width).
  SDL_Window* window = nullptr;
  float mickey_scale = 1.0f;

  // From the message pump: every SDL input event passes through here.
  void on_event(const SDL_Event& e);

  // The keys held now, by DIK code (kept from the events, for tools).
  bool key_down(uint8_t dik) const { return held_[dik]; }
  // The mouse in window pixels and its buttons now (Windows' cursor, GetCursorPos).
  float mouse_x = 0, mouse_y = 0;
  uint32_t mouse_buttons = 0;  // bit 0 left, 1 right, 2 middle
  // Window-pixel motion since the last call, whether or not zInput holds the
  // mouse (for tools such as the free camera; the game reads mouse_state).
  void take_motion(float* x, float* y) { *x = raw_dx_, *y = raw_dy_, raw_dx_ = raw_dy_ = 0; }

  static constexpr uint32_t kBufferSize = 0x80;  // DIPROP_BUFFERSIZE (0x0051a170)

 private:
  void capture(bool on);
  bool opened_[3] = {};
  bool acquired_[3] = {};
  bool lost_[3] = {};
  uint32_t coop_[3] = {};
  bool focused_ = true;
  int32_t range_[8][2] = {{0, 65535}, {0, 65535}, {0, 65535}, {0, 65535},
                          {0, 65535}, {0, 65535}, {0, 65535}, {0, 65535}};
  uint32_t deadzone_[8] = {};
  std::deque<platform::KeyEvent> keys_;
  bool overflow_ = false;
  uint32_t sequence_ = 0;
  bool held_[256] = {};
  float dx_ = 0, dy_ = 0, dz_ = 0;
  float raw_dx_ = 0, raw_dy_ = 0;
  uint8_t buttons_[4] = {};
  SDL_Joystick* joy_ = nullptr;
};

// zInput's Win32 calls on SDL (FIX (A), docs/input.md): the client area is
// the game's frame (the window scaled to it), the cursor SDL's, and the
// characters (MapVirtualKey, GetKeyboardState, ToAscii) the SDL keymap's.
class SdlZinHost : public zin::Host {
 public:
  SdlZinHost(SdlInput* input, int frame_w = 640, int frame_h = 480) : input_(input), fw_(frame_w), fh_(frame_h) {}
  void client_to_screen(int32_t* x, int32_t* y) override;
  void screen_to_client(int32_t* x, int32_t* y) override;
  void set_cursor_pos(int32_t x, int32_t y) override;
  void client_size(int32_t* w, int32_t* h) override { *w = fw_, *h = fh_; }
  uint32_t map_virtual_key(uint32_t scan) override;
  bool keyboard_state(uint8_t* state) override;
  int32_t to_ascii(uint32_t vk, uint32_t scan, const uint8_t* state, uint16_t* out) override;
  uint32_t* settings_block(const char* name, uint32_t size) override;  // the settings store's entry

 private:
  void window_size(int* w, int* h) const;
  SdlInput* input_;
  int fw_, fh_;
};

}  // namespace sdlfe
