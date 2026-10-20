// Movement and physics (mech3/mech_control, mech_move) for tests/differential/test_move*.py.
// State goes back and forth as byte images laid out like the original
// objects; calls into unported code are logged as (count, code, args...).
#include <cstdint>
#include <cstring>

#include <vector>

#include "mech3/mech.h"
#include "mech3/mech_control.h"
#include "mech3/mech_move.h"

using namespace mech3;

namespace {
template <typename T> T rd(const uint8_t* b, int off) {
  T v;
  std::memcpy(&v, b + off, sizeof v);
  return v;
}
template <typename T> void wr(uint8_t* b, int off, T v) { std::memcpy(b + off, &v, sizeof v); }

struct Io {
  uint8_t* b;
  bool store;
  void f(int off, float& v) { store ? wr(b, off, v) : void(v = rd<float>(b, off)); }
  void i(int off, int32_t& v) { store ? wr(b, off, v) : void(v = rd<int32_t>(b, off)); }
  void u(int off, uint32_t& v) { store ? wr(b, off, v) : void(v = rd<uint32_t>(b, off)); }
  void bo(int off, bool& v) { store ? wr<uint8_t>(b, off, v) : void(v = rd<uint8_t>(b, off) != 0); }
  void v3(int off, Vec3& v) { f(off, v.x), f(off + 4, v.y), f(off + 8, v.z); }
};

void controls_io(MechControls& c, uint8_t* b, bool store) {
  Io io{b, store};
  io.u(0x08, c.flags);
  io.f(0x10, c.throttle), io.f(0x14, c.turn), io.f(0x18, c.pitch), io.f(0x1c, c.twist);
  io.f(0x20, c.jump), io.f(0x24, c.jj_strafe), io.f(0x28, c.jj_forward);
  io.f(0x2c, c.axis_2c), io.f(0x30, c.axis_30);
  io.f(0x44, c.throttle_target), io.f(0x48, c.turn_target), io.f(0x4c, c.jump_target);
  io.bo(0x50, c.reverse);
}
std::vector<int32_t> g_log;
void log(std::initializer_list<int32_t> v) {
  g_log.push_back(static_cast<int32_t>(v.size()));
  g_log.insert(g_log.end(), v);
}
int32_t bits(float f) {
  int32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}

// A whole mech for the gait and turn: its parts, controls and animations.
struct Gait {
  Mech m;
  MechControls c;
  Leg legs[2];
  Foot feet[2];
  Vec3 foot_pos[2] = {};
  Torso torso;
  struct Anim : MechAnim {
    int index = 0;
    void set_blend(float w) override { log({4, index, bits(w)}); }
    void seek(int e) override { log({5, index, e}); }
  } anims[13];
};
Gait* g = nullptr;

int anim_index(const MechAnim* a) {
  for (int i = 0; i < 13; ++i)
    if (a == &g->anims[i]) return i;
  return -1;
}

void mech_io(Mech& m, uint8_t* b, bool store) {
  Io io{b, store};
  io.v3(0x3c, m.vec_3c), io.v3(0x58, m.vec_58);
  io.f(0x68, m.height);
  io.bo(0xde, m.powered_down);
  io.f(0x418, m.blend_weight), io.i(0x420, m.anim_index);
  for (int k = 0; k < 13; ++k) io.f(0x4a0 + 8 * k, m.gait[k][0]), io.f(0x4a4 + 8 * k, m.gait[k][1]);
  io.f(0x520, m.turn_rate), io.f(0x528, m.speed_factor), io.f(0x52c, m.speed_target);
  io.f(0x530, m.throttle_up), io.f(0x534, m.throttle_down), io.f(0x538, m.turn_accel);
  io.f(0x554, m.heat);
  io.i(0x578, m.anim_state), io.i(0x57c, m.prev_state);
  io.bo(0x584, m.no_power_down), io.bo(0x586, m.crouched), io.bo(0x588, m.wading);
  io.u(0x58c, m.gait_frames), io.i(0x5ac, m.limp), io.u(0x688, m.detail), io.f(0x6b0, m.frame_dt);
}

void foot_io(Foot& f, Vec3& pos, uint8_t* b, bool store) {
  Io io{b, store};
  io.f(0x8c, f.clearance), io.i(0x94, f.ground_state), io.bo(0xc0, f.pivot), io.f(0xd4, f.pivot_rate);
  io.f(0xdc, f.water_depth), io.v3(0xf0, pos);
}

void anim_io(MechAnim& a, uint8_t* b, bool store) {
  Io io{b, store};
  io.f(0x04, a.rate), io.f(0x08, a.mult), io.f(0x0c, a.stride), io.f(0x54, a.loop_time), io.f(0x88, a.t);
  io.f(0x8c, a.stride_speed);
  for (int k = 0; k < 3; ++k) io.f(0xd0 + 4 * k, a.events[k]);
}
}  // namespace

