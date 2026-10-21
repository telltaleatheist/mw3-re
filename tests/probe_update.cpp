// Mech::Update (mech3/mech_update.cpp) for tests/differential/test_update.py.
// The mech, its controls and torso go back and forth as byte images laid out
// like the original's; every call out is logged as (count, code, args...)
// on both sides.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zentity/zent_manager.h"
#include "mech3/mech.h"
#include "mech3/mech_config.h"
#include "mech3/mech_move.h"
#include "mech3/mech_reader.h"

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
  void u32(int off, uint32_t& v) { store ? wr(b, off, v) : void(v = rd<uint32_t>(b, off)); }
  void bo(int off, bool& v) { store ? wr<uint8_t>(b, off, v) : void(v = rd<uint8_t>(b, off) != 0); }
  void u8(int off, uint8_t& v) { store ? wr<uint8_t>(b, off, v) : void(v = rd<uint8_t>(b, off)); }
  void hd(int off, Handle& v) { store ? wr<uint32_t>(b, off, u(v)) : void(v = h(rd<uint32_t>(b, off))); }
  void v3(int off, Vec3& v) { f(off, v.x), f(off + 4, v.y), f(off + 8, v.z); }
};

std::vector<int32_t> g_log;
void log(std::initializer_list<int32_t> v) {
  g_log.push_back(static_cast<int32_t>(v.size()));
  g_log.insert(g_log.end(), v);
}

struct LogPart : Torso {
  int code = 0;
  void update(float dt) override { log({code, bits(dt)}); }
};
struct LogLeg : Leg {
  int code = 0;
  void update(float dt) override { log({code, bits(dt)}); }
};
struct LogArm : Arm {
  int code = 0;
  void update(float dt) override { log({code, bits(dt)}); }
};
struct LogHip : Hip {
  void update(float dt) override { log({24, bits(dt)}); }
};

struct State {
  Mech m;
  MechControls c;
  MechData pilot;
  Skis skis;
  LogPart torso;
  LogHip hip;
  LogLeg legs[2];
  Foot feet[2];
  LogArm arms[2];
  MechDfn dfn{0, nullptr};
  Vec3 pos;
  float transform[12];
  uint32_t next = 0x9000, key = 0, torso_addr = 0, hit_raw = 0;
  int32_t go = 1, held = 0;
  uint8_t blocked[4] = {};
  bool weapons = false, screen = false;
  int32_t view_mode = -1;
};
State* g = nullptr;

void mech_io(Mech& m, uint8_t* b, bool store) {
  Io io{b, store};
  io.hd(0x4, m.node);
  io.bo(0x8, m.active);
  io.v3(0x24, m.forward), io.v3(0x30, m.velocity), io.v3(0x3c, m.vec_3c);
  for (int k = 0; k < 4; ++k) io.f(0x48 + 4 * k, m.orientation[k]);
  io.v3(0x58, m.vec_58);
  io.f(0x68, m.height);
  io.bo(0xdc, m.power_down_pending), io.bo(0xdd, m.heat_shutdown), io.bo(0xde, m.powered_down);
  io.f(0xe0, m.last_hit_time);
  for (int k = 0; k < 4; ++k) io.bo(0x374 + k, m.blocked[k]);
  io.f(0x414, m.alive_time);
  io.f(0x528, m.speed_factor);
  io.f(0x53c, m.knock), io.v3(0x540, m.knock_vec);
  io.f(0x54c, m.lean);
  io.f(0x570, m.narc_time);
  io.i(0x578, m.anim_state), io.i(0x57c, m.prev_state), io.i(0x580, m.jump_jets_firing);
  io.bo(0x584, m.no_power_down);
  io.f(0x590, m.lie_time);
  io.v3(0x594, m.impulse), io.v3(0x5a0, m.push);
  io.i(0x5ac, m.limp);
  io.bo(0x5b0, m.collides);
  for (int k = 0; k < 25; ++k) io.hd(0x5b8 + 4 * k, m.sounds[k]);
  io.hd(0x68c, m.x68c);
  io.u32(0x688, m.detail);
  io.f(0x6b0, m.frame_dt);
  // the part as the emulator's address: the torso's, else what came in
  uint32_t part = m.last_hit_part == g->m.torso ? g->torso_addr : g->hit_raw;
  io.u32(0xe4, part);
  if (!store) {
    g->hit_raw = part;
    m.last_hit_part = part == g->torso_addr ? g->m.torso : nullptr;
  }
}

