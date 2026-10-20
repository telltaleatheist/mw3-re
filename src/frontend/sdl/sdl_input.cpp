#include "frontend/sdl/sdl_input.h"

#include <cmath>
#include <cstring>

namespace sdlfe {

uint8_t dik_from_scancode(SDL_Scancode s) {
  // Letters by position on the US layout, as the PC's scan codes are.
  static const uint8_t kLetters[26] = {0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17,
                                       0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13,
                                       0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c};
  if (s >= SDL_SCANCODE_A && s <= SDL_SCANCODE_Z) return kLetters[s - SDL_SCANCODE_A];
  if (s >= SDL_SCANCODE_1 && s <= SDL_SCANCODE_0) return static_cast<uint8_t>(0x02 + (s - SDL_SCANCODE_1));
  if (s >= SDL_SCANCODE_F1 && s <= SDL_SCANCODE_F10) return static_cast<uint8_t>(0x3b + (s - SDL_SCANCODE_F1));
  switch (s) {
    case SDL_SCANCODE_ESCAPE: return 0x01;           // DIK_ESCAPE
    case SDL_SCANCODE_MINUS: return 0x0c;
    case SDL_SCANCODE_EQUALS: return 0x0d;
    case SDL_SCANCODE_BACKSPACE: return 0x0e;        // DIK_BACK
    case SDL_SCANCODE_TAB: return 0x0f;
    case SDL_SCANCODE_LEFTBRACKET: return 0x1a;
    case SDL_SCANCODE_RIGHTBRACKET: return 0x1b;
    case SDL_SCANCODE_RETURN: return 0x1c;
    case SDL_SCANCODE_LCTRL: return 0x1d;
    case SDL_SCANCODE_SEMICOLON: return 0x27;
    case SDL_SCANCODE_APOSTROPHE: return 0x28;
    case SDL_SCANCODE_GRAVE: return 0x29;
    case SDL_SCANCODE_LSHIFT: return 0x2a;
    case SDL_SCANCODE_BACKSLASH: return 0x2b;
    case SDL_SCANCODE_NONUSHASH: return 0x2b;
    case SDL_SCANCODE_COMMA: return 0x33;
    case SDL_SCANCODE_PERIOD: return 0x34;
    case SDL_SCANCODE_SLASH: return 0x35;
    case SDL_SCANCODE_RSHIFT: return 0x36;
    case SDL_SCANCODE_KP_MULTIPLY: return 0x37;      // DIK_MULTIPLY
    case SDL_SCANCODE_LALT: return 0x38;             // DIK_LMENU
    case SDL_SCANCODE_SPACE: return 0x39;
    case SDL_SCANCODE_CAPSLOCK: return 0x3a;         // DIK_CAPITAL
    case SDL_SCANCODE_NUMLOCKCLEAR: return 0x45;
    case SDL_SCANCODE_SCROLLLOCK: return 0x46;
    case SDL_SCANCODE_KP_7: return 0x47;
    case SDL_SCANCODE_KP_8: return 0x48;
    case SDL_SCANCODE_KP_9: return 0x49;
    case SDL_SCANCODE_KP_MINUS: return 0x4a;         // DIK_SUBTRACT
    case SDL_SCANCODE_KP_4: return 0x4b;
    case SDL_SCANCODE_KP_5: return 0x4c;
    case SDL_SCANCODE_KP_6: return 0x4d;
    case SDL_SCANCODE_KP_PLUS: return 0x4e;          // DIK_ADD
    case SDL_SCANCODE_KP_1: return 0x4f;
    case SDL_SCANCODE_KP_2: return 0x50;
    case SDL_SCANCODE_KP_3: return 0x51;
    case SDL_SCANCODE_KP_0: return 0x52;
    case SDL_SCANCODE_KP_PERIOD: return 0x53;        // DIK_DECIMAL
    case SDL_SCANCODE_NONUSBACKSLASH: return 0x56;   // DIK_OEM_102
    case SDL_SCANCODE_F11: return 0x57;
    case SDL_SCANCODE_F12: return 0x58;
    case SDL_SCANCODE_F13: return 0x64;
    case SDL_SCANCODE_F14: return 0x65;
    case SDL_SCANCODE_F15: return 0x66;
    case SDL_SCANCODE_KP_EQUALS: return 0x8d;        // DIK_NUMPADEQUALS
    case SDL_SCANCODE_KP_ENTER: return 0x9c;         // DIK_NUMPADENTER
    case SDL_SCANCODE_RCTRL: return 0x9d;
    case SDL_SCANCODE_KP_DIVIDE: return 0xb5;
    case SDL_SCANCODE_PRINTSCREEN: return 0xb7;      // DIK_SYSRQ
    case SDL_SCANCODE_RALT: return 0xb8;             // DIK_RMENU
    case SDL_SCANCODE_PAUSE: return 0xc5;
    case SDL_SCANCODE_HOME: return 0xc7;
    case SDL_SCANCODE_UP: return 0xc8;
    case SDL_SCANCODE_PAGEUP: return 0xc9;           // DIK_PRIOR
    case SDL_SCANCODE_LEFT: return 0xcb;
    case SDL_SCANCODE_RIGHT: return 0xcd;
    case SDL_SCANCODE_END: return 0xcf;
    case SDL_SCANCODE_DOWN: return 0xd0;
    case SDL_SCANCODE_PAGEDOWN: return 0xd1;         // DIK_NEXT
    case SDL_SCANCODE_INSERT: return 0xd2;
    case SDL_SCANCODE_DELETE: return 0xd3;
    case SDL_SCANCODE_LGUI: return 0xdb;             // DIK_LWIN
    case SDL_SCANCODE_RGUI: return 0xdc;
    case SDL_SCANCODE_APPLICATION: return 0xdd;      // DIK_APPS
    default: return 0;
  }
}

SdlInput::~SdlInput() {
  if (joy_) SDL_CloseJoystick(joy_);
}

bool SdlInput::open(Device d) {
  if (d == kJoystick) {
    if (!joy_) {
      int n = 0;
      SDL_JoystickID* ids = SDL_GetJoysticks(&n);
      if (ids && n > 0) joy_ = SDL_OpenJoystick(ids[0]);  // EnumDevices(DIDEVTYPE_JOYSTICK, attached only): the first
      SDL_free(ids);
    }
    if (!joy_) return false;
  }
  opened_[d] = true;
  return true;
}

void SdlInput::close(Device d) {
  opened_[d] = acquired_[d] = false;
  if (d == kJoystick && joy_) SDL_CloseJoystick(joy_), joy_ = nullptr;
  if (d == kKeyboard) keys_.clear();
}

int32_t SdlInput::acquire(Device d) {
  if (!opened_[d]) return kNotAcquired;
  acquired_[d] = true;
  return 0;
}
int32_t SdlInput::unacquire(Device d) {
  acquired_[d] = false;
  if (d == kKeyboard) keys_.clear();  // a lost device's buffer is gone
  if (d == kMouse) dx_ = dy_ = dz_ = 0;
  return 0;
}

int32_t SdlInput::keyboard_data(platform::KeyEvent* out, uint32_t* count) {
  if (!opened_[kKeyboard] || !acquired_[kKeyboard]) return *count = 0, kNotAcquired;
  uint32_t n = 0;
  while (n < *count && !keys_.empty()) out[n++] = keys_.front(), keys_.pop_front();
  *count = n;
  const bool over = overflow_;
  overflow_ = false;
  return over ? kBufferOverflow : 0;
}

int32_t SdlInput::mouse_state(platform::MouseState* out) {
  if (!opened_[kMouse] || !acquired_[kMouse]) return kNotAcquired;
  out->x = static_cast<int32_t>(dx_), out->y = static_cast<int32_t>(dy_), out->z = static_cast<int32_t>(dz_);
  dx_ -= out->x, dy_ -= out->y, dz_ -= out->z;  // the fractions carry over
  std::memcpy(out->buttons, buttons_, 4);
  return 0;
}

int32_t SdlInput::joystick_state(platform::JoyState* out) {
  if (!joy_ || !opened_[kJoystick]) return kNotAcquired;
  SDL_UpdateJoysticks();  // Poll (IDirectInputDevice2 +0x64)
  *out = platform::JoyState{};
  int32_t* axes[8] = {&out->x, &out->y, &out->z, &out->rx, &out->ry, &out->rz, &out->slider[0], &out->slider[1]};
  const int na = SDL_GetNumJoystickAxes(joy_);
  for (int i = 0; i < 8; ++i)
    *axes[i] = i < na ? static_cast<int32_t>(SDL_GetJoystickAxis(joy_, i)) + 32768 : (kAxisMin + kAxisMax) / 2;
  const int nh = SDL_GetNumJoystickHats(joy_);
  for (int i = 0; i < 4 && i < nh; ++i) {
    static const int32_t kAngles[16] = {-1, 0, 9000, 4500, 18000, -1, 13500, -1,
                                        27000, 31500, -1, -1, 22500, -1, -1, -1};
    const int32_t a = kAngles[SDL_GetJoystickHat(joy_, i) & 15];  // up 1, right 2, down 4, left 8
    out->pov[i] = a < 0 ? 0xffffffffu : static_cast<uint32_t>(a);
  }
  const int nb = SDL_GetNumJoystickButtons(joy_);
  for (int i = 0; i < 128 && i < nb; ++i) out->buttons[i] = SDL_GetJoystickButton(joy_, i) ? 0x80 : 0;
  return acquired_[kJoystick] ? 0 : kNotAcquired;
}

void SdlInput::on_event(const SDL_Event& e) {
  switch (e.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
      const uint8_t dik = dik_from_scancode(e.key.scancode);
      if (!dik) break;
      const bool down = e.type == SDL_EVENT_KEY_DOWN;
      if (down && e.key.repeat) break;  // DirectInput reports a held key once
      held_[dik] = down;
      if (!acquired_[kKeyboard]) break;
      if (keys_.size() >= kBufferSize) {
        overflow_ = true;  // DirectInput keeps the oldest and drops the rest
        break;
      }
      keys_.push_back({dik, down ? 0x80u : 0u, static_cast<uint32_t>(e.key.timestamp / 1000000), sequence_++});
      break;
    }
    case SDL_EVENT_MOUSE_MOTION:
      mouse_x = e.motion.x, mouse_y = e.motion.y;
      if (acquired_[kMouse]) dx_ += e.motion.xrel, dy_ += e.motion.yrel;
      break;
    case SDL_EVENT_MOUSE_WHEEL:
      if (acquired_[kMouse]) dz_ += e.wheel.y * 120.0f;  // WHEEL_DELTA a notch
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
      // DirectInput's order: left, right, middle, then the fourth.
      const int b = e.button.button == SDL_BUTTON_LEFT ? 0 : e.button.button == SDL_BUTTON_RIGHT ? 1
                    : e.button.button == SDL_BUTTON_MIDDLE ? 2 : e.button.button == SDL_BUTTON_X1 ? 3 : -1;
      if (b < 0) break;
      const bool down = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
      buttons_[b] = down ? 0x80 : 0;
      if (b < 3) mouse_buttons = down ? mouse_buttons | (1u << b) : mouse_buttons & ~(1u << b);
      mouse_x = e.button.x, mouse_y = e.button.y;
      break;
    }
    default: break;
  }
}

}  // namespace sdlfe
