// The parts' setups and updates (mech3/mech_part_update.cpp) for
// tests/differential/test_parts.py. The mech, its twelve parts, the skis and
// the controls go back and forth as byte images laid out like the
// original's (test_parts.py has the same offsets); each node as its 3x4.
// Calls out are logged as (count, code, args...) on both sides.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zmath/zmth_matrix.h"
#include "mech3/mech.h"
#include "mech3/mech_collide.h"
#include "mech3/mech_command.h"
#include "mech3/mech_control.h"
#include "mech3/mech_move.h"
#include "mech3/mech_reader.h"
#include "mech3/mech_skis.h"
#include "mech3/player_view.h"

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
  void bo(int off, bool& v) { store ? wr<uint8_t>(b, off, v) : void(v = rd<uint8_t>(b, off) != 0); }
  void v3(int off, Vec3& v) { f(off, v.x), f(off + 4, v.y), f(off + 8, v.z); }
  void q(int off, float* v) {
    for (int k = 0; k < 4; ++k) f(off + 4 * k, v[k]);
  }
  // A handle as its index (0 none).
  void h(int off, Handle& v) {
    if (store) wr<uint32_t>(b, off, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(v)));
    else v = reinterpret_cast<Handle>(static_cast<uintptr_t>(rd<uint32_t>(b, off)));
  }
};

std::vector<int32_t> g_log;
void log(std::initializer_list<int32_t> v) {
  g_log.push_back(static_cast<int32_t>(v.size()));
  g_log.insert(g_log.end(), v);
}
void log_vec(const std::vector<int32_t>& e) {
  g_log.push_back(static_cast<int32_t>(e.size()));
  g_log.insert(g_log.end(), e.begin(), e.end());
}

// Parts in test_parts.py's order; nodes 0..11 are theirs, then the mech's,
// the feet's two shadows and the two splash effects' nodes.
enum { kLeg0, kLeg1, kCalf0, kCalf1, kFoot0, kFoot1, kHip, kTorso, kArm0, kArm1, kHand0, kHand1, kParts };
constexpr int kImage = 0x1b4, kMechNode = 12, kShadow0 = 13, kNodes = 17;
constexpr int32_t kIds[kParts] = {kLeftThigh, kRightThigh, kLeftCalf, kRightCalf, kLeftFoot, kRightFoot,
                                  kHip, kTorso, kLeftArm, kRightArm, kLeftHand, kRightHand};

// The env words (test_parts.py's ENV).
enum {
  kFlags, kScreenFlags, kTarget, kPosition = kTarget + 3, kMechMatrix = kPosition + 3,
  kEuler = kMechMatrix + 12, kLook = kEuler + 3, kSoil = kLook + 3, kSteps = kSoil + 12,
  kPrints = kSteps + 11, kPlaying = kPrints + 11, kWorld = kPlaying + 3, kDust = kWorld + 3 * kNodes,
  kVmode = kDust + 5 + 16, kPartSel = kVmode + 1, kTBox = kPartSel + 1, kEnv = kTBox + 7,
};

CameraMode* g_view_mode = nullptr;
CameraMode* g_cockpit = nullptr;

struct PartsWorld {
  Mech m;
  MechData pilot;
  MechData::Soils soils;
  MechControls ctl;
  Leg legs[2];
  Calf calves[2];
  Foot feet[2];
  Hip hip;
  Torso torso;
  Arm arms[2];
  Hand hands[2];
  Skis skis;
  MechDfn dfn{0, nullptr};
  float nodes[kNodes][12] = {};
  float transform[12] = {};
  Handle world_node = nodes[15];
  uint32_t env[kEnv] = {};
  Vec3 position{};
  int screen = 1;
  MechPart* part(int k) {
    switch (k) {
      case kLeg0: case kLeg1: return &legs[k - kLeg0];
      case kCalf0: case kCalf1: return &calves[k - kCalf0];
      case kFoot0: case kFoot1: return &feet[k - kFoot0];
      case kHip: return &hip;
      case kTorso: return &torso;
      case kArm0: case kArm1: return &arms[k - kArm0];
      default: return &hands[k - kHand0];
    }
  }
  int part_index(const MechPart* p) {
    for (int k = 0; k < kParts; ++k)
      if (p == part(k)) return k;
    return -1;
  }
};
PartsWorld* g = nullptr;

