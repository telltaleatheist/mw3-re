#pragma once
#include <SDL3/SDL.h>

#include <deque>

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

  // From the message pump: every SDL input event passes through here.
  void on_event(const SDL_Event& e);

  // The keys held now, by DIK code (kept from the events, for tools).
  bool key_down(uint8_t dik) const { return held_[dik]; }
  // The mouse in window pixels and its buttons now (Windows' cursor, GetCursorPos).
  float mouse_x = 0, mouse_y = 0;
  uint32_t mouse_buttons = 0;  // bit 0 left, 1 right, 2 middle

  static constexpr uint32_t kBufferSize = 0x80;  // DIPROP_BUFFERSIZE (0x0051a170)

 private:
  bool opened_[3] = {};
  bool acquired_[3] = {};
  std::deque<platform::KeyEvent> keys_;
  bool overflow_ = false;
  uint32_t sequence_ = 0;
  bool held_[256] = {};
  float dx_ = 0, dy_ = 0, dz_ = 0;
  uint8_t buttons_[4] = {};
  SDL_Joystick* joy_ = nullptr;
};

}  // namespace sdlfe
