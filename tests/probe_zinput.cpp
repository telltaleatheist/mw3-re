// zInput for test_zinput.py: every DirectInput, Win32 and message call goes
// to one Python callback, which answers both sides from the same script; the
// state comes back as the original's globals would read (zin_dump).
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zerror/zerr.h"
#include "gamez/zinput/zinput.h"
#include "gamez/zutil/zutl_msg.h"

namespace {

using Cb = int32_t (*)(int32_t op, int32_t a, int32_t b, void* buf, uint32_t* io);
Cb g_cb = nullptr;

enum Op {
  kOpen = 1, kClose, kAcquire, kUnacquire, kCoop, kKbd, kMouseState, kJoy, kCaps, kObjName, kSetRange,
  kGetRange, kDeadzone,
  kC2S = 20, kS2C, kSetCursor, kClientSize, kMapVk, kKbState, kToAscii,
  kMsgId = 30, kMsgText,
  kCmdCb = 40, kCharCb, kReport,
};

struct FakeInput : platform::Input {
  bool open(Device d) override { return g_cb(kOpen, d, 0, nullptr, nullptr) != 0; }
  void close(Device d) override { g_cb(kClose, d, 0, nullptr, nullptr); }
  int32_t acquire(Device d) override { return g_cb(kAcquire, d, 0, nullptr, nullptr); }
  int32_t unacquire(Device d) override { return g_cb(kUnacquire, d, 0, nullptr, nullptr); }
  void set_cooperative_level(Device d, uint32_t f) override { g_cb(kCoop, d, static_cast<int32_t>(f), nullptr, nullptr); }
  int32_t keyboard_data(platform::KeyEvent* out, uint32_t* count) override { return g_cb(kKbd, 0, 0, out, count); }
  int32_t mouse_state(platform::MouseState* out) override { return g_cb(kMouseState, 0, 0, out, nullptr); }
  int32_t joystick_state(platform::JoyState* out) override { return g_cb(kJoy, 0, 0, out, nullptr); }
  int32_t joystick_caps(uint32_t* flags, uint32_t* axes) override {
    uint32_t io[2] = {};
    const int32_t r = g_cb(kCaps, 0, 0, nullptr, io);
    *flags = io[0], *axes = io[1];
    return r;
  }
  int32_t joystick_object_name(uint32_t ofs, char* name, uint32_t) override {
    return g_cb(kObjName, static_cast<int32_t>(ofs), 0, name, nullptr);
  }
  int32_t joystick_set_range(uint32_t ofs, int32_t mn, int32_t mx) override {
    uint32_t io[2] = {static_cast<uint32_t>(mn), static_cast<uint32_t>(mx)};
    return g_cb(kSetRange, static_cast<int32_t>(ofs), 0, nullptr, io);
  }
  int32_t joystick_get_range(uint32_t ofs, int32_t* mn, int32_t* mx) override {
    uint32_t io[2] = {};
    const int32_t r = g_cb(kGetRange, static_cast<int32_t>(ofs), 0, nullptr, io);
    *mn = static_cast<int32_t>(io[0]), *mx = static_cast<int32_t>(io[1]);
    return r;
  }
  int32_t joystick_set_deadzone(uint32_t ofs, uint32_t dz) override {
    return g_cb(kDeadzone, static_cast<int32_t>(ofs), static_cast<int32_t>(dz), nullptr, nullptr);
  }
};

struct FakeHost : zin::Host {
  void client_to_screen(int32_t* x, int32_t* y) override {
    uint32_t io[2] = {static_cast<uint32_t>(*x), static_cast<uint32_t>(*y)};
    g_cb(kC2S, 0, 0, nullptr, io);
    *x = static_cast<int32_t>(io[0]), *y = static_cast<int32_t>(io[1]);
  }
  void screen_to_client(int32_t* x, int32_t* y) override {
    uint32_t io[2] = {static_cast<uint32_t>(*x), static_cast<uint32_t>(*y)};
    g_cb(kS2C, 0, 0, nullptr, io);
    *x = static_cast<int32_t>(io[0]), *y = static_cast<int32_t>(io[1]);
  }
  void set_cursor_pos(int32_t x, int32_t y) override { g_cb(kSetCursor, x, y, nullptr, nullptr); }
  void client_size(int32_t* w, int32_t* h) override {
    uint32_t io[2] = {};
    g_cb(kClientSize, 0, 0, nullptr, io);
    *w = static_cast<int32_t>(io[0]), *h = static_cast<int32_t>(io[1]);
  }
  uint32_t map_virtual_key(uint32_t scan) override {
    return static_cast<uint32_t>(g_cb(kMapVk, static_cast<int32_t>(scan), 0, nullptr, nullptr));
  }
  bool keyboard_state(uint8_t* st) override { return g_cb(kKbState, 0, 0, st, nullptr) != 0; }
  int32_t to_ascii(uint32_t vk, uint32_t scan, const uint8_t* st, uint16_t* out) override {
    uint32_t io[1] = {};
    const int32_t r = g_cb(kToAscii, static_cast<int32_t>(vk), static_cast<int32_t>(scan),
                           const_cast<uint8_t*>(st), io);
    *out = static_cast<uint16_t>(io[0]);
    return r;
  }
};

FakeInput g_input;
FakeHost g_host;
char g_text[0x200];

int32_t msg_id(const char* name) { return g_cb(kMsgId, 0, 0, const_cast<char*>(name), nullptr); }
const char* msg_text(int32_t id) {
  g_text[0] = 0;
  return g_cb(kMsgText, id, 0, g_text, nullptr) ? g_text : nullptr;
}
// Command callbacks, eight, told apart by their index; one char callback.
template <int N>
void cmd_cb(int32_t cmd) { g_cb(kCmdCb, N, cmd, nullptr, nullptr); }
const zin::CommandCallback kCmd[8] = {cmd_cb<0>, cmd_cb<1>, cmd_cb<2>, cmd_cb<3>,
                                      cmd_cb<4>, cmd_cb<5>, cmd_cb<6>, cmd_cb<7>};
void char0(int32_t ch, int32_t key, uint32_t ctx) {
  uint32_t io[1] = {ctx};
  g_cb(kCharCb, ch, key, nullptr, io);
}

uint32_t cmd_id(zin::CommandCallback c) {
  if (!c) return 0;
  for (int i = 0; i < 8; ++i)
    if (kCmd[i] == c) return 0x100 + static_cast<uint32_t>(i);
  return 0xdead;
}

uint32_t f2u(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}
float u2f(uint32_t u) {
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}

}  // namespace