void controls_io(MechControls& c, uint8_t* b, bool store) {
  Io io{b, store};
  io.u32(0x8, c.flags);
  io.i(0xc, c.view);
  io.f(0x10, c.throttle), io.f(0x14, c.turn), io.f(0x18, c.pitch), io.f(0x1c, c.twist);
  io.f(0x2c, c.axis_2c), io.f(0x30, c.axis_30);
  io.u8(0x34, c.fire);
  io.i(0x38, c.wrapped);
  io.f(0x44, c.throttle_target), io.f(0x48, c.turn_target);
  io.bo(0x50, c.reverse);
  io.bo(0x58, c.moves[0]), io.bo(0x59, c.moves[1]);
}

void torso_io(Torso& t, uint8_t* b, bool store) {
  Io io{b, store};
  io.i(0x9c, t.aim);
  io.f(0xbc, t.twist_in), io.f(0xc0, t.pitch_in), io.f(0xc4, t.look_yaw), io.f(0xc8, t.look_pitch);
}
}  // namespace

extern "C" {

// One Mech::Update(dt). `budget` is (updating, resume, current, left,
// per_frame), in and out. `flags`: 1 the view's player, 2 the mission's
// player, 4 remote, 8 view mode 1, 16 weapons changed, 32 a screen.
// `skis`: slid, surface[1], surface[4]. `scripted`: the key state, the
// controls' Update result, Collide's result, its four blocked bytes.
void probe_update(uint8_t* mech, uint8_t* ctl, uint8_t* torso, uint32_t torso_addr, int32_t* budget,
                  uint32_t flags, int32_t actions, float slid, float surf1, float surf4, int32_t roster,
                  float lean_angle, float lean_factor, float time, float floor, uint32_t key, int32_t go,
                  int32_t held, uint32_t blocked, float* transform, float dt, int32_t* dead_updates,
                  int32_t* out, int32_t* n) {
  static State state;
  g = &state;
  g->torso_addr = torso_addr;
  Mech& m = g->m;
  m = Mech{};
  m.torso = &g->torso;
  mech_io(m, mech, false);
  g->c = MechControls{};
  controls_io(g->c, ctl, false);
  m.controls = &g->c;
  g->c.owner = &m;
  g->pilot = MechData{};
  g->pilot.roster = roster;
  m.pilot = &g->pilot;
  g->skis = Skis{};
  g->skis.slid = slid != 0.0f;
  g->skis.surface[1] = surf1;
  g->skis.surface[4] = surf4;
  m.skis = &g->skis;
  g->torso = LogPart{};
  g->torso.code = 23;
  torso_io(g->torso, torso, false);
  m.hip = &g->hip;
  for (int i = 0; i < 2; ++i) {
    g->legs[i] = LogLeg{};
    g->legs[i].code = 25 + i;
    g->legs[i].foot = &g->feet[i];
    m.legs[i] = &g->legs[i];
    g->arms[i] = LogArm{};
    g->arms[i].code = 27 + i;
    m.arms[i] = &g->arms[i];
  }
  m.actions.clear();
  for (int i = 0; i < actions; ++i) m.actions.push_back(1);
  m.remote = flags & 4;
  g->dfn.put(0x154, lean_angle);
  g->dfn.put(0x158, lean_factor);
  std::memcpy(g->transform, transform, 48);
  m.transform = g->transform;
  g->pos = {1.5f, 2.5f, 3.5f};
  g->key = key, g->go = go, g->held = held;
  for (int k = 0; k < 4; ++k) g->blocked[k] = blocked >> (8 * k) & 0xff;
  g->weapons = flags & 16;
  g->screen = flags & 32;
  g->view_mode = flags & 8 ? 1 : 0;
  g->next = 0x9000;
  World& w = world();
  w.player = flags & 1 ? &m : nullptr;
  w.mission_player = flags & 2 ? &m : nullptr;
  w.time = time;
  w.move_volume_floor = floor;
  w.dead_updates = *dead_updates;
  zent::Budget& b = zent::budget();
  b.updating = budget[0] != 0;
  b.resume = h(budget[1]);
  b.current = h(budget[2]);
  b.left = budget[3];
  b.per_frame = budget[4];
  g_log.clear();

  HeatCalls& hc = heat_calls();
  hc = HeatCalls{};
  hc.stop_sound = [](Sound s) { log({1, static_cast<int32_t>(u(s))}); };
  hc.screen = [](Mech&) -> void* {
    log({40});
    return g->screen ? h(0x7000) : nullptr;
  };
  MoveCalls& mc = move_calls();
  mc = MoveCalls{};
  mc.pilot_sound = [](Mech&, int id, float vol, float pitch) -> Sound {
    log({3, id, bits(vol), bits(pitch)});
    return h(g->next++);
  };
  mc.fade_sound = [](Sound s) -> Sound {
    log({4, static_cast<int32_t>(u(s))});
    return nullptr;
  };
  skis_calls().position = [](Mech&) -> const Vec3* { return &g->pos; };
  DamageCalls& dc = damage_calls();
  dc.last_hit = [](Mech&) { log({5}); };
  dc.view_mode = []() { return g->view_mode; };
  UpdateCalls& c = update_calls();
  c = UpdateCalls{};
  c.finish_power_down = [](Mech&) { log({6}); };
  c.stop_cockpit_sound = [](int i) { log({7, i}); };
  c.add_dead = [](Mech&) { log({8}); };
  c.key = [](int cmd) {
    log({9, cmd});
    return g->key;
  };
  c.controls_view = [](MechControls&, int mode) { log({10, mode}); };
  c.controls_update = [](Mech&, float d) {
    log({11, bits(d)});
    return g->go;
  };
  c.player_frame = [](Mech&, float d) { log({12, bits(d)}); };
  c.update_action = [](Mech&, float d) { log({13, bits(d)}); };
  c.select_gait = [](Mech&, float d) { log({14, bits(d)}); };
  c.get_up = [](Mech&) { log({15}); };
  c.turn = [](Mech&, float d) { log({16, bits(d)}); };
  c.collide = [](Mech& mm) {
    log({17});
    for (int k = 0; k < 4; ++k) mm.blocked[k] = g->blocked[k] != 0;
    return g->held == 1;
  };
  c.dfn = [](Mech&) -> const MechDfn* { return &g->dfn; };
  c.translate = [](Handle node, float x, float y, float z) {
    log({18, static_cast<int32_t>(u(node)), bits(x), bits(y), bits(z)});
  };
  c.moved = [](Mech&) { log({19}); };
  c.landmines = [](const Vec3* p) { log({20, bits(p->x)}); };
  c.weapons_changed = [](Mech&) {
    log({21});
    return g->weapons;
  };
  c.hud_weapons = [](void* s) { log({22, static_cast<int32_t>(u(s))}); };
  c.weapons_fire = [](Mech&, uint8_t f) { log({30, f}); };
  c.weapons_update = [](Mech&, float d) { log({31, bits(d)}); };
  c.equipment_update = [](Mech&, float d) { log({32, bits(d)}); };
  c.update_heat = [](Mech&, float d) { log({33, bits(d)}); };
  c.set_volume = [](Sound s, float v) { log({34, static_cast<int32_t>(u(s)), bits(v)}); };
  c.set_position = [](Sound s, const Vec3*) { log({35, static_cast<int32_t>(u(s))}); };
  c.unwrap = [](MechControls& cc) -> MechControls* {
    log({36});
    return &cc;
  };
  c.x68c_update = [](Handle x, float d) { log({37, static_cast<int32_t>(u(x)), bits(d)}); };
  c.net_update = [](Mech&, float d) { log({38, bits(d)}); };
  c.purge = [](Mech&, float d) { log({39, bits(d)}); };

  m.update(dt);

  mech_io(m, mech, true);
  controls_io(g->c, ctl, true);
  torso_io(g->torso, torso, true);
  std::memcpy(transform, g->transform, 48);
  budget[0] = b.updating;
  budget[1] = static_cast<int32_t>(u(b.resume));
  budget[2] = static_cast<int32_t>(u(b.current));
  budget[3] = b.left;
  budget[4] = b.per_frame;
  *dead_updates = w.dead_updates;
  w.player = w.mission_player = nullptr;
  c = UpdateCalls{};
  mc = MoveCalls{};
  hc = HeatCalls{};
  dc.last_hit = nullptr;
  dc.view_mode = nullptr;
  const int32_t len = static_cast<int32_t>(g_log.size());
  std::memcpy(out, g_log.data(), 4 * (len < *n ? len : *n));
  *n = len;
}

}  // extern "C"