int node_index(const void* n) {
  for (int k = 0; k < kNodes; ++k)
    if (n == g->nodes[k]) return k;
  return -1;
}
int32_t hid(Handle h) { return static_cast<int32_t>(reinterpret_cast<uintptr_t>(h)); }

void base_io(MechPart& p, Io& io) {
  io.bo(0x48, p.destroyed), io.bo(0x49, p.submerged), io.bo(0x4a, p.pending), io.v3(0x4c, p.pending_pos);
  io.q(0x58, p.pending_q);
}

void part_io(int k, uint8_t* b, bool store) {
  Io io{b, store};
  MechPart& p = *g->part(k);
  base_io(p, io);
  if (k == kLeg0 || k == kLeg1) {
    Leg& l = g->legs[k - kLeg0];
    io.bo(0x7c, l.knee_back), io.f(0x80, l.thigh_angle), io.f(0x84, l.knee_angle);
    io.q(0x88, l.thigh_q), io.q(0x98, l.calf_q);
    float* ik[] = {&l.rest_x, &l.thigh_len, &l.ik_b0, &l.hip_x, &l.calf_len, &l.rest_angle, &l.ankle.x, &l.ankle.y,
                   &l.ankle.z, &l.ik_k, &l.ik_m};
    for (int i = 0; i < 11; ++i) io.f(0xa8 + 4 * i, *ik[i]);
  } else if (k == kFoot0 || k == kFoot1) {
    Foot& f = g->feet[k - kFoot0];
    io.f(0x74, f.sole), io.v3(0x7c, f.normal), io.bo(0x88, f.x88), io.f(0x8c, f.clearance);
    io.f(0x90, f.last_clearance), io.i(0x94, f.ground_state), io.bo(0xac, f.in_water), io.bo(0xc0, f.pivot);
    io.q(0xc4, f.rot), io.f(0xd4, f.pivot_rate), io.f(0xd8, f.spin), io.f(0xdc, f.water_depth);
    io.f(0xe0, f.water_surface), io.h(0xe4, f.effect), io.i(0xe8, f.soil);
    uint32_t sh = f.shadow ? 1 : 0;
    io.u32(0x78, sh);
    if (!store) f.shadow = sh ? g->nodes[kShadow0 + k - kFoot0] : nullptr;
  } else if (k == kHip) {
    Hip& h = g->hip;
    io.q(0x74, h.rot), io.q(0x84, h.inv), io.f(0x94, h.track), io.v3(0x98, h.offset), io.f(0xa4, h.target_x);
    io.f(0xa8, h.crouch_target), io.f(0xac, h.target_z);
  } else if (k == kTorso) {
    Torso& t = g->torso;
    io.i(0x84, t.mode), io.i(0x88, t.aim_local), io.v3(0x8c, t.aim_dir), io.f(0x98, t.facing), io.i(0x9c, t.aim);
    io.bo(0xa0, t.absolute), io.bo(0xa1, t.pitch_lock), io.bo(0xa2, t.view_aim), io.q(0xa4, t.view_q);
    io.f(0xb4, t.look_yaw_angle), io.f(0xb8, t.look_pitch_angle), io.f(0xbc, t.twist_in), io.f(0xc0, t.pitch_in);
    io.f(0xc4, t.look_yaw), io.f(0xc8, t.look_pitch), io.f(0xd0, t.kick), io.f(0xd4, t.kick_twist);
    io.v3(0xd8, t.jolt_spin), io.v3(0xe4, t.jolt_turn), io.q(0xf0, t.jolt_q), io.v3(0x100, t.jolt);
    io.v3(0x10c, t.jolt_pos), io.q(0x118, t.turn_q), io.q(0x128, t.rot), io.f(0x138, t.pitch), io.f(0x13c, t.yaw);
    io.f(0x140, t.min_pitch), io.f(0x144, t.max_pitch), io.f(0x148, t.max_yaw), io.f(0x158, t.freq);
    io.f(0x15c, t.damp), io.v3(0x160, t.front), io.v3(0x16c, t.flat_front), io.i(0x1b0, t.centring);
    io.f(0x7c, t.half_width), io.f(0x80, t.half_depth);
    for (int j = 0; j < 3; ++j) io.f(0x14c + 4 * j, t.jolt_scale[j]);
  } else if (k == kArm0 || k == kArm1) {
    Arm& a = g->arms[k - kArm0];
    io.f(0x2c, a.x2c), io.f(0x34, a.recoil), io.f(0x78, a.length), io.f(0x7c, a.recoil_hold);
    io.f(0x80, a.recoil_pitch), io.f(0x84, a.recoil_push), io.f(0x88, a.recoil_twist), io.v3(0x8c, a.rest);
    io.f(0x98, a.aim_pitch), io.f(0x9c, a.aim_yaw), io.q(0xa4, a.aim_q), io.f(0xb4, a.aim_rate);
    io.f(0xb8, a.min_pitch), io.f(0xbc, a.max_pitch), io.f(0xc0, a.min_yaw), io.f(0xc4, a.max_yaw);
    io.f(0xc8, a.hand_share), io.bo(0xcc, a.hand_aims);
    uint32_t has = a.hand ? 1 : 0;
    io.u32(0x74, has);
    if (!store) a.hand = has ? &g->hands[k - kArm0] : nullptr;
  } else if (k == kHand0 || k == kHand1) {
    Hand& h = g->hands[k - kHand0];
    io.q(0x74, h.rot), io.v3(0x84, h.rest);
  }
}

