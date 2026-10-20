// The player's and the AI's controls (mech3/mech_control.cpp) for
// tests/differential/test_player.py. The controls and the mech go back and
// forth as byte images laid out like the original's; zInput's commands,
// joystick, mouse and the HUD come from the env words. Calls out are logged
// as (count, code, args...) on both sides.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zinput/zinput.h"
#include "mech3/mech.h"
#include "platform/platform.h"

using namespace mech3;

namespace {
template <typename T> T rd(const uint8_t* b, int off) {
  T v;
  std::memcpy(&v, b + off, sizeof v);
  return v;
}
template <typename T> void wr(uint8_t* b, int off, T v) { std::memcpy(b + off, &v, sizeof v); }
int32_t bits(float f) {
  int32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}
float flt(uint32_t u) {
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}

struct Io {
  uint8_t* b;
  bool store;
  void f(int off, float& v) { store ? wr(b, off, v) : void(v = rd<float>(b, off)); }
  void i(int off, int32_t& v) { store ? wr(b, off, v) : void(v = rd<int32_t>(b, off)); }
  void u32(int off, uint32_t& v) { store ? wr(b, off, v) : void(v = rd<uint32_t>(b, off)); }
  void u8(int off, uint8_t& v) { store ? wr(b, off, v) : void(v = rd<uint8_t>(b, off)); }
  void bo(int off, bool& v) { store ? wr<uint8_t>(b, off, v) : void(v = rd<uint8_t>(b, off) != 0); }
  void h(int off, void*& v) {
    if (store) wr<uint32_t>(b, off, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(v)));
    else v = reinterpret_cast<void*>(static_cast<uintptr_t>(rd<uint32_t>(b, off)));
  }
};

std::vector<int32_t> g_log;
void log(std::initializer_list<int32_t> v) {
  g_log.push_back(static_cast<int32_t>(v.size()));
  g_log.insert(g_log.end(), v);
}

// The env words (test_player.py's ENV).
enum {
  kFlags, kCamView, kScreenFlags, kScreenBusy, kBrain, kAxes, kCommands = kAxes + 16, kJoy = kCommands + 0x40,
  kJoyConfig = kJoy + 0x44, kMouse = kJoyConfig + 12, kEnv = kMouse + 15,
};

struct ControlsWorld {
  Mech m;
  PlayerControls pc;
  AiControls ac;
  uint32_t env[kEnv] = {};
  platform::JoyState joy;
  zin::JoyAxis axes[6];
  zin::MouseInfo mouse;
  int screen = 1;
};
ControlsWorld* g = nullptr;

void ctl_io(MechControls& c, uint8_t* b, bool store) {
  Io io{b, store};
  int32_t view = c.view;
  io.u32(0x08, c.flags), io.i(0x0c, view), c.view = view;
  io.f(0x10, c.throttle), io.f(0x14, c.turn), io.f(0x18, c.pitch), io.f(0x1c, c.twist), io.f(0x20, c.jump);
  io.f(0x24, c.jj_strafe), io.f(0x28, c.jj_forward), io.f(0x2c, c.axis_2c), io.f(0x30, c.axis_30);
  io.u8(0x34, c.fire), io.bo(0x3c, c.joy_throttle_set), io.bo(0x3d, c.jump_held), io.bo(0x3e, c.throttle_held);
  io.bo(0x3f, c.throttle_returns), io.f(0x40, c.joy_throttle), io.f(0x44, c.throttle_target);
  io.f(0x48, c.turn_target), io.f(0x4c, c.jump_target), io.bo(0x50, c.reverse), io.u32(0x54, c.last_pov);
}

void player_io(uint8_t* b, bool store) {
  Io io{b, store};
  PlayerControls& p = g->pc;
  ctl_io(p, b, store);
  io.f(0x138, p.look_dx), io.f(0x13c, p.look_dy), io.f(0x140, p.look_x), io.f(0x144, p.look_y);
}

void mech_io(uint8_t* b, bool store) {
  Io io{b, store};
  Mech& m = g->m;
  io.bo(0xdc, m.power_down_pending), io.bo(0xde, m.powered_down), io.i(0x3d8, m.equipment[11]);
  io.f(0x530, m.throttle_up), io.f(0x534, m.throttle_down), io.f(0x538, m.turn_accel);
  io.bo(0x584, m.no_power_down), io.h(0x614, m.sounds[23]);
}