extern "C" {

// The gait (op 0, 0x4181c0) or the turn (op 1, 0x418fc0) on a whole mech:
// images of the mech (0x6d8), controls (0x74), two feet (0x100, the
// translation at +0xf0), the torso (0x1b4) and 13 animations (0xdc each,
// `present` bit k), plus is_player and the force-feedback object. Returns
// the call log's length in `out`.
int32_t probe_gait(int32_t op, uint8_t* mech, uint8_t* ctl, uint8_t* feet, uint8_t* torso, uint8_t* anims,
                   uint32_t present, int32_t player, int32_t ff, float dt, int32_t* out, int32_t cap) {
  static Gait state;
  g = &state;
  Mech& m = g->m;
  m = Mech{};
  mech_io(m, mech, false);
  controls_io(g->c, ctl, false);
  m.controls = &g->c;
  g->c.owner = &m;
  for (int i = 0; i < 2; ++i) {
    g->legs[i].foot = &g->feet[i];
    m.legs[i] = &g->legs[i];
    g->feet[i].translation = &g->foot_pos[i];
    foot_io(g->feet[i], g->foot_pos[i], feet + 0x100 * i, false);
  }
  { Io io{torso, false}; io.i(0x1b0, g->torso.centring); }
  m.torso = &g->torso;
  for (int k = 0; k < 13; ++k) {
    g->anims[k].index = k;
    anim_io(g->anims[k], anims + 0xdc * k, false);
    m.anims[k] = (present >> k & 1) ? &g->anims[k] : nullptr;
  }
  world().mission_player = player ? &m : nullptr;
  world().stats = ff != 0;
  MoveCalls& c = move_calls();
  c.move = [](Mech&, float d, MechAnim* a, float speed) { log({1, bits(d), anim_index(a), bits(speed)}); };
  c.crouch = [](Mech&, int on) { log({2, on}); };
  c.wading_effect = [](int on) { log({3, on}); };
  g_log.clear();
  if (op == 0) m.select_gait(dt);
  else m.turn(dt);
  mech_io(m, mech, true);
  controls_io(g->c, ctl, true);
  for (int i = 0; i < 2; ++i) foot_io(g->feet[i], g->foot_pos[i], feet + 0x100 * i, true);
  for (int k = 0; k < 13; ++k) anim_io(g->anims[k], anims + 0xdc * k, true);
  world().mission_player = nullptr;
  const int32_t n = static_cast<int32_t>(g_log.size()) < cap ? static_cast<int32_t>(g_log.size()) : cap;
  std::memcpy(out, g_log.data(), 4 * n);
  return n;
}

// MechControls::update (0x426960) on a controls image (0x74 bytes) whose owner
// reads powered_down and the three rates from `owner` (a mech image).
int32_t probe_controls_update(uint8_t* ctl, uint8_t* owner, float dt) {
  static Mech m;
  Io io{owner, false};
  io.bo(0xde, m.powered_down);
  io.f(0x530, m.throttle_up), io.f(0x534, m.throttle_down), io.f(0x538, m.turn_accel);
  MechControls c;
  controls_io(c, ctl, false);
  c.owner = &m;
  const int32_t r = c.update(dt);
  controls_io(c, ctl, true);
  return r;
}

void probe_controls_set_throttle(uint8_t* ctl, float v) {
  MechControls c;
  controls_io(c, ctl, false);
  c.set_throttle(v);
  controls_io(c, ctl, true);
}

}  // extern "C"
