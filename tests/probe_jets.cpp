// Jump jets, AddJumpJet and the landing (mech3/mech_move) for
// tests/differential/test_jets.py. The mech and its controls go back and
// forth as byte images laid out like the original's; every call out is
// logged as (count, code, args...) on both sides.
#include <cstdint>
#include <cstring>
#include <vector>

#include "crt/msvc_rand.h"
#include "mech3/mech.h"
#include "mech3/mech_config.h"
#include "mech3/mech_move.h"
#include "mech3/mission_world.h"

namespace mech3 {
void set_mw2_jump_jets(int32_t on);  // options.h
}

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
struct LogAnim : MechAnim {
  void seek(int e) override { log({7, e}); }
};
// The gait animation Move plays: logs its calls, its velocity is scripted.
struct GaitAnim : MechAnim {
  float vz = 0;
  void set_speed(float s) override { log({33, bits(s)}); }
  void advance(float d) override { log({34, bits(d)}); }
  void velocity(Vec3* v) override {
    log({35});
    v->z = vz;
  }
};

struct State {
  Mech m;
  MechControls c;
  Pilot pilot;
  Config config{0, "test"};
  Skis skis;
  LogLeg legs[2];
  Foot feet[2];
  LogAnim pose;
  GaitAnim gait;
  Vec3 pos;
  uint32_t next = 0x9000;
  int32_t sky = 0, queue_ok = 0;
};
State* g = nullptr;

void mech_io(Mech& m, uint8_t* b, bool store) {
  Io io{b, store};
  io.v3(0x24, m.forward), io.v3(0x3c, m.vec_3c);
  io.f(0x68, m.height);
  io.hd(0x3a8, m.jet_node);
  io.i(0x3d8, m.equipment[11]);
  io.v3(0x488, m.momentum);
  io.f(0x518, m.max_speed);
  io.f(0x53c, m.knock), io.v3(0x540, m.knock_vec);
  io.f(0x560, m.jet_burn), io.f(0x564, m.jet_fuel), io.bo(0x568, m.infinite_fuel);
  io.i(0x578, m.anim_state), io.i(0x57c, m.prev_state), io.i(0x580, m.jump_jets_firing);
  io.bo(0x584, m.no_power_down);
  int32_t frames = static_cast<int32_t>(m.gait_frames);
  io.i(0x58c, frames);
  m.gait_frames = static_cast<uint32_t>(frames);
  io.f(0x6c4, m.knockdown[3]), io.f(0x6c8, m.knockdown[4]);
  io.v3(0x594, m.impulse);
  io.hd(0x5c4, m.sounds[3]);
  io.hd(0x620, m.jet_effect);
  int32_t d = static_cast<int32_t>(m.detail);
  io.i(0x688, d);
  m.detail = static_cast<uint32_t>(d);
}

void controls_io(MechControls& c, uint8_t* b, bool store) {
  Io io{b, store};
  io.f(0x10, c.throttle), io.f(0x20, c.jump), io.f(0x24, c.jj_strafe), io.f(0x28, c.jj_forward);
  io.f(0x44, c.throttle_target), io.bo(0x50, c.reverse);
}
}  // namespace

extern "C" {

void probe_jets_seed(uint32_t seed) { crt::msvc_srand(seed); }

// op 3 move(dt, anim (arg 1) or none, speed) with the anim's scripted velocity vz.
// op 0 jump_jets(dt), 1 ground_step(dt), 2 add_jump_jet(arg), on a mech
// image (0x6d8) and controls (0x74). `flags`: 1 player, 2 force feedback,
// 4 MW2 jets option, 8 remote, 16 the jump pose exists, 32 the skis report
// airborne, 64 the fall's queue succeeds, 128 the view's player. Returns the function's result.
int32_t probe_jets(int32_t op, uint8_t* mech, uint8_t* ctl, uint32_t flags, int32_t roster, float pos_y,
                   float ground6, float gravity, float dt, int32_t arg, float speed, float vz, int32_t* out,
                   int32_t* n) {
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
  g->config.mech = roster;
  m.config = &g->config;
  g->skis = Skis{};
  g->skis.ground[6] = ground6;
  m.skis = &g->skis;
  for (int i = 0; i < 2; ++i) {
    g->legs[i].index = i;
    g->legs[i].foot = &g->feet[i];
    m.legs[i] = &g->legs[i];
  }
  Hip hip;
  m.hip = &hip;
  m.anims[12] = flags & 16 ? &g->pose : nullptr;
  m.remote = flags & 8;
  g->pos = {0, pos_y, 0};
  g->sky = flags & 32 ? 1 : 0;
  g->queue_ok = flags & 64 ? 1 : 0;
  world().mission_player = flags & 1 ? &m : nullptr;
  world().stats = flags & 2;
  world().player = flags & 128 ? &m : nullptr;
  g->gait.vz = vz;
  set_mw2_jump_jets(flags & 4 ? 1 : 0);
  environment().gravity = gravity;
  g_log.clear();
  g->next = 0x9000;

  HeatCalls& hc = heat_calls();
  hc.stop_sound = [](Sound s) { log({1, static_cast<int32_t>(u(s))}); };
  MoveCalls& c = move_calls();
  c.stop_effect = [](Handle e) { log({2, static_cast<int32_t>(u(e))}); };
  c.pilot_sound = [](Mech&, int id, float vol, float pitch) -> Sound {
    log({3, id, bits(vol), bits(pitch)});
    return h(g->next++);
  };
  c.fade_sound = [](Sound s) -> Sound {
    log({4, static_cast<int32_t>(u(s))});
    return nullptr;
  };
  c.pilot_effect = [](Mech&, int slot, Handle node) -> Handle {
    log({5, slot, static_cast<int32_t>(u(node))});
    return h(g->next++);
  };
  c.ff_jets = [](int on) { log({6, on}); };
  c.jet_pose = [](Mech&) { log({8}); };
  c.landing_dust = [](Mech&) { log({9}); };
  c.landing_pose = [](Mech&) { log({10}); };
  c.skis_update = [](Mech&, float d) {
    log({15, bits(d)});
    return g->sky != 0;
  };
  c.ff_landing = [](float j) { log({16, bits(j)}); };
  c.foot_ground = [](Foot& f) { log({31, &f == &g->feet[1] ? 1 : 0}); };
  c.gait_pose = [](Mech&, MechAnim&, float b) { log({30, bits(b)}); };
  hc.play_mech_sound = [](int i) { log({32, i}); };
  skis_calls().position = [](Mech&) -> const Vec3* { return &g->pos; };
  DamageCalls& dc = damage_calls();
  dc.anim_ok = [](Mech&, int kind, int force) {
    log({12, kind, force});
    return g->queue_ok != 0;
  };
  dc.hip_reset = [](MechPart&, const Vec3*) { log({13}); };
  dc.stats = [](uint32_t fn, int) { log({14, static_cast<int32_t>(fn)}); };

  int32_t r = 0;
  if (op == 0) r = m.jump_jets(dt);
  else if (op == 1) m.ground_step(dt);
  else if (op == 2) m.add_jump_jet(arg != 0);
  else m.move(dt, arg ? &g->gait : nullptr, speed);
  mech_io(m, mech, true);
  controls_io(g->c, ctl, true);
  world().mission_player = nullptr;
  world().player = nullptr;
  const int32_t len = static_cast<int32_t>(g_log.size());
  std::memcpy(out, g_log.data(), 4 * (len < *n ? len : *n));
  *n = len;
  return r;
}

}  // extern "C"