void mech_io(uint8_t* b, bool store) {
  Io io{b, store};
  Mech& m = g->m;
  io.v3(0x24, m.forward), io.v3(0x30, m.velocity), io.v3(0x3c, m.vec_3c), io.f(0x4bc, m.gait[3][1]);
  io.f(0x508, m.hip_height), io.f(0x50c, m.crouch_depth);
  io.f(0x518, m.max_speed), io.f(0x520, m.turn_rate), io.f(0x534, m.throttle_down), io.bo(0x584, m.no_power_down), io.bo(0x585, m.knee_back);
  io.bo(0x586, m.crouched), io.h(0x5c8, m.servo()), io.h(0x5e4, m.sounds[11]), io.h(0x5e8, m.sounds[12]);
  io.u32(0x688, m.detail);
  io.bo(0xdc, m.power_down_pending), io.bo(0xde, m.powered_down);
  io.v3(0xec, m.part_aim), io.i(0xf8, m.xf8);
}

void skis_io(uint8_t* b, bool store) {
  Io io{b, store};
  Skis& s = g->skis;
  io.bo(0x5, s.slid);
  for (int i = 0; i < 2; ++i) io.f(0x54 + 4 * i, s.a[i]), io.f(0x5c + 4 * i, s.b[i]), io.f(0x64 + 4 * i, s.c[i]);
  for (int j = 0; j < 7; ++j) {
    io.v3(0xd0 + 0xc * j, s.local[j]);
    io.f(0x15c + 4 * j, s.surface[j]);
    io.i(0x178 + 4 * j, s.in_water[j]);
    GroundHit& c = s.contact[j];
    const int o = 0x194 + 0x28 * j;
    io.v3(o, c.normal), io.f(o + 0xc, c.x), io.f(o + 0x10, c.height), io.f(o + 0x14, c.z);
    io.h(o + 0x20, c.material), io.h(o + 0x24, c.node);
  }
  io.f(0x2f0, s.half), io.f(0x2f4, s.mid_width);
}

void ctl_io(uint8_t* b, bool store) {
  Io io{b, store};
  io.u32(0x08, g->ctl.flags), io.i(0x0c, g->ctl.view), io.f(0x10, g->ctl.throttle), io.f(0x14, g->ctl.turn);
}

