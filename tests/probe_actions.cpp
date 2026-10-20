// The action queue and its state machine (mech3/mech_move: falls, landing
// from a fall, getting up) for tests/differential/test_actions.py. State goes
// back and forth as byte images laid out like the original's; the actions'
// root sample is a fixed function of their time on both sides; every call
// out is logged as (count, code, args...).
#include <cstdint>
#include <cstring>
#include <vector>

#include "mech3/mech.h"
#include "mech3/mech_move.h"

using namespace mech3;

namespace {
template <typename T> T rd(const uint8_t* b, int off) {
  T v;
  std::memcpy(&v, b + off, sizeof v);
  return v;
}
template <typename T> void wr(uint8_t* b, int off, T v) { std::memcpy(b + off, &v, sizeof v); }
Handle h(uint32_t v) { return reinterpret_cast<Handle>(static_cast<uintptr_t>(v)); }
uint32_t u(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
int32_t bits(float f) {
  int32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}

struct Io {
  uint8_t* b;
  bool store;
  void f(int off, float& v) { store ? wr(b, off, v) : void(v = rd<float>(b, off)); }
  void i(int off, int32_t& v) { store ? wr(b, off, v) : void(v = rd<int32_t>(b, off)); }
  void bo(int off, bool& v) { store ? wr<uint8_t>(b, off, v) : void(v = rd<uint8_t>(b, off) != 0); }
  void hd(int off, Handle& v) { store ? wr<uint32_t>(b, off, u(v)) : void(v = h(rd<uint32_t>(b, off))); }
  void v3(int off, Vec3& v) { f(off, v.x), f(off + 4, v.y), f(off + 8, v.z); }
};

std::vector<int32_t> g_log;
void log(std::initializer_list<int32_t> v) {
  g_log.push_back(static_cast<int32_t>(v.size()));
  g_log.insert(g_log.end(), v);
}

struct LogLeg : Leg {
  int index = 0;
  void apply_damage(float d, int section) override { log({11, index, bits(d), section}); }
};
// The root sample stand-in, as test_actions.py's: (1.5 t, 0.25 t + k, -2 t).
struct TestAction : MechAction {
  int kind = 0;
  void hip(float* pp, float q[4]) override {
    Vec3* p = reinterpret_cast<Vec3*>(pp);
    *p = {t * 1.5f, t * 0.25f + static_cast<float>(kind), t * -2.0f};
    q[0] = 1.0f, q[1] = 0.0f, q[2] = t * 0.5f, q[3] = 0.0f;
  }
};

struct State {
  Mech m;
  MechControls c;
  Pilot pilot;
  LogLeg legs[2];
  Foot feet[2];
  Head head;
  Torso torso;
  Hip hip;
  TestAction acts[9];
  Vec3 pos;
  uint32_t next = 0x9000;
  int32_t queue_ok = 0;
};
State* g = nullptr;

void mech_io(Mech& m, uint8_t* b, bool store) {
  Io io{b, store};
  io.bo(0x8, m.active);
  io.v3(0x24, m.forward), io.v3(0x30, m.velocity), io.v3(0x3c, m.vec_3c), io.v3(0x58, m.vec_58);
  io.bo(0xdc, m.power_down_pending), io.bo(0xde, m.powered_down);
  io.v3(0x488, m.momentum), io.v3(0x494, m.action_root);
  io.f(0x53c, m.knock), io.v3(0x540, m.knock_vec);
  io.i(0x578, m.anim_state), io.i(0x57c, m.prev_state);
  io.bo(0x584, m.no_power_down);
  io.f(0x590, m.lie_time);
  io.v3(0x594, m.impulse), io.v3(0x5a0, m.push);
  io.hd(0x600, m.sounds[0x12]), io.hd(0x604, m.sounds[0x13]);
  io.i(0x638, m.hud_fall);
  int32_t d = static_cast<int32_t>(m.detail);
  io.i(0x688, d);
  m.detail = static_cast<uint32_t>(d);
}

void controls_io(MechControls& c, uint8_t* b, bool store) {
  Io io{b, store};
  io.f(0x10, c.throttle), io.f(0x14, c.turn), io.f(0x18, c.pitch), io.f(0x1c, c.twist), io.f(0x20, c.jump);
  io.f(0x24, c.jj_strafe), io.f(0x28, c.jj_forward), io.f(0x2c, c.axis_2c), io.f(0x30, c.axis_30);
  io.f(0x44, c.throttle_target), io.f(0x48, c.turn_target), io.f(0x4c, c.jump_target), io.bo(0x50, c.reverse);
}

void action_io(TestAction& a, uint8_t* b, bool store) {
  Io io{b, store};
  io.f(0x54, a.loop_time), io.f(0x88, a.t), io.bo(0xec, a.done), io.bo(0xed, a.reversed);
}
}  // namespace

extern "C" {

// op 0 update_action(dt), 1 queue_action(arg, arg2), 2 death_fall(arg),
// 3 stagger(knock_vec), 4 get_up(), 5 knocked_down(arg). Images: the mech
// (0x6d8), controls (0x74), nine actions (0xf0 each, `present` bit k), the
// two feet (+0x78 shadow) and the vital parts' destroyed bytes. The queue is
// `queue` (n in *nq, written back). `flags`: 1 player, 2 force feedback, 4
// remote. Returns the function's result.
int32_t probe_actions(int32_t op, uint8_t* mech, uint8_t* ctl, uint8_t* acts, uint32_t present, uint32_t shadow0,
                      uint32_t shadow1, uint32_t destroyed, uint32_t flags, int32_t roster, int32_t last_action,
                      float dt, int32_t arg, int32_t arg2, int32_t* queue, int32_t* nq, int32_t* out, int32_t* n,
                      int32_t* last_out) {
  static State state;
  g = &state;
  Mech& m = g->m;
  m = Mech{};
  mech_io(m, mech, false);
  g->c = MechControls{};
  controls_io(g->c, ctl, false);
  m.controls = &g->c;
  g->c.owner = &m;
  g->pilot.roster = roster;
  m.pilot = &g->pilot;
  for (int i = 0; i < 2; ++i) {
    g->legs[i].index = i;
    g->legs[i].foot = &g->feet[i];
    g->legs[i].destroyed = destroyed >> (1 + i) & 1;
    m.legs[i] = &g->legs[i];
  }
  g->feet[0].shadow = h(shadow0), g->feet[1].shadow = h(shadow1);
  g->head.destroyed = destroyed & 1;
  g->torso.destroyed = destroyed >> 3 & 1;
  m.head = &g->head, m.torso = &g->torso, m.hip = &g->hip;
  for (int k = 0; k < 9; ++k) {
    g->acts[k] = TestAction{};
    g->acts[k].kind = k;
    action_io(g->acts[k], acts + 0xf0 * k, false);
    m.action_objects[k] = (present >> k & 1) ? &g->acts[k] : nullptr;
  }
  m.actions.clear();
  for (int k = 0; k < *nq; ++k) m.actions.push_back(queue[k]);
  m.remote = flags & 4;
  g->pos = {0, 0, 0};
  world().mission_player = flags & 1 ? &m : nullptr;
  world().stats = flags & 2;
  world().last_action = last_action;
  g_log.clear();
  g->next = 0x9000;

  MoveCalls& c = move_calls();
  c.pilot_sound = [](Mech&, int id, float vol, float pitch) -> Sound {
    log({3, id, bits(vol), bits(pitch)});
    return h(g->next++);
  };
  c.skis_update = [](Mech&, float d) {
    log({15, bits(d)});
    return false;
  };
  c.controls_landed = nullptr;
  c.weapons_enable = [](Mech&, int on) { log({18, on}); };
  c.fall_camera = [](Mech&) { log({19}); };
  c.restore_camera = [](Mech&) { log({20}); };
  c.action_dust = [](Mech&) { log({21}); };
  c.action_pose = [](Mech&, MechAction&, const Vec3& hip, const float q[4]) {
    log({17, bits(hip.x), bits(hip.y), bits(hip.z), bits(q[0]), bits(q[1]), bits(q[2]), bits(q[3])});
  };
  DamageCalls& dc = damage_calls();
  dc.anim_ok = nullptr;  // the real queue
  dc.net_event = [](Mech&, int kind, intptr_t a) { log({22, kind, static_cast<int32_t>(a)}); };
  dc.node_set_active = [](Handle node, int on) { log({23, static_cast<int32_t>(u(node)), on}); };
  dc.hip_reset = [](MechPart&, const Vec3*) { log({13}); };
  dc.stats = [](uint32_t fn, int) { log({14, static_cast<int32_t>(fn)}); };

  int32_t r = 0;
  switch (op) {
    case 0: m.update_action(dt); break;
    case 1: r = m.queue_action(arg, arg2 != 0); break;
    case 2: r = m.death_fall(arg); break;
    case 3: m.stagger(m.knock_vec); break;
    case 4: m.get_up(); break;
    default: r = m.knocked_down(arg); break;
  }
  mech_io(m, mech, true);
  controls_io(g->c, ctl, true);
  for (int k = 0; k < 9; ++k) action_io(g->acts[k], acts + 0xf0 * k, true);
  int32_t k = 0;
  for (int32_t v : m.actions) queue[k++] = v;
  *nq = k;
  *last_out = world().last_action;
  world().mission_player = nullptr;
  dc.net_event = nullptr, dc.node_set_active = nullptr, dc.hip_reset = nullptr, dc.stats = nullptr;
  const int32_t len = static_cast<int32_t>(g_log.size());
  std::memcpy(out, g_log.data(), 4 * (len < *n ? len : *n));
  *n = len;
  return r;
}

}  // extern "C"