extern "C" {

void zin_setup(Cb cb, int with_di) {
  g_cb = cb;
  zin::reset_state();
  zin::set_input(with_di ? &g_input : nullptr);
  zin::set_host(&g_host);
  zutl::messages().get_id = msg_id;
  zutl::messages().get_text = msg_text;
  zerr::set_sink([](int, const char*, int line, const char* name) {
    g_cb(kReport, 0, line, const_cast<char*>(name), nullptr);
  });
}

// One call into zInput, by the same numbers test_zinput.py uses.
uint32_t zin_op(int32_t op, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, const char* s, char* out) {
  using namespace zin;
  switch (op) {
    case 0: return static_cast<uint32_t>(start(a, b));
    case 1: return static_cast<uint32_t>(stop());
    case 2: reset(); return 0;
    case 3: suspend(); return 0;
    case 4: resume(); return 0;
    case 5: return kbd_use();
    case 6: return joy_use();
    case 7: return joy_unuse();
    case 8: return mouse_use();
    case 9: return mouse_unuse();
    case 10: poll(a != 0); return 0;
    case 11: return key_state(static_cast<int32_t>(a));
    case 12: set_char_callback(a ? char0 : nullptr, b); return 0;
    case 13: return read_key(false);
    case 14: return static_cast<uint32_t>(key_by_name(s));
    case 15: mouse_set(u2f(a), u2f(b)); return 0;
    case 16: mouse_from_cursor(static_cast<int32_t>(a), static_cast<int32_t>(b)); return 0;
    case 17: mouse_centre(); return 0;
    case 18: mouse_set_screen(static_cast<int32_t>(a), static_cast<int32_t>(b)); return 0;
    case 19: mouse_set_accel(u2f(a), u2f(b)); return 0;
    case 20: {
      MouseInfo m;
      const int32_t r = mouse_copy(&m);
      std::memcpy(out, &m, sizeof m);
      return static_cast<uint32_t>(r);
    }
    case 21: return set_mode(a);
    case 22: return static_cast<uint32_t>(mouse_wait_button(false));
    case 23: mouse_reset(); return 0;
    case 24: init(static_cast<int32_t>(a), s); return 0;
    case 25: bind(static_cast<int32_t>(a), s, b, c, d, e); return a;
    case 26: return set_callback(static_cast<int32_t>(a), b ? kCmd[b & 7] : nullptr);
    case 27: return command(static_cast<int32_t>(a));
    case 28: return run_callback(static_cast<int32_t>(a));
    case 29: out[0] = 0; command_name(static_cast<int32_t>(a), out, b); return 0;
    case 30: out[0] = 0; std::strcpy(out + 0x100, key_name(a, out, b)); return 0;
    case 31: out[0] = 0; std::strcpy(out + 0x100, joy_button_name(static_cast<int32_t>(a), out, b)); return 0;
    case 32: out[0] = 0; std::strcpy(out + 0x100, mouse_button_name(static_cast<int32_t>(a), out, b)); return 0;
    case 33: map_refresh(); return 0;
    case 34: map_reset(a != 0); return 0;
    case 35: push_map(nullptr); return 0;
    case 36: pop_map(); return 0;
    case 37: shutdown(); return 0;
    case 38: return static_cast<uint32_t>(key_of(static_cast<int32_t>(a)));
    case 39: return static_cast<uint32_t>(key1_command(static_cast<int32_t>(a)));
    case 40: return joy_button(static_cast<int32_t>(a));
    case 41: return static_cast<uint32_t>(joy_wait_button(false));
    case 42: joy_reset(); return 0;
    case 43: return force_feedback();
    case 44: return mouse_button(static_cast<int32_t>(a));
    case 45: return joy_present() ? 1 : 0;
    case 46: return key_char(a);
    case 47: return static_cast<uint32_t>(key2_command(static_cast<int32_t>(a)));
    case 48: return static_cast<uint32_t>(joy_command(static_cast<int32_t>(a)));
    case 49: return static_cast<uint32_t>(mouse_command(static_cast<int32_t>(a)));
    default: return 0xffffffff;
  }
}

// The state in the original's terms (test_zinput.py's dump() reads the same
// from the emulator): a list of dwords.
int32_t zin_dump(uint32_t* o) {
  const zin::State& s = zin::state();
  uint32_t* p = o;
  *p++ = s.window;
  *p++ = s.started;
  *p++ = s.kbd_flags | s.joy_flags << 8 | s.mouse_flags << 16;
  *p++ = s.kbd_users | static_cast<uint32_t>(s.joy_users) << 16;
  *p++ = s.mouse_users;
  *p++ = s.kbd_ready;
  *p++ = s.event_count;
  *p++ = s.pressed_capacity;
  *p++ = s.modifiers;
  *p++ = s.char_callback ? 1 : 0;
  *p++ = s.char_ctx;
  for (const zin::KeySlot& k : s.keys) *p++ = k.state, *p++ = k.callback ? 1 : 0;
  for (uint32_t i = 0; i < s.event_count && i < s.events.size(); ++i) *p++ = s.events[i].ofs, *p++ = s.events[i].data;
  *p++ = s.joy_ready;
  const auto joy = [&](const platform::JoyState& j) {
    std::memcpy(p, &j, sizeof j);
    p += sizeof j / 4;
  };
  joy(s.joy);
  joy(s.joy_prev);
  *p++ = s.joy_axes, *p++ = s.ff, *p++ = s.ff_attack, *p++ = s.ff_fade;
  std::memcpy(p, s.axes, sizeof s.axes);
  p += sizeof s.axes / 4;
  *p++ = s.mouse_ready;
  std::memcpy(p, &s.mouse_cur, 16), p += 4;
  std::memcpy(p, &s.mouse_prev, 16), p += 4;
  *p++ = static_cast<uint32_t>(s.centre_y);
  *p++ = f2u(s.scale_x), *p++ = f2u(s.scale_y), *p++ = f2u(s.scale_z);
  *p++ = static_cast<uint32_t>(s.centre_z), *p++ = static_cast<uint32_t>(s.centre_x);
  *p++ = s.mouse_acquired;
  *p++ = static_cast<uint32_t>(s.width), *p++ = static_cast<uint32_t>(s.height);
  std::memcpy(p, &s.mouse, sizeof s.mouse), p += sizeof s.mouse / 4;
  *p++ = f2u(s.accel_x), *p++ = f2u(s.accel_y), *p++ = s.mode, *p++ = static_cast<uint32_t>(s.mouse_result);
  *p++ = static_cast<uint32_t>(s.stack.size());
  if (const zin::CmdMap* m = s.map) {
    *p++ = 1, *p++ = m->owned, *p++ = static_cast<uint32_t>(m->count), *p++ = m->enabled;
    for (int32_t i = 0; i < m->count; ++i) *p++ = m->bindings[i], *p++ = cmd_id(m->callbacks[i]);
    for (int32_t v : m->table) *p++ = static_cast<uint32_t>(v);
  } else {
    *p++ = 0;
  }
  return static_cast<int32_t>(p - o);
}

// A command's name, a key's, a joystick or mouse button's (which 0..3).
void zin_name(int32_t which, int32_t i, char* out) {
  const zin::State& s = zin::state();
  out[0] = 0;
  if (which == 0 && s.map && i < s.map->count) std::strcpy(out, s.map->names[i].c_str());
  if (which == 1 && s.key_named[i]) std::strcpy(out, s.key_names[i].c_str());
  if (which == 2 && s.joy_named[i]) std::strcpy(out, s.joy_names[i].c_str());
  if (which == 3 && s.mouse_named[i]) std::strcpy(out, s.mouse_names[i].c_str());
  if (which == 1 && !s.key_named[i]) std::strcpy(out, "<none>");
}

}  // extern "C"
