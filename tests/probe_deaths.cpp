// The deaths' small pieces (c1m1 item 2) for tests/differential/test_deaths.py:
// the parts' effect-end callback, the canned effects and their node copies,
// the death's preparation, zEffect's node checks.
#include <cstdint>
#include <cstring>
#include <iterator>
#include <vector>

#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zeffect/zeff_anim_run.h"
#include "mech3/mech.h"
#include "mech3/mech_collide.h"
#include "mech3/mech_data.h"
#include "mech3/mech_parts.h"

using namespace mech3;

namespace {
std::vector<uint8_t> g_out;
void u32(uint32_t v) {
  for (int i = 0; i < 4; ++i) g_out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void f32(float v) {
  uint32_t b;
  std::memcpy(&b, &v, 4);
  u32(b);
}
int finish(uint8_t* out, int cap) {
  const int n = static_cast<int>(g_out.size());
  if (n <= cap) std::memcpy(out, g_out.data(), g_out.size());
  return n;
}
Handle h(uint32_t v) { return reinterpret_cast<Handle>(static_cast<uintptr_t>(v)); }
uint32_t u(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }

// The canned effects' outside calls, logged: 1 copy, 2 place, 3 start, 4 free.
struct Hooks : MechDataHooks {
  const uint32_t* copies = nullptr;  // what each copy returns, in turn
  int next = 0;
  uint32_t started = 0;
  Handle node_copy(Handle node) override {
    const uint32_t r = copies[next++];
    u32(1), u32(u(node)), u32(r);
    return h(r);
  }
  void node_place(Handle node, float x, float y, float z) override { u32(2), u32(u(node)), f32(x), f32(y), f32(z); }
  Handle effect_start(Handle effect, Handle node, const float args[9]) override {
    u32(3), u32(u(effect)), u32(u(node));
    for (int i = 0; i < 9; ++i) f32(args[i]);
    return h(started);
  }
  void node_free(Handle node) override { u32(4), u32(u(node)); }
} g_hooks;

Pilot g_pilot;
}  // namespace

extern "C" {

// 0x00461c90 on a torso whose node's id word is `id` (no node when
// has_node is 0); handles are +0x6c, +0x70, +0x74, +0x78. Out: the four after.
int probe_effect_ended(int32_t has_node, int32_t id, const uint32_t* handles, uint32_t effect, int32_t value,
                       uint8_t* out, int cap) {
  g_out.clear();
  Torso t;
  t.node = has_node ? h(0x5000) : nullptr;
  t.id = id;
  t.debris_effect = h(handles[0]), t.trail_effect = h(handles[1]);
  t.death_effect = h(handles[2]), t.death_effect2 = h(handles[3]);
  part_effect_ended(h(effect), t, value);
  u32(u(t.debris_effect)), u32(u(t.trail_effect)), u32(u(t.death_effect)), u32(u(t.death_effect2));
  t.node = nullptr;
  return finish(out, cap);
}

// The pilot's canned slots: per slot effect, ofs[3] (float bits), node.
void probe_canned_setup(const uint32_t* slots) {
  g_pilot = Pilot{};
  for (int i = 0; i < 5; ++i) {
    const uint32_t* s = slots + 5 * i;
    g_pilot.canned[i].effect = h(s[0]);
    std::memcpy(g_pilot.canned[i].offset, s + 1, 12);
    g_pilot.canned[i].node = h(s[4]);
  }
  set_mech_data_hooks(&g_hooks);
}

// 0x00432830 (at/vel null: the zero vector), then the result.
int probe_canned(int32_t slot, uint32_t node, const float* at, const float* vel, const uint32_t* copies,
                 uint32_t started, uint8_t* out, int cap) {
  g_out.clear();
  g_hooks.copies = copies, g_hooks.next = 0, g_hooks.started = started;
  u32(u(canned_effect(g_pilot, slot, h(node), at, vel)));
  return finish(out, cap);
}

// 0x00431800: the copies freed in order.
int probe_canned_free(uint8_t* out, int cap) {
  g_out.clear();
  canned_nodes_free();
  u32(0xf);
  set_mech_data_hooks(nullptr);
  return finish(out, cap);
}

// 0x0041d070 on a mech: its state, detail bits, +0x634, whether it is the
// mission's player; the hip's world position; what the canned effect and the
// sound return. Out: the calls, then +0x634 and +0x5e0.
int probe_death_prep(int32_t state, uint32_t detail, uint32_t x634, int32_t player, const float* hip_at,
                     uint32_t canned, uint32_t sound, uint8_t* out, int cap) {
  g_out.clear();
  static const float* s_at;
  static uint32_t s_canned, s_sound;
  s_at = hip_at, s_canned = canned, s_sound = sound;
  Mech m;
  Hip hip;
  hip.node = h(0x6000);
  m.hip = &hip;
  m.pilot = &g_pilot;
  m.anim_state = state, m.detail = detail, m.x634 = x634;
  World& w = world();
  Mech* keep = w.mission_player;
  w.mission_player = player ? &m : nullptr;
  PartCalls& pc = part_calls();
  DamageCalls& dc = damage_calls();
  CollideCalls& cc = collide_calls();
  HeatCalls& hc = heat_calls();
  const PartCalls p0 = pc;
  const DamageCalls d0 = dc;
  const CollideCalls c0 = cc;
  const HeatCalls h0 = hc;
  pc.world_position = [](Handle n, Vec3* o) {
    u32(0x10), u32(u(n));
    *o = {s_at[0], s_at[1], s_at[2]};
    return 0;
  };
  dc.canned = [](Mech&, int slot, Handle node, const float* at, const float* vel) -> Handle {
    u32(0x11), u32(static_cast<uint32_t>(slot)), u32(u(node)), f32(at[0]), f32(at[1]), f32(at[2]), u32(vel ? 1 : 0);
    return h(s_canned);
  };
  cc.crouch = [](Hip&, float v) { u32(0x12), f32(v); };
  hc.play_sound = [](Mech&, int id) -> Sound {
    u32(0x13), u32(static_cast<uint32_t>(id));
    return h(s_sound);
  };
  dc.mission_player_died = [] { u32(0x14); };
  m.death_prep();
  u32(0xf), u32(m.x634), u32(u(m.sounds[10]));
  pc = p0, dc = d0, cc = c0, hc = h0;
  w.mission_player = keep;
  m.hip = nullptr, m.pilot = nullptr;
  for (Sound& s : m.sounds) s = nullptr;
  hip.node = nullptr;
  return finish(out, cap);
}

// 0x00506ae0 for each query, then 0x00506b60: node, root, the objects' and
// nodes' nodes. Out: the answers, then the lists after.
int probe_anim_nodes(uint32_t node, uint32_t root, const uint32_t* objects, int32_t n_obj, const uint32_t* nodes,
                     int32_t n_nodes, const uint32_t* queries, int32_t n_q, uint8_t* out, int cap) {
  g_out.clear();
  zeff::AnimDef d;
  d.node = static_cast<zcls::Node*>(h(node));
  d.root = static_cast<zcls::Node*>(h(root));
  std::vector<zeff::ObjectRef> obj(static_cast<size_t>(n_obj));
  std::vector<zeff::NodeRef40> nds(static_cast<size_t>(n_nodes));
  for (int i = 0; i < n_obj; ++i) obj[i].node = static_cast<zcls::Node*>(h(objects[i]));
  for (int i = 0; i < n_nodes; ++i) nds[i].node = static_cast<zcls::Node*>(h(nodes[i]));
  d.objects = obj.data(), d.nodes = nds.data();
  d.raw[0x109] = static_cast<uint8_t>(n_obj), d.raw[0x10a] = static_cast<uint8_t>(n_nodes);
  for (int i = 0; i < n_q; ++i) u32(zeff::anim_uses_node(&d, static_cast<zcls::Node*>(h(queries[i]))) ? 1 : 0);
  zeff::anim_forget_nodes(&d);
  for (auto& o : obj) u32(u(o.node));
  for (auto& n : nds) u32(u(n.node));
  return finish(out, cap);
}

}  // extern "C"

namespace {
uint32_t s_info, s_vehicle, s_building;
int32_t s_victim_roster, s_game;
Mech* s_victim_mech;
}  // namespace

extern "C" {

// 0x0041b420 on a killer: its roster, allies, +0xf8 and counters; the
// player (the killer, or another with `player_team`); the victim's views
// (info 0: none; has_mech: a mech of `victim_roster`); the game type.
// Out: the calls, then +0xf8 and the six counters.
int probe_on_kill(int32_t roster, uint32_t allies, int32_t xf8, const uint32_t* counters, int32_t killer_is_player,
                  uint32_t player_team, int32_t game, uint32_t info, int32_t has_mech, int32_t victim_roster,
                  uint32_t vehicle, uint32_t building, uint8_t* out, int cap) {
  g_out.clear();
  Pilot kp, pp, vp;
  kp.roster = roster, vp.roster = victim_roster;
  Mech k, p, v;
  k.pilot = &kp, k.allies = allies, k.xf8 = xf8;
  for (int i = 0; i < 6; ++i) k.x3f4[i] = counters[i];
  p.pilot = &pp, p.team = player_team;
  v.pilot = &vp;
  s_info = info, s_vehicle = vehicle, s_building = building, s_game = game;
  s_victim_mech = has_mech ? &v : nullptr;
  World& w = world();
  Mech* keep = w.mission_player;
  w.mission_player = killer_is_player ? &k : &p;
  DamageCalls& dc = damage_calls();
  HeatCalls& hc = heat_calls();
  const DamageCalls d0 = dc;
  const HeatCalls h0 = hc;
  dc.victim_view = [](Handle, int slot) -> Handle {
    switch (slot) {
      case 0x2c: return h(s_info);
      case 0x50: return s_victim_mech;
      case 0x40: return h(s_vehicle);
      default: return h(s_building);
    }
  };
  dc.controls_killed = [](Mech&, Handle o) { u32(0x21), u32(u(o)); };
  dc.salvage_mech = [](Handle m) { u32(0x22), u32(m == s_victim_mech ? 1 : 0); };
  dc.salvage_vehicle = [](Handle o) { u32(0x23), u32(u(o)); };
  hc.game_type = [] { return static_cast<int>(s_game); };
  k.on_kill(h(0x9999));
  u32(0xf), u32(static_cast<uint32_t>(k.xf8));
  for (int i = 0; i < 6; ++i) u32(k.x3f4[i]);
  dc = d0, hc = h0;
  w.mission_player = keep;
  k.pilot = p.pilot = v.pilot = nullptr;
  return finish(out, cap);
}

}  // extern "C"

// --- a part flying off (0x00461d80, thigh 0x00466fe0, arm 0x00467100) ---
namespace {
struct LogArm : Arm {
  int side = 0;
  void destroy(int a, int b) override { u32(0x41), u32(side), u32(a), u32(b); }
  void set_internal(float v, int s) override { u32(0x42), u32(side), f32(v), u32(s); }
};
struct LogTorso : Torso {
  void destroy(int a, int b) override { u32(0x43), u32(a), u32(b); }
};
const float* s_world;
uint32_t s_copy, s_create;
float* s_new_matrix;
}  // namespace

extern "C" {

// kind 1 an arm, 2 a thigh, of `side`; the part's node id; the stub flag and
// whether the pilot has stub models; what the copy and create give; the
// node's world matrix and the part's local one; whether it has a parent; the
// mech's velocity. Out: the calls, then the result, the velocity and the
// matrix the new node holds.
int probe_fly(int32_t kind, int32_t side, int32_t roster, int32_t id, int32_t stub, int32_t stubs, uint32_t copy,
              uint32_t create, const float* world_m, const float* local, int32_t parent, const float* mech_vel,
              uint8_t* out, int cap) {
  g_out.clear();
  s_world = world_m, s_copy = copy, s_create = create;
  Pilot p;
  p.roster = roster, p.name = "owens";
  p.x210 = stubs ? h(0x710) : nullptr;
  for (int i = 0; i < 4; ++i) p.stubs[i] = stubs ? h(0x700 + i) : nullptr;
  Mech m;
  m.pilot = &p;
  m.velocity = {mech_vel[0], mech_vel[1], mech_vel[2]};
  LogArm arms[2];
  Hand hands[2];
  Leg legs[2];
  Calf calves[2];
  Foot feet[2];
  LogTorso torso;
  m.torso = &torso;
  for (int i = 0; i < 2; ++i) {
    arms[i].side = i, arms[i].mech = &m, arms[i].hand = &hands[i], arms[i].id = 2 + i;
    hands[i].mech = &m, hands[i].id = 9 + i;
    legs[i].mech = &m, legs[i].calf = &calves[i], legs[i].foot = &feet[i], legs[i].id = 4 + i;
    calves[i].mech = &m, calves[i].id = 11 + i;
    feet[i].mech = &m, feet[i].id = 13 + i;
    m.arms[i] = &arms[i], m.legs[i] = &legs[i];
  }
  MechPart& x = kind == 1 ? static_cast<MechPart&>(arms[side]) : static_cast<MechPart&>(legs[side]);
  x.id = id;
  x.node = h(0x5000);
  float own[12], fresh[12] = {};
  std::memcpy(own, local, sizeof own);
  x.rotation = own;
  s_new_matrix = fresh;
  static int32_t s_parent;
  s_parent = parent;
  FlyCalls& f = fly_calls();
  DamageCalls& dc = damage_calls();
  PoseCalls& pc = pose_calls();
  const FlyCalls f0 = f;
  const DamageCalls d0 = dc;
  const PoseCalls p0 = pc;
  dc.node_update = [](MechPart&) { u32(0x30); };
  dc.node_matrix = [](MechPart&) -> const float* { return s_world; };
  dc.weapons_lost = [](Mech&, int part) { u32(0x40), u32(static_cast<uint32_t>(part)); };
  f.copy = [](Handle s) -> Handle { u32(0x31), u32(u(s)); return h(s_copy); };
  f.create = []() -> Handle { u32(0x32); return h(s_create); };
  f.report = [](const char* mech, int32_t i) { u32(0x33), u32(static_cast<uint32_t>(std::strlen(mech))), u32(static_cast<uint32_t>(i)); };
  f.rename = [](Handle n, Handle from) { u32(0x34), u32(u(n)), u32(u(from)); };
  f.parent = [](Handle) -> Handle { return s_parent ? h(0x5100) : nullptr; };
  f.set_flag8 = [](Handle n, int32_t on) { u32(0x35), u32(u(n)), u32(static_cast<uint32_t>(on)); };
  f.remove_child = [](Handle p, Handle c) { u32(0x36), u32(u(p)), u32(u(c)); };
  f.attach = [](Handle n) { u32(0x37), u32(u(n)); };
  f.set_flags2c = [](Handle, int32_t bit, int32_t on) { u32(0x38), u32(static_cast<uint32_t>(bit)), u32(static_cast<uint32_t>(on)); };
  f.set_pose = [](Handle n, const float mm[12]) {
    u32(0x39), u32(u(n));
    for (int i = 0; i < 12; ++i) f32(mm[i]);
  };
  f.add_child = [](Handle p, Handle c) { u32(0x3a), u32(u(p)), u32(u(c)); };
  f.set_node = [](MechPart& q, Handle n) { u32(0x3b), u32(u(n)); q.rotation = s_new_matrix; };
  pc.node_set = [](Handle, const float*) { u32(0x3c); };
  f.mark = [](MechPart&, int32_t i) { u32(0x3d), u32(static_cast<uint32_t>(i)); };
  f.clear_ids = [](Handle n) { u32(0x3e), u32(u(n)); };
  f.instant_action = nullptr;
  Vec3 vel{9, 9, 9}, from{}, xx{};
  const size_t before = m.x6d0.size();
  const Handle r = x.fly_off(&from, &xx, &vel, stub);
  for (auto it = std::next(m.x6d0.begin(), static_cast<long>(before)); it != m.x6d0.end(); ++it) u32(0x3f), u32(u(*it));
  u32(0xf), u32(u(r)), f32(vel.x), f32(vel.y), f32(vel.z);
  for (float v : fresh) f32(v);
  f = f0, dc = d0, pc = p0;
  for (auto& a : arms) a.mech = nullptr, a.hand = nullptr;
  for (auto& l : legs) l.mech = nullptr, l.calf = nullptr, l.foot = nullptr;
  x.node = nullptr, x.rotation = nullptr;
  m.pilot = nullptr, m.torso = nullptr;
  for (auto& a : m.arms) a = nullptr;
  for (auto& l : m.legs) l = nullptr;
  return finish(out, cap);
}

}  // extern "C"

#include "gamez/zmodel/gmod_hit.h"

extern "C" {

// The surface marks' setters (0x005426a0..0x00542710) over a run of ops:
// 1 set_type(a), 2 set_map(a), 3 set_uv(a, b bits), 4 enable(a),
// 5 is_map(a). Out after each: on, type, the three maps, u, v (and the
// answer of 5).
int probe_marks(const uint32_t* ops, int32_t n, uint8_t* out, int cap) {
  g_out.clear();
  zmod::UvCapture keep = zmod::uv_capture();
  zmod::uv_capture() = zmod::UvCapture{};
  for (int32_t i = 0; i < n; ++i) {
    const uint32_t op = ops[3 * i], a = ops[3 * i + 1], b = ops[3 * i + 2];
    auto* e = static_cast<zimg::TextureEntry*>(h(a));
    switch (op) {
      case 1: zmod::mark_set_type(static_cast<int32_t>(a)); break;
      case 2: zmod::mark_set_map(e); break;
      case 3: {
        float fu, fv;
        std::memcpy(&fu, &a, 4), std::memcpy(&fv, &b, 4);
        zmod::mark_set_uv(fu, fv);
        break;
      }
      case 4: zmod::mark_enable(static_cast<int32_t>(a)); break;
      default: u32(zmod::mark_is_map(e) ? 1 : 0); break;
    }
    const zmod::UvCapture& c = zmod::uv_capture();
    u32(static_cast<uint32_t>(c.on)), u32(static_cast<uint32_t>(c.type));
    for (auto* m : c.maps) u32(u(m));
    f32(c.uv[0]), f32(c.uv[1]);
    u32(zmod::mark_on() ? 1 : 0);
  }
  zmod::uv_capture() = keep;
  return finish(out, cap);
}

}  // extern "C"

extern "C" {

// 0x00413cb0 on a mech whose list holds `count` nodes: instant action or
// not, the mech's node's opacity flag and value. Out: the calls, then the
// list's length.
int probe_fade(int32_t count, int32_t ia, int32_t used, float value, uint8_t* out, int cap) {
  g_out.clear();
  static int32_t s_ia, s_used;
  static float s_value;
  s_ia = ia, s_used = used, s_value = value;
  Mech m;
  m.node = h(0x6000);
  for (int32_t i = 0; i < count; ++i) m.x6d0.push_back(h(0x7000 + 0x10 * static_cast<uint32_t>(i)));
  FlyCalls& f = fly_calls();
  const FlyCalls f0 = f;
  f.instant_action = [] { return s_ia != 0; };
  f.opacity_used = [](Handle n) { u32(0x51), u32(u(n)); return s_used != 0; };
  f.use_opacity = [](Handle n, int32_t on) { u32(0x52), u32(u(n)), u32(static_cast<uint32_t>(on)); };
  f.opacity = [](Handle n) { u32(0x53), u32(u(n)); return s_value; };
  f.set_opacity = [](Handle n, float v) { u32(0x54), u32(u(n)), f32(v); };
  m.fade_debris(1.0f / 30);
  u32(0xf), u32(static_cast<uint32_t>(m.x6d0.size()));
  f = f0;
  m.node = nullptr;
  return finish(out, cap);
}

}  // extern "C"

// --- the parts' destructors (0x0045fd40 and the subclasses') ---
namespace {
struct LogCalf : Calf {
  void release() override { u32(0x65), u32(11); }
};
struct LogFoot : Foot {
  void release() override { u32(0x65), u32(13); }
};
struct LogHand : Hand {
  void release() override { u32(0x65), u32(9); }
};
}  // namespace

extern "C" {

// kind 0 base, 1 torso, 2 thigh, 3 arm, 4 foot; w: trail, debris, x68,
// node, death effect 1, 2, child a (calf / hand), child b (foot), the foot's
// effect, its toe arrays (0/1).
int probe_release(int32_t kind, const uint32_t* w, uint8_t* out, int cap) {
  g_out.clear();
  FreeCalls& f = free_calls();
  DamageCalls& dc = damage_calls();
  PartCalls& pc = part_calls();
  const FreeCalls f0 = f;
  const DamageCalls d0 = dc;
  const PartCalls p0 = pc;
  f = FreeCalls{};
  f.stop_effect = [](Handle e) { u32(0x60), u32(u(e)); };
  f.ref_free = [](Handle r, Handle n) { u32(0x61), u32(u(r)), u32(u(n)); };
  f.rename = [](Handle n, const char* s) { u32(0x62), u32(u(n)), u32(static_cast<uint32_t>(std::strlen(s))); };
  f.set_flags2c = [](Handle, int32_t bit, int32_t on) { u32(0x63), u32(static_cast<uint32_t>(bit)), u32(static_cast<uint32_t>(on)); };
  f.clear_ids = [](Handle n) { u32(0x64), u32(u(n)); };
  dc.stop_foot_effect = [](Handle e) { u32(0x66), u32(u(e)); };
  pc.effect_free = [](Handle e) { u32(0x67), u32(u(e)); };
  MechPart* p;
  switch (kind) {
    case 1: {
      auto* t = new Torso;
      t->death_effect = h(w[4]), t->death_effect2 = h(w[5]);
      p = t;
      break;
    }
    case 2: {
      auto* l = new Leg;
      l->calf = w[6] ? new LogCalf : nullptr;
      l->foot = w[7] ? new LogFoot : nullptr;
      p = l;
      break;
    }
    case 3: {
      auto* a = new Arm;
      a->hand = w[6] ? new LogHand : nullptr;
      p = a;
      break;
    }
    case 4: {
      auto* ft = new Foot;
      ft->effect = h(w[8]);
      if (w[9]) ft->toe_nodes = new Handle[2](), ft->toe_q = new float[2][4]();
      p = ft;
      break;
    }
    default: p = new Torso;  // the base's body only: a torso without death effects
  }
  p->trail_effect = h(w[0]), p->debris_effect = h(w[1]), p->x68 = h(w[2]), p->node = h(w[3]);
  part_delete(p);
  u32(0xf);
  f = f0, dc = d0, pc = p0;
  return finish(out, cap);
}

}  // extern "C"