void hooks() {
  PartCalls& c = part_calls();
  c = PartCalls{};
  c.world_position = [](Handle n, Vec3* out) {
    const int i = node_index(n);
    log({20, i});
    if (i >= 0) *out = {flt(g->env[kWorld + 3 * i]), flt(g->env[kWorld + 3 * i + 1]), flt(g->env[kWorld + 3 * i + 2])};
    return i >= 0 ? 0 : 1;
  };
  c.node_to_world = [](Handle n, int mode) {
    log({21, node_index(n), mode});
    zmth::MatrixStack& st = zmth::matrix_stack();
    for (int i = 0; i < 12; ++i) st.matrix().m[i] = flt(g->env[kMechMatrix + i]);
    st.is_identity() = 0;
    return 0;
  };
  c.node_angles = [](Handle n, float out[3]) {
    log({22, node_index(n)});
    for (int i = 0; i < 3; ++i) out[i] = flt(g->env[kEuler + i]);
    return 0;
  };
  c.dfn = [](Mech&) -> const MechDfn* {
    log({45});
    return &g->dfn;
  };
  c.register_part = [](Mech&, MechPart& p) { log({36, g->part_index(&p)}); };
  c.init = [](MechPart& p) { log({37, g->part_index(&p)}); };
  c.tree_update = [](Handle n) { log({38, node_index(n)}); };
  c.foot_effect = []() -> Handle {
    log({39});
    return reinterpret_cast<Handle>(uintptr_t{7});
  };
  c.effect_playing = [](Handle e) {
    log({24, hid(e)});
    return g->env[kPlaying + hid(e)] != 0;
  };
  c.effect_start = [](Handle e, float t) { log({26, hid(e), bits(t)}); };
  c.effect_place = [](Handle e, float x, float y, float z) { log({27, hid(e), bits(x), bits(y), bits(z)}); };
  c.step_on = [](const Vec3& at, GroundHit& h, float f) {
    log({28, static_cast<int32_t>(&h - g->skis.contact), bits(at.x), bits(at.y), bits(at.z), bits(f)});
  };
  c.footstep_ff = [](float s) { log({29, bits(s)}); };
  c.play_at = [](Handle s, const Vec3* at, float v) { log({31, hid(s), bits(at->x), bits(at->y), bits(at->z), bits(v)}); };
  c.look_limits = [](Mech&, float* y, float* p, float* mp) {
    log({41});
    *y = flt(g->env[kLook]), *mp = flt(g->env[kLook + 1]), *p = flt(g->env[kLook + 2]);
  };
  c.aim_point = [](Mech&, Vec3* at) {
    if (!(g->env[kFlags] >> 4 & 1)) return false;
    *at = {flt(g->env[kTarget]), flt(g->env[kTarget + 1]), flt(g->env[kTarget + 2])};
    return true;
  };
  c.screen_flags = [](void*) { return g->env[kScreenFlags]; };
  c.sound = [](Mech&, int id, float vol) -> void* {
    log({33, id, bits(vol), 0});
    return reinterpret_cast<void*>(uintptr_t{9});
  };
  c.flag8 = [](Handle n, int32_t* get, int set) {
    if (get) {
      log({48, node_index(n)});
      *get = static_cast<int32_t>(g->env[kDust + 3]);
    } else {
      log({49, node_index(n), set});
    }
    return 0;
  };
  c.ground_at = [](float x, float y, float z, void* out) {
    log({47, node_index(g->world_node), bits(x), bits(y), bits(z)});
    auto& h = *static_cast<GroundHits*>(out);
    h.count = static_cast<uint8_t>(g->env[kDust + 4]);
    for (int k = 0; k < h.count; ++k) {
      h.hit[k].height = flt(g->env[kDust + 5 + 2 * k]);
      h.hit[k].material = reinterpret_cast<Handle>(uintptr_t{g->env[kDust + 6 + 2 * k]});
    }
  };
  c.material_soil = [](Handle m) { return static_cast<uint32_t>(hid(m)); };
  move_calls().pilot_sound = [](Mech&, int id, float vol, float) -> Sound {
    log({42});
    log({33, id, bits(vol), 1});
    return reinterpret_cast<Sound>(uintptr_t{9});
  };
  DamageCalls& d = damage_calls();
  d.spawn_effect_at = [](Handle e, Handle n, float x, float y, float z) {
    log({46, hid(e), node_index(n), bits(x), bits(y), bits(z)});
    return Handle{};
  };
  d.stop_foot_effect = [](Handle e) { log({25, hid(e)}); };
  d.node_set_active = [](Handle n, int on) { log({23, node_index(n), on}); };
  d.spawn_effect = [](Handle e, Handle n, const float a[9]) {
    std::vector<int32_t> v{30, hid(e), hid(n)};
    for (int i = 0; i < 9; ++i) v.push_back(bits(a[i]));
    log_vec(v);
    return Handle{};
  };
  d.hud_weapons = [](void*) { log({32}); };
  d.free_part = [](MechPart* p) { log({40, g->part_index(p)}); };
  HeatCalls& hc = heat_calls();
  hc.screen = [](Mech&) -> void* {
    log({43});
    return g->env[kFlags] >> 3 & 1 ? &g->screen : nullptr;
  };
  hc.stop_sound = [](Sound s) { log({34, hid(s)}); };
  controls_calls().mouse_set = [](float x, float y) { log({50, bits(x), bits(y)}); };
  controls_calls().mech_camera = [](Mech&, int32_t) -> CameraMode* { log({41}); return g_cockpit; };
  controls_calls().view_target = []() { return world().player; };
  controls_calls().view_mode = []() -> CameraMode* { return g_view_mode; };
  c.world_point = [](Handle n, Vec3* p) {
    const int i = node_index(n);
    log({54, i, bits(p->x), bits(p->y), bits(p->z)});
    const int k = i < 0 ? 0 : i;
    *p = {p->x + flt(g->env[kWorld + 3 * k]), p->y + flt(g->env[kWorld + 3 * k + 1]), p->z + flt(g->env[kWorld + 3 * k + 2])};
    return 0;
  };
  collide_calls().contact_mech = [](Handle) -> Mech* { log({55}); return g->env[kPartSel] & 4 ? &g->m : nullptr; };
  c.find_in_tree = [](Handle n, const char* name) -> Handle {
    log({56, node_index(n), std::strcmp(name, "torso_polys") == 0 ? 1 : 0});
    return g->env[kTBox] & 1 ? static_cast<Handle>(g->nodes[16]) : nullptr;
  };
  collide_calls().hit_box = [](Handle n) -> const float* {
    log({57, node_index(n)});
    static float box[6];
    if (!(g->env[kTBox] & 2)) return nullptr;
    for (int k = 0; k < 6; ++k) box[k] = flt(g->env[kTBox + 1 + k]);
    return box;
  };
  command_calls().seen_position = [](uint32_t s) -> const float* {
    log({53, static_cast<int32_t>(s)});
    static float at[3];
    for (int k = 0; k < 3; ++k) at[k] = flt(g->env[kPosition + k]);
    return at;
  };
  update_calls().turn = [](Mech&, float dt) { log({35, bits(dt)}); };
  update_calls().dfn = [](Mech&) -> const MechDfn* {
    log({44});
    return &g->dfn;
  };
  skis_calls().position = [](Mech&) -> const Vec3* {
    log({42});
    return &g->position;
  };
  skis_calls().soil = [](Handle mat) { return static_cast<int>(g->env[kSoil + hid(mat)]); };
  pose_calls().node_matrix = [](Handle n) { return static_cast<float*>(n); };
  pose_calls().node_set = [](Handle n, const float* m) {
    std::vector<int32_t> e{14, node_index(n)};
    for (int i = 0; i < 12; ++i) e.push_back(bits(m[i]));
    log_vec(e);
  };
}

