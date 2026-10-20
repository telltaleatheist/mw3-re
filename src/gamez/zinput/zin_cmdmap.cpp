// The command map (docs/input.md section 3): the game's commands, each bound
// to up to two key codes, a joystick button and a mouse button, with a
// callback. Original file unknown: it follows zin_init.cpp (0x0051bb30..
// 0x0051d490).
#include <cstdio>
#include <cstring>

#include "gamez/zinput/zinput.h"
#include "gamez/zutil/zutl_msg.h"

namespace zin {

extern Host* g_host;

namespace {

CmdMap* cur() { return state().map; }
uint32_t& word(int32_t cmd) { return cur()->bindings[cmd]; }

// strncpy into a 0x50-byte name, as the original's fixed buffers hold it.
std::string copy_name(const char* src, size_t n) {
  std::string out;
  for (size_t i = 0; i < n && src[i]; ++i) out += src[i];
  return out;
}

// Mech3.exe 1.2 @ 0x0051c5a0
// A message by its key, or the fallback when Mech3Msg.dll has none.
std::string message_or(const char* key, const char* fallback) {
  if (const int32_t id = zutl::msg_id(key)) {
    const zutl::Messages& m = zutl::messages();
    const char* t = m.get_text ? m.get_text(id) : nullptr;
    return t ? t : "";
  }
  return fallback;
}

// 0x0051c340: what every bound key's callback is: the key's command run.
void key_thunk(int32_t key) {
  const int32_t cmd = key_command(key);
  if (CommandCallback cb = cur()->callbacks[cmd]) cb(cmd);
}

// 0x0051c480's strncat with the count it keeps: less the whole string so far.
void append(char* out, const std::string& s, size_t& n) {
  std::strncat(out, s.c_str(), n);
  n -= std::strlen(out);
}

}  // namespace

// Mech3.exe 1.2 @ 0x0051bb30
// A copy of `from` (the bindings, callbacks and names), or an empty map.
CmdMap* map_new(const CmdMap* from) {
  CmdMap* m = new CmdMap();
  m->owned = 0;
  m->enabled = 1;
  if (from) {
    m->count = from->count;
    m->own_bindings.assign(from->bindings, from->bindings + from->count);
    m->bindings = m->own_bindings.data();
    m->callbacks = from->callbacks;
    m->names.resize(static_cast<size_t>(from->count));
    for (int32_t i = 0; i < from->count; ++i) m->names[i] = from->names[i].substr(0, 0x50);
    map_rebuild(m);
  }
  return m;
}

// Mech3.exe 1.2 @ 0x0051bc20
void map_free(CmdMap* m) {
  m->callbacks.clear();
  m->names.clear();
  m->own_bindings.clear();
}

// Mech3.exe 1.2 @ 0x0051bca0
// The lookup tables made again from the bindings, and every bound command's
// callback put on its keys.
void map_rebuild(CmdMap* m) {
  std::memset(m->table, 0, sizeof m->table);
  clear_key_callbacks();
  for (int32_t i = 1; i < m->count; ++i) {
    const uint32_t b = m->bindings[i];
    m->key1_cmd(b & 0x7ff) = i;
    m->key2_cmd(b >> 11 & 0x7ff) = i;
    m->joy_cmd(b >> 22 & 0xf) = i;
    m->mouse_cmd(b >> 26 & 3) = i;
    if (m->callbacks[i]) {
      CmdMap* saved = state().map;
      state().map = m;  // 0x0051c2b0 runs on this map
      set_callback(i, m->callbacks[i]);
      state().map = saved;
    }
  }
}

// Mech3.exe 1.2 @ 0x0051bd70
// The bindings are the settings store's entry `name` (default "CmdMap"),
// cleared here.
void map_create(CmdMap* m, int32_t count, const char* name) {
  m->count = count;
  if (!name) name = "CmdMap";
  uint32_t* block = g_host->settings_block(name, static_cast<uint32_t>(count) * 4);
  if (!block) {
    m->own_bindings.assign(static_cast<size_t>(count), 0);
    block = m->own_bindings.data();
  }
  m->bindings = block;
  m->callbacks.assign(static_cast<size_t>(count), nullptr);
  m->names.assign(static_cast<size_t>(count), std::string());
  map_clear(m, true);
}

// Mech3.exe 1.2 @ 0x0051bdf0
void map_release(CmdMap* m) {
  m->callbacks.clear();
  m->names.clear();
  m->bindings = nullptr;
}

// Mech3.exe 1.2 @ 0x0051be60
void map_clear(CmdMap* m, bool callbacks) {
  for (int32_t i = 0; i < m->count; ++i) {
    m->bindings[i] = pack(0, 0, 0, 0);
    if (callbacks) m->callbacks[i] = nullptr;
  }
  map_rebuild(m);
}

// Mech3.exe 1.2 @ 0x0051beb0
uint32_t pack(uint32_t key1, uint32_t key2, uint32_t joy, uint32_t mouse) {
  return (((mouse & 3) << 4 | (joy & 0xf)) << 11 | (key2 & 0x7ff)) << 11 | (key1 & 0x7ff);
}

// Mech3.exe 1.2 @ 0x0051bf00, 0x0051bf20, 0x0051bf40, 0x0051bf60
uint32_t key1_of(int32_t cmd) { return word(cmd) & 0x7ff; }
uint32_t key2_of(int32_t cmd) { return word(cmd) >> 11 & 0x7ff; }
uint32_t joy_of(int32_t cmd) { return word(cmd) >> 22 & 0xf; }
uint32_t mouse_of(int32_t cmd) { return word(cmd) >> 26 & 3; }

// Mech3.exe 1.2 @ 0x0051bee0
int32_t key_of(int32_t cmd) {
  const uint32_t k = key1_of(cmd);
  return static_cast<int32_t>(k ? k : key2_of(cmd));
}

// Mech3.exe 1.2 @ 0x0051bf80, 0x0051bf90, 0x0051bfc0, 0x0051bfd0
int32_t key1_command(int32_t key) { return cur()->key1_cmd(static_cast<uint32_t>(key)); }
int32_t key2_command(int32_t key) { return cur()->key2_cmd(static_cast<uint32_t>(key)); }
int32_t joy_command(int32_t b) { return cur()->joy_cmd(static_cast<uint32_t>(b)); }
int32_t mouse_command(int32_t b) { return cur()->mouse_cmd(static_cast<uint32_t>(b)); }

// Mech3.exe 1.2 @ 0x0051bfa0
int32_t key_command(int32_t key) {
  const int32_t c = key1_command(key);
  return c ? c : key2_command(key);
}

// Mech3.exe 1.2 @ 0x0051bfe0
// The key taken from the command that had it, and the command's old key let go.
void bind_key1(uint32_t key, int32_t cmd) {
  CmdMap* m = cur();
  if (key != 0) {
    m->bindings[m->key1_cmd(key)] &= 0xfffff800;
    m->key1_cmd(key) = cmd;
  }
  if (cmd != 0) {
    m->key1_cmd(m->bindings[cmd] & 0x7ff) = 0;
    m->bindings[cmd] = (m->bindings[cmd] & ~0x7ffu) | (key & 0x7ff);
  }
}

// Mech3.exe 1.2 @ 0x0051c040
void bind_key2(uint32_t key, int32_t cmd) {
  CmdMap* m = cur();
  if (key != 0) {
    m->bindings[m->key2_cmd(key)] &= 0xffc007ff;
    m->key2_cmd(key) = cmd;
  }
  if (cmd != 0) {
    m->key2_cmd(m->bindings[cmd] >> 11 & 0x7ff) = 0;
    m->bindings[cmd] = (m->bindings[cmd] & 0xffc007ff) | (key & 0x7ff) << 11;
  }
}

// Mech3.exe 1.2 @ 0x0051c0b0
void bind_joy(uint32_t b, int32_t cmd) {
  CmdMap* m = cur();
  if (b != 0) {
    m->bindings[m->joy_cmd(b)] &= 0xfc3fffff;
    m->joy_cmd(b) = cmd;
  }
  if (cmd != 0) {
    m->joy_cmd(m->bindings[cmd] >> 22 & 0xf) = 0;
    m->bindings[cmd] = (b & 0xf) << 22 | (m->bindings[cmd] & 0xfc3fffff);
  }
}

// Mech3.exe 1.2 @ 0x0051c120
void bind_mouse(uint32_t b, int32_t cmd) {
  CmdMap* m = cur();
  if (b != 0) {
    m->bindings[m->mouse_cmd(b)] &= 0xf3ffffff;
    m->mouse_cmd(b) = cmd;
  }
  if (cmd != 0) {
    m->mouse_cmd(m->bindings[cmd] >> 26 & 3) = 0;
    m->bindings[cmd] = (b & 3) << 26 | (m->bindings[cmd] & 0xf3ffffff);
  }
}

// Mech3.exe 1.2 @ 0x0051c190
void bind(int32_t cmd, const char* name, uint32_t key1, uint32_t key2, uint32_t joy, uint32_t mouse) {
  if (name && *name) cur()->names[cmd] = copy_name(name, 0x4f);
  bind_key1(key1, cmd);
  bind_key2(key2, cmd);
  bind_joy(joy, cmd);
  bind_mouse(mouse, cmd);
}

// Mech3.exe 1.2 @ 0x0051c200
void mouse_callbacks() {
  const MouseInfo* m = mouse_info();
  const uint32_t s[3] = {m->left, m->right, m->middle};
  for (int32_t b = 1; b <= 3; ++b) {
    if (s[b - 1] != kPressed) continue;
    if (CommandCallback cb = cur()->callbacks[mouse_command(b)]) cb(mouse_command(b));
  }
}

// Mech3.exe 1.2 @ 0x0051c270
void joy_callbacks() {
  for (int32_t b = 1; b < 0xb; ++b) {
    if (joy_button(b) != kPressed) continue;
    const int32_t c = joy_command(b);
    if (CommandCallback cb = cur()->callbacks[c]) cb(c);
  }
}

// Mech3.exe 1.2 @ 0x0051c2b0
// Only a command with a key gets its callback; each key takes the first one
// given (0x0051a9c0) and runs whichever command it is bound to now.
uint32_t set_callback(int32_t cmd, CommandCallback cb) {
  const uint32_t k1 = key1_of(cmd), k2 = key2_of(cmd);
  if (k1 == 0 && k2 == 0) return 0;
  cur()->callbacks[cmd] = cb;
  if (k1) set_key_callback(static_cast<int32_t>(k1), key_thunk);
  if (k2) set_key_callback(static_cast<int32_t>(k2), key_thunk);
  return 1;
}

// Mech3.exe 1.2 @ 0x0051c370
// A command's state: its keys', joystick button's and mouse button's, or'ed.
uint32_t command(int32_t cmd) {
  if (!cur()->enabled) return 0;
  uint32_t s = 0;
  const uint32_t k1 = key1_of(cmd), k2 = key2_of(cmd);
  if (k1) s = key_state(static_cast<int32_t>(k1));
  if (k2) s |= key_state(static_cast<int32_t>(k2));
  const uint32_t j = joy_of(cmd);
  if (j != 0 && j < 0xb) s |= joy_button(static_cast<int32_t>(j));
  const MouseInfo* m = mouse_info();
  switch (mouse_of(cmd)) {
    case 1: return s | m->left;
    case 2: return s | m->right;
    case 3: return s | m->middle;
    default: return s;
  }
}

// Mech3.exe 1.2 @ 0x0051c420
uint32_t run_callback(int32_t cmd) {
  if (CommandCallback cb = cur()->callbacks[cmd]) {
    cb(cmd);
    return 1;
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x0051c450
const char* command_name(int32_t cmd, char* out, size_t n) {
  std::strncpy(out, cur()->names[cmd].c_str(), n);
  return out;
}

// Mech3.exe 1.2 @ 0x0051c480
// "Ctrl-Alt-Shift-" and the key's name, localized. Quirk kept: the count
// left drops by the whole text so far at each step (harmless at 0x50).
const char* key_name(uint32_t key, char* out, size_t n) {
  const State& s = state();
  const uint32_t k = key & 0xff;
  if (k >= 0xde || !s.key_named[k]) return "";
  out[0] = 0;
  if (key & kCtrl) append(out, message_or("MSG_KEY_CTRL", "Ctrl-"), n);
  if (key & kAlt) append(out, message_or("MSG_KEY_ALT", "Alt-"), n);
  if (key & kShift) append(out, message_or("MSG_KEY_SHIFT", "Shift-"), n);
  std::strncat(out, s.key_names[k].c_str(), n);
  return out;
}

// Mech3.exe 1.2 @ 0x0051c5d0
const char* joy_button_name(int32_t b, char* out, size_t n) {
  if (!state().joy_named[b]) return "";
  std::strncpy(out, state().joy_names[b].c_str(), n);
  return out;
}

// Mech3.exe 1.2 @ 0x0051c600
const char* mouse_button_name(int32_t b, char* out, size_t n) {
  if (!state().mouse_named[b]) return "";
  std::strncpy(out, state().mouse_names[b].c_str(), n);
  return out;
}

// Mech3.exe 1.2 @ 0x0051c630
void init(int32_t count, const char* name) {
  State& s = state();
  s.base.reset(map_new(nullptr));
  s.map = s.base.get();
  map_create(s.map, count, name);
  make_key_names();
  make_joy_names();
  make_mouse_names();
}

// Mech3.exe 1.2 @ 0x0051c6b0
// The keys' names by DIK code. Quirk kept: Left Alt is looked up as
// MSG_KEY_HOME (falling back to "LMENU"), so where the message exists it is
// shown as Home; Page Up and Page Down fall back to "PAGE UP" / "PAGE DOWN".
void make_key_names() {
  static const struct {
    uint8_t dik;
    const char* key;
    const char* fallback;
  } kNames[] = {
#define K(d, n) {d, "MSG_KEY_" n, n}
      K(0x02, "1"), K(0x03, "2"), K(0x04, "3"), K(0x05, "4"), K(0x06, "5"), K(0x07, "6"), K(0x08, "7"),
      K(0x09, "8"), K(0x0a, "9"), K(0x0b, "0"), K(0x10, "Q"), K(0x11, "W"), K(0x12, "E"), K(0x13, "R"),
      K(0x14, "T"), K(0x15, "Y"), K(0x16, "U"), K(0x17, "I"), K(0x18, "O"), K(0x19, "P"), K(0x1e, "A"),
      K(0x1f, "S"), K(0x20, "D"), K(0x21, "F"), K(0x22, "G"), K(0x23, "H"), K(0x24, "J"), K(0x25, "K"),
      K(0x26, "L"), K(0x2c, "Z"), K(0x2d, "X"), K(0x2e, "C"), K(0x2f, "V"), K(0x30, "B"), K(0x31, "N"),
      K(0x32, "M"), K(0x33, "COMMA"), K(0x34, "PERIOD"), K(0x35, "SLASH"), K(0x2b, "BACKSLASH"),
      K(0x29, "GRAVE"), K(0x1a, "LBRACKET"), K(0x1b, "RBRACKET"), K(0x0c, "MINUS"), K(0x0d, "EQUALS"),
      K(0x0e, "BACK"), K(0x0f, "TAB"), K(0x27, "SEMICOLON"), K(0x28, "APOSTROPHE"), K(0x39, "SPACE"),
      K(0x3a, "CAPITAL"), K(0x2a, "LSHIFT"), K(0x36, "RSHIFT"), K(0x3b, "F1"), K(0x3c, "F2"),
      K(0x3d, "F3"), K(0x3e, "F4"), K(0x3f, "F5"), K(0x40, "F6"), K(0x41, "F7"), K(0x42, "F8"),
      K(0x43, "F9"), K(0x44, "F10"), K(0x57, "F11"), K(0x58, "F12"), K(0x64, "F13"), K(0x65, "F14"),
      K(0x66, "F15"), K(0x45, "NUMLOCK"), K(0x46, "SCROLL"), K(0x4a, "SUBTRACT"), K(0x4e, "ADD"),
      K(0x53, "DECIMAL"), K(0xb5, "DIVIDE"), K(0x37, "MULTIPLY"), K(0x4f, "NUMPAD1"), K(0x50, "NUMPAD2"),
      K(0x51, "NUMPAD3"), K(0x4b, "NUMPAD4"), K(0x4c, "NUMPAD5"), K(0x4d, "NUMPAD6"), K(0x47, "NUMPAD7"),
      K(0x48, "NUMPAD8"), K(0x49, "NUMPAD9"), K(0x52, "NUMPAD0"), K(0x8d, "NUMPADEQUALS"),
      K(0x9c, "NUMPADENTER"), K(0xb3, "NUMPADCOMMA"), K(0xc8, "UP"), K(0xd0, "DOWN"), K(0xcb, "LEFT"),
      K(0xcd, "RIGHT"), K(0x1d, "LCONTROL"), K(0x9d, "RCONTROL"), K(0x1c, "RETURN"), K(0x01, "ESCAPE"),
      K(0x70, "KANA"), K(0x79, "CONVERT"), K(0x7b, "NOCONVERT"), K(0x7d, "YEN"), K(0x90, "CIRCUMFLEX"),
      K(0x91, "AT"), K(0x92, "COLON"), K(0x93, "UNDERLINE"), K(0x94, "KANJI"), K(0x95, "STOP"),
      K(0x96, "AX"), K(0x97, "UNLABELED"), K(0xb7, "SYSRQ"), K(0xb8, "RMENU"),
      {0x38, "MSG_KEY_HOME", "LMENU"}, K(0xc7, "HOME"), K(0xcf, "END"),
      {0xc9, "MSG_KEY_PRIOR", "PAGE UP"}, {0xd1, "MSG_KEY_NEXT", "PAGE DOWN"}, K(0xd2, "INSERT"),
      K(0xd3, "DELETE"), K(0xdb, "LWIN"), K(0xdc, "RWIN"), K(0xdd, "APPS"),
#undef K
  };
  State& s = state();
  for (const auto& k : kNames) {
    s.key_names[k.dik] = message_or(k.key, k.fallback);
    s.key_named[k.dik] = true;
  }
}

// Mech3.exe 1.2 @ 0x0051d020
void make_joy_names() {
  State& s = state();
  for (int i = 1; i <= 10; ++i) {
    char key[64], fallback[64];
    std::snprintf(key, sizeof key, "MSG_JBTN_%d", i);
    std::snprintf(fallback, sizeof fallback, "Button %d", i);
    s.joy_names[i] = message_or(key, fallback);
    s.joy_named[i] = true;
  }
}

// Mech3.exe 1.2 @ 0x0051d080
void make_mouse_names() {
  State& s = state();
  s.mouse_names[1] = message_or("MSG_MBTN_LEFT", "Left");
  s.mouse_names[2] = message_or("MSG_MBTN_RIGHT", "Right");
  s.mouse_names[3] = message_or("MSG_MBTN_MIDDLE", "Middle");
  s.mouse_named[1] = s.mouse_named[2] = s.mouse_named[3] = true;
}

// Mech3.exe 1.2 @ 0x0051d0c0
void shutdown() {
  State& s = state();
  while (s.map && s.map->owned) pop_map();
  for (int i = 0; i < 4; ++i) s.mouse_names[i].clear(), s.mouse_named[i] = false;  // 0x0051d190
  for (int i = 0; i < 11; ++i) s.joy_names[i].clear(), s.joy_named[i] = false;     // 0x0051d160
  for (int i = 0; i < 0xde; ++i) s.key_names[i].clear(), s.key_named[i] = false;   // 0x0051d130
  if (s.map) {
    map_release(s.map);
    map_free(s.map);
    if (s.map == s.base.get()) s.base.reset();
    else delete s.map;
    s.map = nullptr;
  }
}

// Mech3.exe 1.2 @ 0x0051d1c0, 0x0051d1d0
void map_refresh() { map_rebuild(cur()); }
void map_reset(bool callbacks) { map_clear(cur(), callbacks); }

// Mech3.exe 1.2 @ 0x0051d390
CmdMap* current_map() { return state().map; }

// Mech3.exe 1.2 @ 0x0051d3a0
// The current map saved and `m` made current; with none, a copy of the
// current one (deleted when popped). The interp console and the debug
// overlay push their own bindings this way.
void push_map(CmdMap* m) {
  State& s = state();
  if (!m) {
    m = map_new(s.map);
    m->owned = 1;
  }
  s.stack.push_back(s.map);
  s.map = m;
  map_rebuild(m);
}

// Mech3.exe 1.2 @ 0x0051d490
void pop_map() {
  State& s = state();
  CmdMap* m = s.map;
  if (m->owned && m) {
    map_free(m);
    delete m;
  }
  s.map = nullptr;
  if (!s.stack.empty()) {
    s.map = s.stack.back();
    s.stack.pop_back();
  }
  if (s.map) map_rebuild(s.map);
}

}  // namespace zin
