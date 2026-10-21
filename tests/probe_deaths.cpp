// The deaths' small pieces (c1m1 item 2) for tests/differential/test_deaths.py:
// the parts' effect-end callback, the canned effects and their node copies,
// the death's preparation, zEffect's node checks.
#include <cstdint>
#include <cstring>
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