void hooks() {
  ControlsCalls& c = controls_calls();
  c = ControlsCalls{};
  c.command = [](int cmd) {
    log({61, cmd});
    return g->env[kCommands + cmd];
  };
  c.joy_state = []() -> const void* { return &g->joy; };
  c.joy_config = []() -> const void* { return g->axes; };
  c.mouse_info = []() -> const void* { return &g->mouse; };
  c.joy_present = [] { return (g->env[kFlags] >> 1 & 1) != 0; };
  c.run_command = [](int cmd) { log({66, cmd}); };
  c.mouse_set = [](float x, float y) { log({67, bits(x), bits(y)}); };
  c.camera_view = [] { return static_cast<int>(g->env[kCamView]); };
  c.mw2_jets = [] { return (g->env[kFlags] >> 2 & 1) != 0; };
  c.pilot_tick = [](MechControls&) { log({62}); };
  c.screen_busy = [](void*) { return static_cast<uint8_t>(g->env[kScreenBusy]); };
  c.zoom = [](void*, bool in) { log({65, in ? 1 : 0}); };
  c.ai_brain = [](MechControls&, float dt) {
    log({68, bits(dt)});
    return static_cast<int>(g->env[kBrain]);
  };
  heat_calls().screen = [](Mech&) -> void* {
    log({63});
    return g->env[kFlags] >> 3 & 1 ? &g->screen : nullptr;
  };
  part_calls().screen_flags = [](void*) { return g->env[kScreenFlags]; };
  part_calls().sound = [](Mech&, int id, float vol) -> void* {
    log({64, id, bits(vol), 0});
    return reinterpret_cast<void*>(uintptr_t{0x77});
  };
}

void unhook() {
  controls_calls() = ControlsCalls{};
  heat_calls().screen = nullptr;
  part_calls().screen_flags = nullptr, part_calls().sound = nullptr;
  for (int k = 0; k < 4; ++k) axis_bindings()[k] = AxisBinding{};
  world().player = nullptr;
}
}  // namespace

extern "C" {

void probe_controls_load(const uint8_t* mech, const uint8_t* ctl, const uint32_t* env) {
  if (!g) g = new ControlsWorld;
  ControlsWorld& w = *g;
  w.m = Mech{};
  w.pc = PlayerControls{};
  w.ac = AiControls{};
  std::memcpy(w.env, env, sizeof w.env);
  mech_io(const_cast<uint8_t*>(mech), false);
  player_io(const_cast<uint8_t*>(ctl), false);
  ctl_io(w.ac, const_cast<uint8_t*>(ctl), false);
  w.pc.owner = &w.m, w.ac.owner = &w.m;
  w.m.controls = &w.pc;
  for (int k = 0; k < 4; ++k) {
    AxisBinding& a = axis_bindings()[k];
    a.source = static_cast<int32_t>(env[kAxes + 4 * k]);
    a.scale = flt(env[kAxes + 4 * k + 1]), a.offset = flt(env[kAxes + 4 * k + 2]);
    a.deadzone = flt(env[kAxes + 4 * k + 3]);
  }
  std::memcpy(&w.joy, env + kJoy, sizeof w.joy);
  for (int k = 0; k < 6; ++k) w.axes[k].scale = flt(env[kJoyConfig + 2 * k]), w.axes[k].centre = flt(env[kJoyConfig + 2 * k + 1]);
  std::memcpy(&w.mouse, env + kMouse, sizeof w.mouse);
  world().player = env[kFlags] & 1 ? &w.m : nullptr;
  g_log.clear();
  hooks();
}

// op 0: the player's update(dt); 1: the AI's.
int probe_controls_op(int op, float dt) {
  return op == 0 ? g->pc.update(dt) : g->ac.update(dt);
}

int probe_controls_store(int op, uint8_t* mech, uint8_t* ctl, int32_t* out, int cap) {
  ControlsWorld& w = *g;
  mech_io(mech, true);
  if (op == 0) player_io(ctl, true);
  else ctl_io(w.ac, ctl, true);
  unhook();
  const int n = static_cast<int>(g_log.size());
  if (n > cap) return -1;
  std::memcpy(out, g_log.data(), n * 4);
  return n;
}

}  // extern "C"
