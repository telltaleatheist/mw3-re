// Damage at run time (mech_parts, mech_damage) for tests/differential/test_damage.py.
// The test builds the same mech in emulator memory and here; state goes back
// and forth as byte images laid out like the original objects, and every call
// into unported code is logged as (code, args...) on both sides.
#include <cstdint>
#include <cstring>
#include <new>
#include <memory>
#include <string>
#include <vector>

#include "crt/msvc_rand.h"
#include "mech3/weapon.h"
#include "mech3/mech.h"
#include "mech3/mech_config.h"

using namespace mech3;

namespace {

enum { kParts = 13 };
// torso, leg0, leg1, arm0, arm1, head, hip, foot0, foot1, calf0, calf1, hand0, hand1
const int kIds[kParts] = {1, 4, 5, 2, 3, 6, 15, 13, 14, 11, 12, 9, 10};

struct Script {
  uint32_t attacker = 0, screen = 0, x1c = 0, owner = 0, fly_node = 0, blend = 0;
  int32_t view = -1;  // -1: no view object
  int32_t game_type = 0, force0 = 0, networked = 0, anim_ok = 0, hud167c = 0, hud1514 = 0;
  int32_t wcrit = 0;
  float throttle = 0;
  Vec3 fly[3] = {};
  Vec3 position = {};
  float matrix[12] = {};
  uint32_t next_effect = 0x7000;
};

struct State {
  Mech mech;
  Mech other;  // the player when it is not this mech
  Pilot pilot;
  int32_t x228[2] = {};
  std::unique_ptr<MechPart> parts[kParts];
  bool freed[kParts] = {};
  uint32_t emu[kParts] = {};
  Handle nodes[kParts] = {};  // each part's node as loaded; a node keeps its part after a detach
  uint32_t emu_mech = 0, emu_nuke = 0;
  std::unique_ptr<Config> config;
  std::vector<ConfigItem*> items;
  WeaponType type;
  WeaponDef def;
  Collision coll;
  std::string name;
  Script s;
  std::vector<int32_t> log;
};
State* g = nullptr;

int32_t fbits(float f) {
  int32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}
template <typename T> T rd(const uint8_t* b, int off) {
  T v;
  std::memcpy(&v, b + off, sizeof v);
  return v;
}
template <typename T> void wr(uint8_t* b, int off, T v) { std::memcpy(b + off, &v, sizeof v); }
Handle h(uint32_t v) { return reinterpret_cast<Handle>(static_cast<uintptr_t>(v)); }
uint32_t u(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }

int index_of(const MechPart* p) {
  for (int i = 0; i < kParts; ++i)
    if (g->parts[i].get() == p) return i;
  return -1;
}
uint32_t emu_of(const MechPart* p) {
  const int i = index_of(p);
  return i < 0 ? 0 : g->emu[i];
}
uint32_t emu_mech(const Mech* m) { return m == &g->mech ? g->emu_mech : 0; }
// Entity handles: the mech itself is the emulator's mech address there.
Handle from_emu(uint32_t v) { return v && v == g->emu_mech ? static_cast<Handle>(&g->mech) : h(v); }
uint32_t to_emu(Handle v) { return v == static_cast<Handle>(&g->mech) ? g->emu_mech : u(v); }

void log(std::initializer_list<int32_t> v) {
  g->log.push_back(static_cast<int32_t>(v.size()));
  g->log.insert(g->log.end(), v);
}

// Field layouts, shared with FIELDS in test_damage.py.
void part_io(MechPart& p, uint8_t* b, bool store) {
  auto f = [&](int off, float& v) { store ? wr(b, off, v) : void(v = rd<float>(b, off)); };
  auto i = [&](int off, int32_t& v) { store ? wr(b, off, v) : void(v = rd<int32_t>(b, off)); };
  auto bo = [&](int off, bool& v) { store ? wr<uint8_t>(b, off, v) : void(v = rd<uint8_t>(b, off) != 0); };
  auto hd = [&](int off, Handle& v) { store ? wr<uint32_t>(b, off, u(v)) : void(v = h(rd<uint32_t>(b, off))); };
  hd(0x08, p.node);
  f(0x38, p.internal), f(0x3c, p.internal_scale), f(0x40, p.armor), f(0x44, p.armor_scale);
  bo(0x48, p.destroyed);
  hd(0x68, p.x68), hd(0x6c, p.debris_effect), hd(0x70, p.trail_effect);
  if (auto* t = dynamic_cast<Torso*>(&p)) {
    hd(0x74, t->death_effect), hd(0x78, t->death_effect2);
    f(0x7c, t->half_width), f(0x80, t->half_depth);
    f(0xd8, t->jolt_spin.x), f(0xdc, t->jolt_spin.y), f(0xe0, t->jolt_spin.z);
    f(0x100, t->jolt.x), f(0x104, t->jolt.y), f(0x108, t->jolt.z);
    for (int k = 0; k < 3; ++k) f(0x14c + 4 * k, t->jolt_scale[k]);
    for (int s = 1; s < 6; ++s) f(0x174 + 4 * s, t->sec_armor[s]), f(0x188 + 4 * s, t->sec_armor_scale[s]);
    f(0x1a0, t->side_internal[0]), f(0x1a4, t->side_internal[1]);
    f(0x1a8, t->side_internal_scale[0]), f(0x1ac, t->side_internal_scale[1]);
  }
  if (auto* ft = dynamic_cast<Foot*>(&p)) {
    bo(0xac, ft->in_water);
    f(0xdc, ft->water_depth), f(0xe0, ft->water_surface);
    hd(0xe4, ft->effect);
    i(0xe8, ft->soil);
  }
  if (auto* a = dynamic_cast<Arm*>(&p)) {
    if (store) wr<uint32_t>(b, 0x74, emu_of(a->hand));
  }
  p.id = p.node ? kIds[index_of(&p)] : -1;
}

void mech_io(Mech& m, uint8_t* b, bool store) {
  auto f = [&](int off, float& v) { store ? wr(b, off, v) : void(v = rd<float>(b, off)); };
  auto i = [&](int off, int32_t& v) { store ? wr(b, off, v) : void(v = rd<int32_t>(b, off)); };
  auto ui = [&](int off, uint32_t& v) { store ? wr(b, off, v) : void(v = rd<uint32_t>(b, off)); };
  auto bo = [&](int off, bool& v) { store ? wr<uint8_t>(b, off, v) : void(v = rd<uint8_t>(b, off) != 0); };
  auto hd = [&](int off, Handle& v) { store ? wr<uint32_t>(b, off, u(v)) : void(v = h(rd<uint32_t>(b, off))); };
  auto v3 = [&](int off, Vec3& v) { f(off, v.x), f(off + 4, v.y), f(off + 8, v.z); };
  hd(0x04, m.node);
  bo(0x08, m.active);
  v3(0x30, m.velocity), v3(0x3c, m.vec_3c), v3(0x58, m.vec_58);
  ui(0xc0, m.team), ui(0xc4, m.allies);
  f(0xe0, m.last_hit_time);
  if (store) wr<uint32_t>(b, 0xe4, emu_of(m.last_hit_part));
  for (int k = 0; k < 15; ++k) i(0x3ac + 4 * k, m.equipment[k]);
  hd(0x3e8, m.decals);
  bo(0x410, m.damaged), bo(0x411, m.killed_by_nuke);
  for (int k = 0; k < 13; ++k) hd(0x424 + 4 * k, reinterpret_cast<Handle&>(m.anims[k]));
  for (int k = 0; k < 10; ++k) hd(0x458 + 4 * k, reinterpret_cast<Handle&>(m.action_objects[k]));
  f(0x510, m.walk_speed), f(0x518, m.max_speed), f(0x53c, m.knock), v3(0x540, m.knock_vec);
  f(0x558, m.heat_added), f(0x570, m.narc_time);
  i(0x578, m.anim_state), i(0x57c, m.prev_state);
  bo(0x584, m.no_power_down);
  i(0x5ac, m.limp);
  for (int k = 0; k < 25; ++k) hd(0x5b8 + 4 * k, m.sounds[k]);
  bo(0x630, m.invincible);
  i(0x65c, m.heat_sinks.count);
  if (store) wr<uint32_t>(b, 0x664, to_emu(m.attacker));
  else m.attacker = from_emu(rd<uint32_t>(b, 0x664));
  for (int k = 0; k < 5; ++k) f(0x6b8 + 4 * k, m.knockdown[k]);
}

void install_hooks() {
  HeatCalls& hc = heat_calls();
  hc = HeatCalls{};
  hc.game_type = [] { return g->s.game_type; };
  hc.throttle = [](Mech&) { return g->s.throttle; };
  hc.screen = [](Mech&) -> void* { return h(g->s.screen); };
  hc.stop_sound = [](Sound s) { log({36, static_cast<int32_t>(u(s))}); };
  hc.play_mech_sound = [](int i) { log({12, i}); };
  DamageCalls& c = damage_calls();
  c = DamageCalls{};
  c.attacker = [] { return from_emu(g->s.attacker); };
  c.node_part = [](Handle node) -> MechPart* {
    for (int i = 0; i < kParts; ++i)
      if (node && g->nodes[i] == node) return g->parts[i].get();
    return nullptr;
  };
  c.node_owner = [](Handle) { return h(g->s.owner); };
  c.message = [](int) { return "NUKE"; };
  c.controls_hit = [](Mech&, WeaponDef*, Collision*, float d, Handle a) {
    log({1, fbits(d), static_cast<int32_t>(to_emu(a))});
  };
  c.controls_stop = [](Mech&) { log({37}); };
  c.net_event = [](Mech&, int kind, intptr_t arg) {
    // the death message carries the attacker
    const uint32_t a = kind == 0x13 ? to_emu(reinterpret_cast<Handle>(arg)) : static_cast<uint32_t>(arg);
    log({7, kind, static_cast<int32_t>(a)});
  };
  c.killer_notify = [](Handle k, Mech& v) { log({38, static_cast<int32_t>(to_emu(k)), static_cast<int32_t>(emu_mech(&v))}); };
  c.stats = [](uint32_t fn, int arg) { log({33, static_cast<int32_t>(fn), arg}); };
  c.stats_hit = [](int id, const Vec3* i, float f) { log({2, id, fbits(i->x), fbits(i->y), fbits(i->z), fbits(f)}); };
  c.networked = [] { return g->s.networked != 0; };
  c.view_mode = [] { return g->s.view; };
  c.mission_failed = [](int why) { log({34, why}); };
  c.mission_player_died = [] { log({35}); };
  c.player_died_hud = [](void*) {
    if (!g->s.hud167c) log({391});
    if ((~(static_cast<uint32_t>(g->s.hud1514) >> 4)) & 1) log({392});
    log({393, 0});
  };
  c.roster_x1c = [](Mech&) { return h(g->s.x1c); };
  c.damage_decal = [](Handle x, Handle d, Collision*, float dmg) {
    log({3, static_cast<int32_t>(u(x)), static_cast<int32_t>(u(d)), fbits(dmg)});
  };
  c.last_hit = [](Mech&) { log({4}); };
  c.zaps_hud = [](void* s, float a, int k, float b) { log({5, static_cast<int32_t>(u(s)), fbits(a), k, fbits(b)}); };
  c.head_hit_hud = [] { log({6}); };
  c.hud_weapons = [](void* s) { log({11, static_cast<int32_t>(u(s))}); };
  c.equip_voice = [](int k) { log({13, k}); };
  c.voice = [](int i) { log({14, i}); };
  c.force_variant0 = [] { return g->s.force0 != 0; };
  c.weapons_lost = [](Mech&, int id) { log({8, id}); };
  c.weapons_critical = [](Mech&, int part, int ammo) {
    log({9, part, ammo});
    return g->s.wcrit != 0;
  };
  c.weapons_stop = [](Mech&) { log({10}); };
  c.fly_off = [](MechPart& p, Vec3* a, Vec3* x, Vec3* v, int flag) {
    log({15, static_cast<int32_t>(emu_of(&p)), flag});
    *a = g->s.fly[0], *x = g->s.fly[1], *v = g->s.fly[2];
    return h(g->s.fly_node);
  };
  c.spawn_effect = [](Handle e, Handle node, const float a[9]) {
    log({16, static_cast<int32_t>(u(e)), static_cast<int32_t>(u(node)), fbits(a[0]), fbits(a[1]), fbits(a[2]),
         fbits(a[3]), fbits(a[4]), fbits(a[5]), fbits(a[6]), fbits(a[7]), fbits(a[8])});
    return h(g->s.next_effect++);
  };
  c.spawn_effect_at = [](Handle e, Handle node, float x, float y, float z) {
    log({17, static_cast<int32_t>(u(e)), static_cast<int32_t>(u(node)), fbits(x), fbits(y), fbits(z)});
    return h(g->s.next_effect++);
  };
  c.on_effect_end = [](Handle e, MechPart& p) { log({18, static_cast<int32_t>(u(e)), static_cast<int32_t>(emu_of(&p))}); };
  c.node_unlink = [](Handle x, Handle node) { log({19, static_cast<int32_t>(u(x)), static_cast<int32_t>(u(node))}); };
  c.node_set_active = [](Handle node, int on) { log({20, static_cast<int32_t>(u(node)), on}); };
  c.node_set_name = [](Handle node, const char* n) {
    log({21, static_cast<int32_t>(u(node)), std::strcmp(n, "deadmech") == 0});
  };
  c.stop_foot_effect = [](Handle e) { log({22, static_cast<int32_t>(u(e))}); };
  c.free_part = [](MechPart* p) {
    log({23, static_cast<int32_t>(emu_of(p))});
    g->freed[index_of(p)] = true;
  };
  c.node_matrix = [](MechPart&) -> const float* { return g->s.matrix; };
  c.node_update = [](MechPart& p) { log({24, static_cast<int32_t>(u(p.node))}); };
  c.anim_ok = [](Mech&, int k, int a) {
    log({25, k, a});
    return g->s.anim_ok != 0;
  };
  c.hip_reset = [](MechPart& hip, const Vec3* v) {
    log({26, static_cast<int32_t>(emu_of(&hip)), fbits(v->x), fbits(v->y), fbits(v->z)});
  };
  c.death_prep = [](Mech&) { log({27}); };
  c.death_anim = [](Mech&, int a) { log({28, a}); };
  c.stop_action = [](Handle o) { log({29, static_cast<int32_t>(u(o))}); };
  c.blend_anim = [](Handle base, Handle limp) {
    log({30, static_cast<int32_t>(u(base)), static_cast<int32_t>(u(limp))});
    return h(g->s.blend);
  };
  c.anim_speed = [](Handle a, float v) { log({31, static_cast<int32_t>(u(a)), fbits(v)}); };
  c.gait_speed = [](Mech&, float v) { log({32, fbits(v)}); };
  c.position = [](Mech&) -> const Vec3* { return &g->s.position; };
}

}  // namespace