void unhook() {
  part_calls() = PartCalls{};
  pose_calls() = PoseCalls{};
  DamageCalls& d = damage_calls();
  d.stop_foot_effect = nullptr, d.node_set_active = nullptr, d.spawn_effect = nullptr, d.hud_weapons = nullptr;
  d.free_part = nullptr, d.spawn_effect_at = nullptr;
  move_calls().pilot_sound = nullptr;
  heat_calls().screen = nullptr, heat_calls().stop_sound = nullptr;
  controls_calls().mouse_set = nullptr, controls_calls().mech_camera = nullptr;
  controls_calls().view_target = nullptr, controls_calls().view_mode = nullptr;
  collide_calls().contact_mech = nullptr, command_calls().seen_position = nullptr;
  collide_calls().hit_box = nullptr;
  update_calls().turn = nullptr, update_calls().dfn = nullptr;
  skis_calls().position = nullptr, skis_calls().soil = nullptr;
  world().player = world().mission_player = nullptr;
  world().stats = false;
}
}  // namespace

extern "C" {

// present: bit k, part k has a node.
void probe_parts_load(const uint8_t* mech, const uint8_t* parts, const float* nodes, uint32_t present,
                      const uint8_t* skis, const uint8_t* ctl, const uint32_t* env, const uint8_t* def) {
  if (!g) g = new PartsWorld;
  PartsWorld& w = *g;
  w.m = Mech{};
  w.pilot = MechData{};
  w.ctl = MechControls{};
  w.skis = Skis{};
  std::memcpy(w.env, env, sizeof w.env);
  std::memcpy(w.dfn.raw, def, sizeof w.dfn.raw);
  std::memcpy(w.nodes, nodes, sizeof w.nodes);
  for (int i = 0; i < 11; ++i) {
    w.soils.step_sound[i] = reinterpret_cast<Handle>(uintptr_t{env[kSteps + i]});
    w.soils.footprint[i] = reinterpret_cast<Handle>(uintptr_t{env[kPrints + i]});
  }
  w.pilot.soils = &w.soils;
  w.pilot.roster = static_cast<int32_t>(env[kDust]);
  w.pilot.landing_dust = reinterpret_cast<Handle>(uintptr_t{env[kDust + 1]});
  w.pilot.landing_splash = reinterpret_cast<Handle>(uintptr_t{env[kDust + 2]});
  w.m.pilot = &w.pilot;
  w.m.controls = &w.ctl;
  w.ctl.owner = &w.m;
  w.m.skis = &w.skis;
  w.skis.mech = &w.m;
  w.m.node = w.nodes[kMechNode];
  for (int i = 0; i < 12; ++i) w.transform[i] = flt(env[kMechMatrix + i]);
  w.m.transform = w.transform;
  w.position = {flt(env[kPosition]), flt(env[kPosition + 1]), flt(env[kPosition + 2])};
  w.m.legs[0] = &w.legs[0], w.m.legs[1] = &w.legs[1];
  w.m.arms[0] = &w.arms[0], w.m.arms[1] = &w.arms[1];
  w.m.hip = &w.hip, w.m.torso = &w.torso;
  for (int i = 0; i < 2; ++i) {
    w.legs[i] = Leg{}, w.calves[i] = Calf{}, w.feet[i] = Foot{}, w.arms[i] = Arm{}, w.hands[i] = Hand{};
    w.legs[i].calf = &w.calves[i], w.legs[i].foot = &w.feet[i];
  }
  w.hip = Hip{}, w.torso = Torso{};
  for (int k = 0; k < kParts; ++k) {
    MechPart& p = *w.part(k);
    p.mech = &w.m;
    const bool has = present >> k & 1;
    p.node = has ? w.nodes[k] : nullptr;
    p.id = has ? kIds[k] : -1;
    p.rotation = w.nodes[k];
    p.translation = reinterpret_cast<Vec3*>(w.nodes[k] + 9);
    part_io(k, const_cast<uint8_t*>(parts + kImage * k), false);
  }
  mech_io(const_cast<uint8_t*>(mech), false);
  skis_io(const_cast<uint8_t*>(skis), false);
  ctl_io(const_cast<uint8_t*>(ctl), false);
  const uint32_t fl = env[kFlags];
  world().player = fl & 1 ? &w.m : nullptr;
  world().mission_player = fl & 2 ? &w.m : nullptr;
  world().stats = fl & 4;
  // The part aim's target (0x0041d810): a contact, what it sees, a TC.
  static ai::Contact contact;
  const uint32_t sel = env[kPartSel];
  contact = ai::Contact{};
  contact.seen = sel & 2 ? 0x7777 : 0;
  w.m.target = sel & 1 ? &contact : nullptr;
  w.m.equipment[14] = sel & 8 ? 1 : 0;
  g_log.clear();
  hooks();
}

// op 0: hip update(a0); 1: leg a1's update; 2: foot a1's ground update; 3:
// arm a1's update; 4: arm a1's kick(a2); 5: torso update; 6: hand a1's
// (base) update; 7: hip setup; 8: leg a1's setup; 9: arm a1's setup.
float probe_parts_op(int op, const float* a) {
  PartsWorld& w = *g;
  const int i = static_cast<int>(a[1]);
  switch (op) {
    case 0: w.hip.update(a[0]); break;
    case 1: w.legs[i].update(a[0]); break;
    case 2: return w.feet[i].ground_update();
    case 3: w.arms[i].update(a[0]); break;
    case 4: w.arms[i].kick(a[2]); break;
    case 5: w.torso.update(a[0]); break;
    case 6: w.hands[i].update(a[0]); break;
    case 7: return w.hip.setup() ? 1.0f : 0.0f;
    case 8: return w.legs[i].setup() ? 1.0f : 0.0f;
    case 9: return w.arms[i].setup() ? 1.0f : 0.0f;
    case 10: w.m.crouch(i != 0); break;
    case 11: w.hip.duck(a[2]); break;
    case 12: {
      const Vec3 v{a[0], a[2], a[3]};
      w.hip.set_crouch(&v);
      break;
    }
    case 13: case 14: w.m.ground_dust(); break;
    case 15: w.torso.set_pitch_lock(i != 0); break;
    case 16: w.torso.hold_view(i != 0); break;
    case 17: {
      const Vec3 dir{a[2], a[3], a[0]};
      Vec3 out{};
      const bool r = w.torso.set_aim(a[3] < -1.5f ? nullptr : &dir, &out, i != 0);
      log({51, bits(out.x), bits(out.y), bits(out.z)});
      return r ? 1.0f : 0.0f;
    }
    case 18: w.torso.centre(i + 1); break;
    case 19: w.torso.set_mode((a[2] > 0.0f ? 1 : 0) + i); break;
    case 23: return w.torso.setup() ? 1.0f : 0.0f;
    case 21: w.m.target_part(static_cast<int32_t>(a[3] * 4 + 8) % 16); break;
    case 22: return w.m.aim_part() ? 1.0f : 0.0f;
    case 24: return w.m.can_face(Vec3{a[2], a[3], a[0]}) ? 1.0f : 0.0f;  // vt+0x8c
    case 29: return w.m.stop_share(a[0] * 100.0f);  // vt+0x12c
    case 30: return w.m.throttle_speed();  // vt+0x114
    case 25: {  // vt+0x90: the clamped direction logged
      Vec3 out{};
      const bool r = w.m.clamp_aim(Vec3{a[2], a[3], a[0]}, &out);
      log({53, bits(out.x), bits(out.y), bits(out.z)});
      return r ? 1.0f : 0.0f;
    }
    case 26: return w.m.faces_within(i ? w.torso.facing : a[2]) ? 1.0f : 0.0f;  // vt+0x150 (i: at its facing)
    case 28: {  // vt+0x08: the velocity others lead it by, logged
      Vec3 out{};
      w.m.lead_velocity(&out);
      log({54, bits(out.x), bits(out.y), bits(out.z)});
      break;
    }
    case 27: {  // vt+0x154 (none when a3 < -1.5)
      const Vec3 dir{a[2], a[3], a[0]};
      w.m.set_torso_aim(a[3] < -1.5f ? nullptr : &dir);
      break;
    }
    case 20: {
      // The cockpit camera (vt+0x10c(1)) and the view's mode as in the emulator.
      static MechCamera cockpit(1), mode(0);
      cockpit.look_rate = 0.0f;
      mode = MechCamera(static_cast<int32_t>(w.env[kVmode] & 7));
      mode.torso_look = 0;
      g_view_mode = w.env[kVmode] & 0x10 ? &mode : nullptr;
      g_cockpit = &cockpit;
      w.ctl.set_view(i + (a[2] > 0.0f ? 2 : 0));
      log({52, static_cast<int32_t>(w.ctl.view), bits(cockpit.look_rate), mode.torso_look});
      break;
    }
    default: return -1.0f;
  }
  return 0.0f;
}

int probe_parts_store(uint8_t* mech, uint8_t* parts, float* nodes, uint8_t* ctl, int32_t* out, int cap) {
  PartsWorld& w = *g;
  for (int k = 0; k < kParts; ++k) part_io(k, parts + kImage * k, true);
  mech_io(mech, true);
  ctl_io(ctl, true);
  std::memcpy(nodes, w.nodes, sizeof w.nodes);
  unhook();
  const int n = static_cast<int>(g_log.size());
  if (n > cap) return -1;
  std::memcpy(out, g_log.data(), n * 4);
  return n;
}

}  // extern "C"
