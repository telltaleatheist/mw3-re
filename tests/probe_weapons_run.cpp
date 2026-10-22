// The weapons at run time (mech3/weapon: manager, groups, bins, projectile and
// beam weapons) for tests/differential/test_weapons_run.py. The definitions
// come from probe_weapons.cpp's run of zwep::init. The world the weapons see
// (the owner mech, its parts and nodes, the view, the target, the clock) is a
// table the test writes before every operation, the same values its emulator
// side puts in memory; handles are the emulator's own addresses, so the calls
// out are logged with the same values on both sides. After each operation the
// manager, its groups, weapons and bins are dumped with pointers as tokens.
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

#include "crt/msvc_rand.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zreader/zreader.h"
#include "gamez/zweapon/zweapon.h"
#include "gamez/zweapon/zwep_init.h"
#include "mech3/mech_config.h"
#include "mech3/mech_names.h"
#include "mech3/mech_reader.h"
#include "mech3/weapon.h"
#include "mech3/weapon_planner.h"

namespace {

using mech3::Handle;
using mech3::Vec3f;

std::vector<int32_t> g_log;
void log(std::initializer_list<int32_t> v) {
  g_log.push_back(static_cast<int32_t>(v.size()));
  g_log.insert(g_log.end(), v);
}
int32_t fb(float f) {
  int32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}
float bf(int32_t i) {
  float f;
  std::memcpy(&f, &i, 4);
  return f;
}
uint32_t u(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
Handle h(uint32_t v) { return reinterpret_cast<Handle>(static_cast<uintptr_t>(v)); }

// The world table; the layout is the test's (test_weapons_run.py, World).
enum W {
  kPartMech, kPlayer, kMissionPlayer, kStats, kLockTargets, kVehicle, kEntity, kMech, kVehicleNode, kEntityNode, kX64,
  kRecord, kTarget, kTargetMech, kTmEcm, kEntityId, kTargetX10, kTargetKey, kTargetProj, kAllies, kTeam,
  kOwnerX58, kFcs, kControl, kHeat, kScreen, kHighlight, kView, kViewKind, kTorsoAim, kTorsoLook,
  kRotA, kRotB = kRotA + 4, kClock = kRotB + 4, kDt, kVel, kSpawn = kVel + 3, kBeam, kNetIds, kNetTargets = kNetIds + 3,
  kTime = kNetTargets + 3, kWorldEnd,
};
int32_t g_w[kWorldEnd];

struct PartRow {
  uint32_t part, node;
  int32_t node_id, water, fail;
  float axis[3], offset[3];
  int32_t equip;  // FCS in this part (by file part id)
  int32_t named;  // the node is called "fp", the firepoints' name
};
PartRow g_parts[16];  // by part id 0..15

std::vector<mech3::Firepoint*> g_fps;
std::vector<std::unique_ptr<mech3::Firepoint>> g_fp_store;
std::unique_ptr<mech3::Config> g_config;
std::vector<std::unique_ptr<mech3::WeaponItem>> g_items;  // the weapons' config items, in add order
std::vector<mech3::ConfigItem*> g_config_items;            // the config's items, in list order
std::unique_ptr<mech3::WeaponManager> g_m;
const mech3::WeaponManager* g_dying = nullptr;
int32_t g_vehicle_fp[2] = {};  // the nodes "firepoint01" and "firepoint" name (test_weapons_run.py)
uint32_t fnv1(const char* s) {
  uint32_t x = 2166136261u;
  for (; s && *s; ++s) x = (x ^ static_cast<uint8_t>(*s)) * 16777619u;
  return x;
}
int32_t vehicle_fp(const char* name) {
  if (!std::strcmp(name, "firepoint01")) return g_vehicle_fp[0];
  if (!std::strcmp(name, "firepoint")) return g_vehicle_fp[1];
  if (!std::strcmp(name, "fp")) return g_vehicle_fp[0];
  return 0;
}
Vec3f g_vel;
uint32_t g_spawned = 0, g_beams = 0, g_sounds = 0;

int32_t slot_of(const mech3::Weapon* w);

const PartRow* part_row(Handle p) {
  for (const PartRow& r : g_parts)
    if (r.part && r.part == u(p)) return &r;
  return nullptr;
}
const PartRow* node_row(Handle n) {
  for (const PartRow& r : g_parts)
    if (r.node && r.node == u(n)) return &r;
  return nullptr;
}

void set_calls() {
  mech3::WeaponCalls& c = mech3::weapon_calls();
  c = mech3::WeaponCalls{};
  c.vehicle_mech = [](Handle) { return h(g_w[kMech]); };
  c.vehicle_entity = [](Handle) { return h(g_w[kEntity]); };
  c.vehicle_node = [](Handle) { return h(g_w[kVehicleNode]); };
  c.add_jump_jet = [](Handle) { log({39}); };
  c.node_find = [](Handle, const char* name) {
    log({37, static_cast<int32_t>(fnv1(name))});
    return h(vehicle_fp(name));
  };
  c.node_by_name = [](Handle, const char* name) {
    log({38, static_cast<int32_t>(fnv1(name))});
    return h(vehicle_fp(name));
  };
  c.vehicle_velocity = [](Handle) -> const Vec3f* {
    g_vel = {bf(g_w[kVel]), bf(g_w[kVel + 1]), bf(g_w[kVel + 2])};
    return &g_vel;
  };
  c.entity_node = [](Handle) { return h(g_w[kEntityNode]); };
  c.entity_x64 = [](Handle) { return h(g_w[kX64]); };
  c.entity_target = [](Handle) { return h(g_w[kRecord]); };
  c.record_target = [](Handle) { return h(g_w[kTarget]); };
  c.target_projectile = [](Handle) { return h(g_w[kTargetProj]); };
  c.entity_allies = [](Handle) { return static_cast<uint32_t>(g_w[kAllies]); };
  c.player_team = [] { return static_cast<uint32_t>(g_w[kTeam]); };
  c.owner_x58 = [](Handle) { return g_w[kOwnerX58]; };
  c.firepoints = [](Handle) { return &g_fps; };
  c.part_by_id = [](Handle, int32_t id) {
    log({1, id});
    return id >= 0 && id < 16 ? h(g_parts[id].part) : nullptr;
  };
  c.part_node = [](Handle p) {
    const PartRow* r = part_row(p);
    return r ? h(r->node) : nullptr;
  };
  c.part_mech = [](Handle) { return h(g_w[kPartMech]); };
  c.node_part = [](Handle n) {
    const PartRow* r = node_row(n);
    log({2, static_cast<int32_t>(u(n))});
    return r ? h(r->part) : nullptr;
  };
  c.node_name = [](Handle n) {
    const PartRow* r = node_row(n);
    return r && r->named ? "fp" : "nofp";
  };
  c.part_in_water = [](Handle p) {
    const PartRow* r = part_row(p);
    return r && r->water != 0;
  };
  c.node_id = [](Handle n) {
    const PartRow* r = node_row(n);
    return r ? r->node_id : -1;
  };
  c.fcs_count = [](Handle) { return g_w[kFcs]; };
  c.equipment_in_part = [](Handle, int32_t kind, int32_t part) {
    log({3, kind, part});
    return part >= 0 && part < 16 && g_parts[part].equip != 0;
  };
  c.config = [](Handle) { return g_config.get(); };
  c.heat = [](Handle) {
    log({4});
    return bf(g_w[kHeat]);
  };
  c.add_heat = [](Handle m, float x) { log({5, static_cast<int32_t>(u(m)), fb(x)}); };
  c.recoil = [](Handle p, float r) { log({6, static_cast<int32_t>(u(p)), fb(r)}); };
  c.net_event = [](Handle o, int32_t k, mech3::Weapon* w) { log({7, static_cast<int32_t>(u(o)), k, slot_of(w)}); };
  c.stats_shot = [](int32_t i) { log({8, i}); };
  c.target_mech = [](Handle) { return h(g_w[kTargetMech]); };
  c.lock_notice = [](Handle tm, Handle v, mech3::WeaponDef* d) {
    log({9, static_cast<int32_t>(u(tm)), static_cast<int32_t>(u(v)), d->index});
  };
  c.ecm_count = [](Handle) { return g_w[kTmEcm]; };
  c.target_position = [](Handle) -> const zwep::Vec3f3* {
    log({10});
    return static_cast<const zwep::Vec3f3*>(h(g_w[kEntityId]));
  };
  c.target_velocity = [](Handle) { return static_cast<const zwep::Vec3f3*>(h(g_w[kTargetX10])); };
  c.target_key = [](Handle) { return h(g_w[kTargetKey]); };
  c.net_target = [](Handle id) -> Handle {
    for (int i = 0; i < 3; ++i)
      if (g_w[kNetIds + i] && static_cast<uint32_t>(g_w[kNetIds + i]) == u(id)) return h(g_w[kNetTargets + i]);
    return nullptr;
  };
  c.node_position = [](Handle n, Vec3f* v) -> int32_t {
    log({11, static_cast<int32_t>(u(n)), fb(v->x), fb(v->y), fb(v->z)});
    const PartRow* r = node_row(n);
    if (!r || r->fail) return 1;
    v->x = v->x + r->offset[0];
    v->y = v->y + r->offset[1];
    v->z = v->z + r->offset[2];
    return 0;
  };
  c.node_axis = [](Handle n) -> const Vec3f* {
    static Vec3f a;
    const PartRow* r = node_row(n);
    a = r ? Vec3f{r->axis[0], r->axis[1], r->axis[2]} : Vec3f{0, 0, 0};
    return &a;
  };
  c.spawn = [](mech3::WeaponDef* d, Handle node, Handle x64, const Vec3f* m, const Vec3f* dir, const Vec3f* v,
               int32_t p5, int32_t p6, bool) -> Handle {
    log({12, d->index, static_cast<int32_t>(u(node)), static_cast<int32_t>(u(x64)), fb(m->x), fb(m->y), fb(m->z),
         fb(dir->x), fb(dir->y), fb(dir->z), v ? fb(v->x) : 0, v ? fb(v->y) : 0, v ? fb(v->z) : 0, p5, p6});
    if (!g_w[kSpawn]) return nullptr;
    return h(static_cast<uint32_t>(g_w[kSpawn]) + 0x600 * (g_spawned++ % 64));
  };
  c.radar_track = [](Handle p, bool fr) { log({13, static_cast<int32_t>(u(p)), fr}); };
  c.beam_create = [](mech3::WeaponDef* d, Handle on, Handle n, const Vec3f* m, const Vec3f* dir) -> Handle {
    log({14, d->index, static_cast<int32_t>(u(on)), static_cast<int32_t>(u(n)), fb(m->x), fb(m->y), fb(m->z),
         fb(dir->x), fb(dir->y), fb(dir->z)});
    if (!g_w[kBeam]) return nullptr;
    return h(static_cast<uint32_t>(g_w[kBeam]) + 0x10 * (g_beams++ % 256));
  };
  c.beam_start = [](Handle b) { log({15, static_cast<int32_t>(u(b))}); };
  c.beam_stop = [](Handle b) { log({16, static_cast<int32_t>(u(b))}); };
  c.beam_free = [](Handle b) -> Handle {
    log({17, static_cast<int32_t>(u(b))});
    return nullptr;
  };
  c.mech_sound = [](int32_t i) { log({18, i}); };
  c.play_loop = [](Handle s) -> Handle {
    log({19, static_cast<int32_t>(u(s))});
    return h(0x7a000000u + 0x10 * (++g_sounds));
  };
  c.play_at = [](Handle s, const Vec3f* p) -> Handle {
    log({20, static_cast<int32_t>(u(s)), fb(p->x), fb(p->y), fb(p->z)});
    return h(0x7a000000u + 0x10 * (++g_sounds));
  };
  c.stop_sound = [](Handle s) { log({21, static_cast<int32_t>(u(s))}); };
  c.destroyed_sounds = [](Handle a, Handle b) { log({22, static_cast<int32_t>(u(a)), static_cast<int32_t>(u(b))}); };
  c.voice_destroyed = [](int32_t i) { log({23, i}); };
  c.screen = [](Handle) { return g_w[kScreen] != 0; };
  c.hud_screen = [](Handle) { return g_w[kScreen] ? h(0x5c5c) : nullptr; };
  c.gauge_6c = [](Handle) {
    log({35, 0x6c});
    return 0x6c6c;
  };
  c.gauge_68 = [](Handle) {
    log({35, 0x68});
    return 0x6868;
  };
  c.hud_440a30 = [](Handle, int32_t a, int32_t b) {
    log({33, a, b});
    return 0x4444;
  };
  c.hud_443d20 = [](Handle, int32_t v) { log({34, v}); };
  // The screen may be missing: the original calls these on 0 + 0x6520 then.
  c.hud_groups = [](Handle) { log({24, g_w[kScreen] != 0}); };
  c.hud_weapons = [](Handle) { log({25, g_w[kScreen] != 0}); };
  c.hud_fire = [](Handle) { log({26, g_w[kScreen] != 0}); };
  c.hud_highlight = [](Handle) -> mech3::Weapon* {
    const int32_t s = g_w[kHighlight];
    return s >= 0 && g_m && static_cast<uint32_t>(s) < g_m->count ? g_m->weapons[s] : nullptr;
  };
  c.view = [] { return h(g_w[kView]); };
  c.view_kind = [](Handle) { return g_w[kViewKind]; };
  c.control_mode = [](Handle) { return g_w[kControl]; };
  c.torso_aim = [](Handle) -> const zmth::Quat* {
    static zmth::Quat q;
    log({27});
    q = {bf(g_w[kRotA]), bf(g_w[kRotA + 1]), bf(g_w[kRotA + 2]), bf(g_w[kRotA + 3])};
    return &q;
  };
  c.torso_look = [](Handle) -> const zmth::Quat* {
    static zmth::Quat q;
    q = {bf(g_w[kRotB]), bf(g_w[kRotB + 1]), bf(g_w[kRotB + 2]), bf(g_w[kRotB + 3])};
    return g_w[kTorsoLook] ? &q : nullptr;
  };

}

void sync_world() {
  mech3::WeaponWorld& w = mech3::weapon_world();
  w.player = h(g_w[kPlayer]);
  w.mission_player = h(g_w[kMissionPlayer]);
  w.stats = g_w[kStats] != 0;
  w.lock_targets = g_w[kLockTargets] != 0;
  w.clock = bf(g_w[kClock]);
  w.dt = bf(g_w[kDt]);
  w.time = bf(g_w[kTime]);
}

int32_t slot_of(const mech3::Weapon* w) {
  const mech3::WeaponManager* m = g_m ? g_m.get() : g_dying;  // the builder deletes the old manager
  if (!w || !m) return -1;
  for (uint32_t i = 0; i < m->count; ++i)
    if (m->weapons[i] == w) return static_cast<int32_t>(i);
  return -2;
}
int32_t bin_token(const mech3::AmmoBin* b) {
  if (!b) return -1;
  for (uint32_t i = 0; i < g_m->count; ++i)
    if (g_m->weapons[i]->bin == b) return static_cast<int32_t>(i);
  return -2;
}
int32_t fp_token(const void* p) {
  for (size_t i = 0; i < g_fps.size(); ++i) {
    if (g_fps[i] == p) return static_cast<int32_t>(i);
    if (g_fps[i]->position == p) return 0x100 + static_cast<int32_t>(i);
  }
  return p ? -2 : -1;
}
int32_t item_token(const mech3::WeaponItem* it) {
  for (size_t i = 0; i < g_items.size(); ++i)
    if (g_items[i].get() == it) return static_cast<int32_t>(i);
  return it ? -2 : -1;
}
// A pointer into a weapon (its lock flag or target) as slot x 0x100 + offset.
int32_t weapon_ptr_token(const void* p) {
  if (!p) return -1;
  for (uint32_t i = 0; g_m && i < g_m->count; ++i) {
    const mech3::Weapon* w = g_m->weapons[i];
    if (p == &w->lock_flag) return static_cast<int32_t>(i * 0x100 + 4);
    if (p == &w->lock) return static_cast<int32_t>(i * 0x100 + 8);
  }
  return -2;
}

void put_weapon(std::vector<int32_t>& o, const mech3::Weapon* w) {
  o.insert(o.end(), {w->lock_flag, static_cast<int32_t>(u(w->lock.entity)), static_cast<int32_t>(u(w->lock.at)), static_cast<int32_t>(u(w->lock.velocity)),
                     static_cast<int32_t>(u(w->projectile_target)), fb(w->rating), w->slot,
                     static_cast<int32_t>(u(w->node)), static_cast<int32_t>(u(w->part)), w->index,
                     w->def ? w->def->index : -1, fb(w->reload), fb(w->target.x), fb(w->target.y), fb(w->target.z),
                     fb(w->muzzle.x), fb(w->muzzle.y), fb(w->muzzle.z), fb(w->direction.x), fb(w->direction.y),
                     fb(w->direction.z), bin_token(w->bin), w->locked, w->fcs, w->group, w->salvo, w->salvo_left,
                     w->x6c, fb(w->damage), w->enabled, static_cast<int32_t>(u(w->loop_sound)),
                     static_cast<int32_t>(u(w->recharge_sound)), fp_token(w->firepoint), fb(w->spread.x),
                     fb(w->spread.y), fb(w->spread.z), item_token(w->item)});
  if (w->is_beam()) {
    const auto* b = static_cast<const mech3::BeamWeapon*>(w);
    o.insert(o.end(), {b->firing, static_cast<int32_t>(u(b->beam))});
  } else {
    o.insert(o.end(), {0, 0});
  }
  const mech3::AmmoBin* b = w->bin;
  if (b && bin_token(b) == w->slot) {
    o.insert(o.end(), {b->refs, b->amount, b->max, b->infinite, b->warned, b->manager ? 1 : 0, fb(b->damage)});
  } else {
    o.insert(o.end(), {0, 0, 0, 0, 0, 0, 0});
  }
}

}  // namespace

extern "C" {

void probe_wm_world(const int32_t* w, const float* parts, int npart) {
  std::memcpy(g_w, w, sizeof g_w);
  for (int i = 0; i < 16 && i < npart; ++i) {
    const float* r = parts + i * 13;
    PartRow& p = g_parts[i];
    std::memcpy(&p.part, &r[0], 4);
    std::memcpy(&p.node, &r[1], 4);
    std::memcpy(&p.node_id, &r[2], 4);
    std::memcpy(&p.water, &r[3], 4);
    std::memcpy(&p.fail, &r[4], 4);
    for (int k = 0; k < 3; ++k) p.axis[k] = r[5 + k], p.offset[k] = r[8 + k];
    std::memcpy(&p.equip, &r[11], 4);
    std::memcpy(&p.named, &r[12], 4);
  }
  sync_world();
}

// A new manager on the vehicle, with the mech's firepoints and the config's
// items (kind, count, group, part, id, extra) for the crits.
void probe_wm_new(const float* fps, int nfp, const int32_t* items, int nitems, uint32_t seed) {
  set_calls();
  g_m.reset();  // the emulator side never frees its old manager: drop what this logs
  g_log.clear();
  for (zwep::WeaponDef& d : zwep::state().defs) zwep::attach_fire(&d, nullptr);
  g_items.clear();
  g_fp_store.clear();
  g_fps.clear();
  for (int i = 0; i < nfp; ++i) {
    const float* r = fps + i * 8;
    auto f = std::make_unique<mech3::Firepoint>();
    f->name = "fp";
    f->radius = r[0];
    f->share = r[1];
    std::memcpy(&f->part, &r[2], 4);
    std::memcpy(&f->model_part, &r[3], 4);
    for (int k = 0; k < 3; ++k) f->position[k] = r[4 + k];
    g_fps.push_back(f.get());
    g_fp_store.push_back(std::move(f));
  }
  g_config = std::make_unique<mech3::Config>(0, "test");
  for (mech3::ConfigItem* it : g_config->items) delete it;
  g_config->items.clear();
  for (int i = 0; i < nitems; ++i) {
    const int32_t* r = items + i * 6;
    mech3::ConfigItem* it;
    if (r[0] == 1) {
      auto* w = new mech3::WeaponItem;
      w->weapon = r[4];
      it = w;
    } else if (r[0] == 2) {
      auto* a = new mech3::AmmoItem;
      a->weapon = r[4];
      it = a;
    } else {
      auto* e = new mech3::EquipmentItem;
      e->equipment = r[5];
      it = e;
    }
    it->count = r[1];
    it->group = r[2];
    it->part = r[3];
    g_config->items.push_back(it);
  }
  crt::msvc_srand(seed);
  g_spawned = g_beams = g_sounds = 0;
  g_m = std::make_unique<mech3::WeaponManager>(h(g_w[kVehicle]));
}

void probe_wm_vehicle_fp(int32_t a, int32_t b) { g_vehicle_fp[0] = a, g_vehicle_fp[1] = b; }

// The operations of test_weapons_run.py; returns the result, logs the calls.
int32_t probe_wm_op(int op, const int32_t* a) {
  sync_world();
  mech3::WeaponManager& m = *g_m;
  int32_t r = 0;
  switch (op) {
    case 0: {  // add a weapon from a new config item (count, group, firepoint, weapon, tech)
      auto it = std::make_unique<mech3::WeaponItem>();
      it->count = a[0], it->group = a[1], it->part = a[2], it->weapon = a[3], it->tech = a[4];
      mech3::Weapon* w = m.add(it.get());
      g_items.push_back(std::move(it));
      r = slot_of(w);
      break;
    }
    case 1: {  // ammo into the first weapon of an index
      mech3::Weapon* w = m.find(a[0], 0);
      if (w && w->bin) w->bin->add(a[1]);
      r = slot_of(w);
      break;
    }
    case 2: m.set_trigger(a[0] != 0); m.update(bf(a[1])); break;
    case 3: r = m.select_group(a[0]); break;
    case 4: r = m.next_group(); break;
    case 5: r = m.prev_group(); break;
    case 6: m.next_weapon(); break;
    case 7: m.prev_weapon(); break;
    case 8: m.mode = (m.mode + 1) % 3; m.mode_sound(); break;
    case 9: m.alpha_strike(bf(a[0])); break;
    case 10: r = m.jettison(); break;
    case 11: {
      const Vec3f v{bf(a[1]), bf(a[2]), bf(a[3])};
      m.set_aim(a[0] ? &v : nullptr);
      break;
    }
    case 12: m.set_online(a[0] != 0); break;
    case 13: r = m.critical(a[0], a[1] != 0); break;
    case 14: m.part_lost(a[0]); break;
    case 15: m.destroy_all(); break;
    case 16: m.set_unlimited(a[0] != 0); break;
    case 17: r = m.arm_restriction(); break;
    case 18: r = m.net_fire(static_cast<uint32_t>(a[0]), h(static_cast<uint32_t>(a[1])), a[2]); break;
    case 19: if (static_cast<uint32_t>(a[0]) < m.count) r = m.weapons[a[0]]->set_group(a[1]); break;
    case 20: m.forget_target(h(static_cast<uint32_t>(a[0]))); break;
    case 21: if (static_cast<uint32_t>(a[0]) < m.count) m.weapons[a[0]]->add_damage(bf(a[1])); break;
    case 22: m.reset(); zwep::set_lock(nullptr, nullptr); break;
    case 23: m.has_target = a[0] != 0; break;
    case 24: {  // the vehicle form: a weapon by index on a node
      mech3::Weapon* w = m.add(a[0], h(static_cast<uint32_t>(a[1])), a[2]);
      r = slot_of(w);
      break;
    }
    case 26: case 27: case 28: {  // a def callback: (def, hit point, owner, dir, pos, speed)
      zwep::WeaponDef& d = zwep::state().defs[static_cast<size_t>(a[0])];
      zwep::HitRecord hit;
      hit.point = {bf(a[1]), bf(a[2]), bf(a[3])};
      zwep::Projectile p;
      p.owner = h(static_cast<uint32_t>(a[4]));
      p.direction = {bf(a[5]), bf(a[6]), bf(a[7])};
      p.position = {bf(a[8]), bf(a[9]), bf(a[10])};
      p.speed = bf(a[11]);
      auto fn = op == 26 ? mech3::salvo_spread : op == 27 ? mech3::airstrike : mech3::burst;
      r = static_cast<int32_t>(fn(&d, &hit, &p));
      break;
    }
    case 30: m.ammo_gauge(); break;
    case 32: {  // the mech's configuration step (0x00414501..0x00414732)
      int32_t equipment[15] = {};
      mech3::Ams ams(150.0f, 0.6f, 100);
      mech3::Config* kept = nullptr;
      zerr::set_sink([](int level, const char*, int line, const char*) { log({15, level, line}); });
      mech3::configure_weapons(h(g_w[kMech]), &m, g_config.get(), &kept, equipment, &ams, h(a[0]));
      zerr::set_sink(nullptr);
      std::vector<int32_t> v{40};
      v.insert(v.end(), equipment, equipment + 15);
      v.push_back(ams.rounds);
      v.push_back(kept == g_config.get() ? 1 : 0);
      g_log.push_back(static_cast<int32_t>(v.size()));
      g_log.insert(g_log.end(), v.begin(), v.end());
      break;
    }
    case 31: {  // the vehicle builder on a made-up `weapons` list (test_weapons_run.py, vehicle_op)
      static const char* const kFp[] = {"firepoint01", "fpX", "firepoint", "firepoint02"};
      std::vector<zrdr::Node*> keep;
      auto list = [&](int32_t n) {
        auto* l = new zrdr::Node[static_cast<size_t>(n) + 1]();
        l[0].type = zrdr::Int;
        l[0].i = n;
        keep.push_back(l);
        return l;
      };
      auto str = [](zrdr::Node& v, const char* t) {
        v.type = zrdr::String;
        v.s = const_cast<char*>(t);
      };
      const int32_t n = a[0];
      zrdr::Node* items = list(n + 1);
      for (int32_t i = 0; i < n; ++i) {
        const int32_t* r = a + 1 + 4 * i;
        const bool fp = r[2] != 0;
        zrdr::Node* it = list(fp ? 11 : 9);
        str(it[1], r[0] >= 0 ? mech3::weapon_name(r[0]) : r[0] == -1 ? "BOGUS" : "TAG_MISSILE");
        it[2] = {zrdr::Int, {1}}, it[3] = {zrdr::Int, {1}};
        it[4].type = zrdr::Int, it[4].i = r[1];
        it[5].type = zrdr::Float, it[5].f = 5.0f;
        it[6].type = zrdr::Float, it[6].f = 25.0f;
        it[7].type = zrdr::Float, it[7].f = 100000.0f;
        it[8] = {zrdr::Int, {0}};
        if (fp) {
          str(it[9], "firepoint");
          zrdr::Node* f = list(3);
          str(f[1], kFp[r[3]]);
          str(f[2], "firepoint02");
          it[10].type = zrdr::List, it[10].list = f;
        }
        items[i + 1].type = zrdr::List, items[i + 1].list = it;
      }
      zrdr::Node* root = list(3);
      str(root[1], "weapons");
      root[2].type = zrdr::List, root[2].list = items;
      zrdr::Node top{};
      top.type = zrdr::List, top.list = root;
      zerr::set_sink([](int level, const char*, int line, const char*) { log({15, level, line}); });
      g_dying = g_m.get();
      mech3::WeaponManager* nm = mech3::build_vehicle_weapons(h(g_w[kVehicle]), g_m.release(), &top);
      g_dying = nullptr;
      zerr::set_sink(nullptr);
      g_m.reset(nm);
      r = nm ? 1 : 0;
      for (zrdr::Node* l : keep) delete[] l;
      if (!g_m) g_m = std::make_unique<mech3::WeaponManager>(h(g_w[kVehicle]));  // the test keeps a manager
      return r;
    }
    case 29: {  // the AI's wait and the mech lab's rounds
      if (static_cast<uint32_t>(a[0]) < m.count) {
        mech3::Weapon* w = m.weapons[a[0]];
        log({31, w->type_ready()});
        if (a[1] && w->def && w->def->type) w->def->type->hold_for_flight();
      }
      if (a[2]) mech3::reset_ready_times();
      log({32, mech3::lab_rounds(a[3], a[4])});
      break;
    }
    case 25: {  // vt calls a test can make directly: dps, ready, ammo, can_fire, ranges
      if (static_cast<uint32_t>(a[0]) < m.count) {
        mech3::Weapon* w = m.weapons[a[0]];
        log({30, fb(w->dps()), fb(w->ready()), w->ammo(), w->can_fire(), fb(w->min_range_sq()),
             fb(w->max_range()), fb(w->max_range_sq())});
      }
      break;
    }
    default: break;
  }
  return r;
}

int probe_wm_log(int32_t* out, int cap) {
  if (static_cast<int>(g_log.size()) > cap) return -1;
  std::memcpy(out, g_log.data(), g_log.size() * 4);
  const int n = static_cast<int>(g_log.size());
  g_log.clear();
  return n;
}

// The manager's state; see the test's read_state for the layout.
int probe_wm_state(int32_t* out, int cap) {
  std::vector<int32_t> o;
  const mech3::WeaponManager& m = *g_m;
  o.insert(o.end(), {m.trigger_down, m.trigger_was, m.state, fb(m.alpha_end), fb(m.aim.x), fb(m.aim.y), fb(m.aim.z),
                     m.aim_valid, m.mode, m.destroyed, static_cast<int32_t>(m.count), m.group,
                     static_cast<int32_t>(u(m.owner)), static_cast<int32_t>(u(m.vehicle)), m.has_target, m.online,
                     fb(m.ammo_value), fb(m.ammo_value_inv)});
  for (const mech3::WeaponGroup* g : m.groups) {
    o.insert(o.end(), {g->index, static_cast<int32_t>(g->count), g->current, slot_of(g->current_weapon)});
    for (uint32_t i = 0; i < g->count; ++i) o.push_back(slot_of(g->weapons[i]));
  }
  for (uint32_t i = 0; i < m.count; ++i) put_weapon(o, m.weapons[i]);
  const zwep::FireState& f = zwep::fire_state();
  o.insert(o.end(), {weapon_ptr_token(f.lock_flag), weapon_ptr_token(f.lock_target), f.beam_ground_fx,
                     fb(f.damage_scale), fb(f.beam_range_scale), fb(f.track_scale[0]), fb(f.track_scale[1]),
                     f.terrain_follow, fb(mech3::weapon_world().cos_35), mech3::weapon_world().cos_35_made});
  for (const zwep::WeaponDef& d : zwep::state().defs)
    o.insert(o.end(), {d.fire.sound_choice, d.fire.animation_choice, static_cast<int32_t>(u(d.fire.attach_node)),
                       fp_token(d.fire.attach_offset)});
  for (const zwep::WeaponDef& d : zwep::state().defs) {
    const mech3::WeaponType* t = d.type;
    if (!t) continue;
    o.push_back(static_cast<int32_t>(t->salvo_marks.size()));
    for (const auto* mk : t->salvo_marks) o.insert(o.end(), {fb(mk->at.x), fb(mk->at.y), fb(mk->at.z), fb(mk->until)});
    o.insert(o.end(), {fb(t->strike_point.x), fb(t->strike_point.y), fb(t->strike_point.z), fb(t->ready_at)});
  }
  if (static_cast<int>(o.size()) > cap) return -1;
  std::memcpy(out, o.data(), o.size() * 4);
  return static_cast<int>(o.size());
}

// A def's fire effect flags (bit 1: attached to the firepoint).
void probe_wm_fire_flag(int def, uint32_t flags) { zwep::state().defs[static_cast<size_t>(def)].fire.flags = flags; }

// The zWeapon fire state as the startup leaves it (0x00583750).
void probe_wm_reset_fire() { zwep::fire_state() = zwep::FireState{}; }

// The planner's three getters (test_weapon_planner.py) with the manager's
// mode and group set for the call, then put back.
void probe_wm_planner(int32_t mode, int32_t group, int32_t* out) {
  mech3::WeaponManager& m = *g_m;
  const int32_t was_mode = m.mode, was_group = m.group;
  m.mode = mode, m.group = group;
  out[0] = fb(mech3::group_max_range(m));
  out[1] = mech3::group_locks_on(m);
  out[2] = mech3::group_has_ammo(m);
  m.mode = was_mode, m.group = was_group;
}

// The volley pick (test_weapon_planner.py): the pilot (or none), its hold
// and alpha share, the target's distance squared (or none), the owner's
// heat, the rolls. out: the answer and the rolls used.
const int32_t* g_rolls = nullptr;
int32_t g_rolls_used = 0;
int32_t next_roll() { return g_rolls[g_rolls_used++]; }
void probe_wm_volley(int32_t pilot, float hold, float chance, int32_t has_target, float dist2, float heat,
                     const int32_t* rolls, int32_t* out) {
  mech3::WeaponManager& m = *g_m;
  const mech3::VolleyPilot p{hold, chance};
  g_rolls = rolls, g_rolls_used = 0;
  out[0] = mech3::volley_pick(m, pilot ? &p : nullptr, has_target ? &dist2 : nullptr, m.owner ? &heat : nullptr,
                              next_roll);
  out[1] = g_rolls_used;
}

}  // extern "C"