extern "C" {

// A fresh mech with all thirteen parts; emu[] are their emulator addresses.
void probe_damage_reset(const uint32_t* emu, uint32_t emu_mech_addr) {
  delete g;
  g = new State;
  g->emu_mech = emu_mech_addr;
  Mech& m = g->mech;
  g->parts[0] = std::make_unique<Torso>();
  g->parts[1] = std::make_unique<Leg>();
  g->parts[2] = std::make_unique<Leg>();
  g->parts[3] = std::make_unique<Arm>();
  g->parts[4] = std::make_unique<Arm>();
  g->parts[5] = std::make_unique<Head>();
  g->parts[6] = std::make_unique<Hip>();
  g->parts[7] = std::make_unique<Foot>();
  g->parts[8] = std::make_unique<Foot>();
  g->parts[9] = std::make_unique<Calf>();
  g->parts[10] = std::make_unique<Calf>();
  g->parts[11] = std::make_unique<Hand>();
  g->parts[12] = std::make_unique<Hand>();
  for (int i = 0; i < kParts; ++i) {
    g->parts[i]->mech = &m;
    g->emu[i] = emu[i];
  }
  m.torso = static_cast<Torso*>(g->parts[0].get());
  for (int k = 0; k < 2; ++k) {
    m.legs[k] = static_cast<Leg*>(g->parts[1 + k].get());
    m.arms[k] = static_cast<Arm*>(g->parts[3 + k].get());
    m.legs[k]->foot = static_cast<Foot*>(g->parts[7 + k].get());
    m.legs[k]->calf = static_cast<Calf*>(g->parts[9 + k].get());
    m.arms[k]->hand = static_cast<Hand*>(g->parts[11 + k].get());
  }
  m.head = static_cast<Head*>(g->parts[5].get());
  m.hip = static_cast<Hip*>(g->parts[6].get());
  m.pilot = &g->pilot;
  install_hooks();
}

// items: rows of (kind, count, group, part, id, tech); list: part indices of
// the mech's part list; actions: the action list.
void probe_damage_load(const uint8_t* mech_img, const uint8_t* part_imgs, const int32_t* pilot,
                       const int32_t* items, int nitems, const int32_t* list, int nlist,
                       const int32_t* actions, int nactions, uint32_t seed) {
  crt::msvc_srand(seed);
  Mech& m = g->mech;
  mech_io(m, const_cast<uint8_t*>(mech_img), false);
  for (int i = 0; i < kParts; ++i) {
    part_io(*g->parts[i], const_cast<uint8_t*>(part_imgs + 0x200 * i), false);
    g->nodes[i] = g->parts[i]->node;
  }
  // pilot: roster, effects [6][3], death[4], x200, water, x228 present, x228[1]
  Pilot& p = g->pilot;
  p.roster = pilot[0];
  for (int i = 0; i < 18; ++i) p.part_effects[i / 3][i % 3] = h(static_cast<uint32_t>(pilot[1 + i]));
  for (int i = 0; i < 4; ++i) p.death_effects[i] = h(static_cast<uint32_t>(pilot[19 + i]));
  p.x200 = pilot[23];
  p.water_death = h(static_cast<uint32_t>(pilot[24]));
  g->x228[1] = pilot[26];
  p.x228 = pilot[25] ? g->x228 : nullptr;
  for (ConfigItem* it : g->items) delete it;
  g->items.clear();
  g->config = std::make_unique<Config>(0, "test");
  for (int r = 0; r < nitems; ++r) {
    const int32_t* row = items + 6 * r;
    ConfigItem* it;
    if (row[0] == 1) {
      auto* w = new WeaponItem;
      w->weapon = row[4], w->tech = row[5];
      it = w;
    } else if (row[0] == 2) {
      auto* a = new AmmoItem;
      a->weapon = row[4];
      it = a;
    } else {
      auto* e = new EquipmentItem;
      e->equipment = row[4], e->tech = row[5];
      it = e;
    }
    it->count = row[1], it->group = row[2], it->part = row[3];
    g->items.push_back(it);
    g->config->items.push_back(it);
  }
  m.config = g->config.get();
  m.parts.clear();
  for (int i = 0; i < nlist; ++i) m.parts.push_back(g->parts[list[i]].get());
  m.actions.assign(actions, actions + nactions);
}

void probe_damage_world(const int32_t* world_v) {
  Mech& m = g->mech;
  // world: player is this (1) or the other, mission player likewise, stats,
  // difficulty, shot[6], toggle, time, dt, the other's allies mask
  World& w = world();
  w.player = world_v[0] ? &m : &g->other;
  w.mission_player = world_v[1] ? &m : &g->other;
  w.stats = world_v[2] != 0;
  std::memcpy(&w.difficulty, &world_v[3], 4);
  std::memcpy(w.shot, &world_v[4], 24);
  w.death_anim_toggle = world_v[10] != 0;
  std::memcpy(&w.time, &world_v[11], 4);
  std::memcpy(&w.dt, &world_v[12], 4);
  g->other.allies = static_cast<uint32_t>(world_v[13]);
}

// The scripted answers of unported code for the next operation.
void probe_damage_script(const int32_t* v, const float* f) {
  Script& s = g->s;
  s.attacker = v[0], s.screen = v[1], s.x1c = v[2], s.view = v[3];
  s.owner = v[4], s.fly_node = v[5];
  s.blend = v[6], s.game_type = v[7], s.force0 = v[8], s.networked = v[9], s.anim_ok = v[10];
  s.hud167c = v[11], s.hud1514 = v[12], s.wcrit = v[13], s.next_effect = static_cast<uint32_t>(v[14]);
  s.throttle = f[0];
  std::memcpy(s.fly, f + 1, 36);
  std::memcpy(&s.position, f + 10, 12);
  std::memcpy(s.matrix, f + 13, 48);
}

// op 0: take_damage(def, coll, damage); 1: part critical(kind, section);
// 2: destroy_part(id); 3: start_limp(leg, fall); 4: knocked_down(kind);
// 5: the torso's hit_section for the world's shot.
// def/coll: flags, heat_2, zaps, zaps_2, force, narc, id, nuke, coll x20, node.
int probe_damage_op(int op, const int32_t* a, int32_t* result, int32_t* out, int cap) {
  g->log.clear();
  Mech& m = g->mech;
  int32_t r = 0;
  switch (op) {
    case 0: {
      g->type.~WeaponType();
      new (&g->type) WeaponType();
      g->type.id = a[6];
      std::memcpy(&g->type.heat_2, &a[1], 4);
      std::memcpy(&g->type.zaps_hud, &a[2], 4);
      std::memcpy(&g->type.zaps_hud_2, &a[3], 4);
      std::memcpy(&g->type.impact_force, &a[4], 4);
      g->type.is_narc = a[5] != 0;
      g->def = WeaponDef{};
      g->def.flags = static_cast<uint32_t>(a[0]);
      g->def.desc = a[7] ? "NUKE" : "LRM20";
      g->def.type = &g->type;
      g->coll.x20 = h(static_cast<uint32_t>(a[8]));
      g->coll.node = h(static_cast<uint32_t>(a[9]));
      float dmg;
      std::memcpy(&dmg, &a[10], 4);
      r = m.take_damage(&g->def, &g->coll, dmg);
      break;
    }
    case 1: r = g->parts[a[0]]->critical(a[1], a[2]); break;
    case 2: m.destroy_part(a[0]); break;
    case 3: r = m.start_limp(a[0], a[1] != 0); break;
    case 4: r = m.knocked_down(a[0]); break;
    case 5: r = m.torso->hit_section(world().shot); break;
    default: break;
  }
  *result = r;
  const int n = static_cast<int>(g->log.size());
  if (n > cap) return -1;
  std::memcpy(out, g->log.data(), 4 * n);
  return n;
}

void probe_damage_store(uint8_t* mech_img, uint8_t* part_imgs, int32_t* groups, int32_t* list, int* nlist,
                        int32_t* freed, int32_t* toggle) {
  mech_io(g->mech, mech_img, true);
  for (int i = 0; i < kParts; ++i) {
    if (g->freed[i]) {
      freed[i] = 1;
      continue;
    }
    freed[i] = 0;
    part_io(*g->parts[i], part_imgs + 0x200 * i, true);
  }
  for (size_t i = 0; i < g->items.size(); ++i) groups[i] = g->items[i]->group;
  int n = 0;
  for (MechPart* p : g->mech.parts) list[n++] = index_of(p);
  *nlist = n;
  *toggle = world().death_anim_toggle;
}

}  // extern "C"
