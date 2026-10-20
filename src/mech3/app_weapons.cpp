// The weapons' hooks wired to the program (docs/weapons.md, In the program):
// the weapon manager's calls into the mech (mech3/weapon), zWeapon's into
// the scene graph, object3d and cls_di.c's queries (gamez/zweapon), and the
// mechs' damage handler. Not original code. Visuals (lights, animations,
// craters, marks) and sounds stay unset: the shots fly and hit without them.
#include <cstdint>
#include <cstring>

#include "gamez/zclass/cls_di.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_util.h"
#include "gamez/zclass/cls_world.h"
#include "gamez/zclass/light.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zweapon/zweapon.h"
#include "gamez/zweapon/zwep_init.h"
#include "mech3/mech.h"
#include "mech3/mech_move.h"
#include "mech3/mission_world.h"
#include "mech3/weapon.h"

namespace mech3 {

int32_t heat();  // 0x0040cfc0, mech3/options.h (not includable with mech.h)

namespace {
zcls::Node* zn(Handle h) { return static_cast<zcls::Node*>(h); }
Mech* mech_of(Handle v) { return static_cast<Mech*>(v); }

// The query record (0x504 bytes, 32 slots) into zWeapon's list (31 slots).
void to_hits(const zcls::LineHits& in, zwep::HitList* out) {
  out->count = in.count;
  out->total = in.total;
  const int n = in.count < 31 ? in.count : 31;
  const int t = in.total < 31 ? in.total : 31;
  const int k = n > t ? n : t;
  for (int i = 0; i < k; ++i) std::memcpy(&out->rec[i], &in.hit[i], sizeof(zwep::HitRecord));
}
static_assert(sizeof(zwep::HitRecord) == sizeof(zcls::LineHit), "the 0x28-byte slot");

zcls::Node* qworld() { return world_hooks().world; }

int32_t segment_list(const zwep::Vec3f3* a, const zwep::Vec3f3* b, zwep::HitList* out) {
  static zcls::LineHits r;
  if (!qworld()) return out->count = 0, 1;
  const int32_t res = zcls::segment_list(qworld(), &a->x, &b->x, &r);
  to_hits(r, out);
  return res;
}

int32_t segment_nearest(const zwep::Vec3f3* a, const zwep::Vec3f3* b, zwep::HitList* out) {
  static zcls::LineHits r;
  if (!qworld()) return out->count = 0, 1;
  const int32_t res = zcls::segment_nearest(qworld(), &a->x, &b->x, &r);
  to_hits(r, out);
  return res;
}

// Mech3.exe 1.2 @ 0x004161a0: the mech's damage handler, vtable +0x164
// (Mech::TakeDamage) on the hit.
uint32_t mech_damage(zwep::Handle object, zwep::WeaponDef* def, zwep::HitRecord* hit, float damage) {
  Collision c;
  c.x20 = hit->material;
  c.node = hit->node;
  return static_cast<uint32_t>(static_cast<Mech*>(object)->take_damage(def, &c, damage));
}

// Mech3.exe 1.2 @ 0x0041d510: a part by its id (docs/structs.md, Parts).
MechPart* part_by_id(Mech& m, int32_t id) {
  switch (id) {
    case 1: case 7: case 8: return m.torso;
    case 2: return m.arms[0];
    case 3: return m.arms[1];
    case 4: return m.legs[0];
    case 5: return m.legs[1];
    case 6: return m.head;
    case 9: return m.arms[0] ? m.arms[0]->hand : nullptr;
    case 10: return m.arms[1] ? m.arms[1]->hand : nullptr;
    case 11: return m.legs[0] ? m.legs[0]->calf : nullptr;
    case 12: return m.legs[1] ? m.legs[1]->calf : nullptr;
    case 13: return m.legs[0] ? m.legs[0]->foot : nullptr;
    case 14: return m.legs[1] ? m.legs[1]->foot : nullptr;
    case 15: return m.hip;
    default: return nullptr;
  }
}

uint32_t g_zone;  // the spawn's zone word: the entity's +0x64 byte
}  // namespace

void wire_weapons() {
  WeaponCalls& c = weapon_calls();
  c.vehicle_entity = [](Handle v) { return v; };                                          // vt+0x2c: itself
  c.vehicle_velocity = [](Handle v) { return reinterpret_cast<const Vec3f*>(&mech_of(v)->velocity); };
  c.entity_node = [](Handle e) { return mech_of(e)->node; };                              // +0x04
  c.entity_x64 = [](Handle e) -> Handle {                                                  // &+0x64
    g_zone = mech_of(e)->x64;
    return &g_zone;
  };
  c.entity_allies = [](Handle e) { return mech_of(e)->enemies; };                         // +0xc8
  c.player_team = [] { return mech3::world().mission_player ? mech3::world().mission_player->team : 0u; };
  c.part_by_id = [](Handle owner, int32_t id) -> Handle { return part_by_id(*mech_of(owner), id); };
  c.part_node = [](Handle part) { return static_cast<MechPart*>(part)->node; };          // +0x08
  c.part_mech = [](Handle part) -> Handle { return static_cast<MechPart*>(part)->mech; };
  c.node_id = [](Handle node) { return node ? zn(node)->get<int32_t>(0x28) : -1; };
  c.node_name = [](Handle node) { return zn(node)->name(); };
  c.config = [](Handle owner) { return mech_of(owner)->config; };                         // +0x3ec
  c.heat = [](Handle owner) { return mech_of(owner)->heat; };                             // vt+0x9c
  c.add_heat = [](Handle owner, float h) {  // 0x00419e40
    if (owner) mech_of(owner)->add_heat(h);
  };                // 0x00419e40
  c.node_position = [](Handle node, Vec3f* p) { return zcls::node_world_point(zn(node), &p->x); };  // 0x004ed650
  c.node_axis = [](Handle node) {  // node +0x38 +0x78: the world matrix's z row
    return reinterpret_cast<const Vec3f*>(static_cast<uint8_t*>(zn(node)->data) + 0x78);
  };

  zwep::RunHooks& r = zwep::run_hooks();
  r.parent_count = [](zwep::Handle n) { return zn(n)->parent_count(); };
  r.first_parent = [](zwep::Handle n) -> zwep::Handle { return zn(n)->parent_count() ? zn(n)->parents[0] : nullptr; };
  r.child_count = [](zwep::Handle n) { return zn(n)->child_count(); };
  r.first_child = [](zwep::Handle n) -> zwep::Handle { return zn(n)->child_count() ? zn(n)->children[0] : nullptr; };
  r.parent = [](zwep::Handle n, int32_t i) -> zwep::Handle { return zn(n)->parents[i]; };
  r.child = [](zwep::Handle n, int32_t i) -> zwep::Handle { return zn(n)->children[i]; };
  r.add_child = [](zwep::Handle p, zwep::Handle c) { zcls::node_add_child(zn(p), zn(c)); };
  r.remove_child = [](zwep::Handle p, zwep::Handle c) { zcls::node_remove_child(zn(p), zn(c)); };
  r.obj_add_child = [](zwep::Handle p, zwep::Handle c) { zcls::object3d_add_child(zn(p), zn(c)); };
  r.obj_remove_child = [](zwep::Handle p, zwep::Handle c) { zcls::object3d_remove_child(zn(p), zn(c)); };
  r.clone = [](zwep::Handle n) -> zwep::Handle { return zcls::tree_copy(zn(n), 0, 1); };
  r.link = [](zwep::Handle n) -> zwep::Handle { return zn(n)->owner; };                    // +0x40
  r.set_link = [](zwep::Handle n, zwep::Handle next) { zn(n)->owner = next; };
  r.set_shootable = [](zwep::Handle n, zwep::Projectile* p) {  // FLYOUT_HEALTH
    zcls::Node* node = zn(n);
    if (!p) {
      node->set_flags(node->flags() & ~0x8000000u);
      return;
    }
    node->set_flags(node->flags() | 0x8000000u);
    node->damage = reinterpret_cast<void*>(uintptr_t{1});
    node->owner = p;
  };
  r.clear_handler = [](zwep::Handle n) { zn(n)->damage = nullptr; };
  r.handler = [](zwep::Handle n) { return reinterpret_cast<uintptr_t>(zn(n)->damage); };
  r.set_handler = [](zwep::Handle n, uintptr_t h) { zn(n)->damage = reinterpret_cast<void*>(h); };
  r.node_frame = [](zwep::Handle n) { return zn(n)->get<int32_t>(0xc4); };
  r.node_flags = [](zwep::Handle n) { return zn(n)->flags(); };
  r.set_active = [](zwep::Handle n, int32_t on) { zcls::node_set_active(zn(n), on); };
  r.set_name = [](zwep::Handle n, const char* s) { zcls::node_set_name(zn(n), s); };
  r.set_flag_4eb620 = [](zwep::Handle n, int32_t on) { zcls::node_set_flag8(zn(n), on); };
  r.set_flag_4eb750 = [](zwep::Handle n, int32_t on) { zcls::node_set_intersect_bbox(zn(n), on); };
  r.set_flag_4eb790 = [](zwep::Handle n, int32_t on) { zcls::node_set_proximity(zn(n), on); };
  r.node_free = [](zwep::Handle n) { zcls::tree_destroy(zn(n)); };
  r.node_destroy = [](zwep::Handle n) { zcls::node_delete(zn(n)); };
  r.object3d_create = []() -> zwep::Handle { return zcls::object3d_create(); };
  r.set_scale = [](zwep::Handle n, float x, float y, float z) { zcls::object3d_set_scale(zn(n), x, y, z); };
  r.set_rotation = [](zwep::Handle n, float x, float y, float z) { zcls::object3d_set_rotation(zn(n), x, y, z); };
  r.set_translation = [](zwep::Handle n, float x, float y, float z) { zcls::object3d_set_translation(zn(n), x, y, z); };
  r.rotate = [](zwep::Handle n, float x, float y, float z) { zcls::object3d_add_rotation(zn(n), x, y, z); };
  r.set_uniform_scale = [](zwep::Handle n, float v) { zcls::object3d_set_opacity(zn(n), v); };  // 0x004fba20
  r.set_flag_4fbb90 = [](zwep::Handle n, int32_t on) { zcls::object3d_use_opacity(zn(n), on); };
  r.light_create = []() -> zwep::Handle { return zcls::light_create(); };                  // 0x004fa720
  r.light_position = [](zwep::Handle l, float x, float y, float z) { zcls::light_set_position(zn(l), x, y, z); };
  r.light_range = [](zwep::Handle l, float a, float b) { zcls::light_set_range(zn(l), a, b); };
  r.light_color = [](zwep::Handle l, float cr, float cg, float cb) { zcls::light_set_color(zn(l), cr, cg, cb); };
  r.world_add_light = [](zwep::Handle l) {
    if (qworld()) zcls::world_add_light(qworld(), zn(l));
  };
  r.world_remove_light = [](zwep::Handle l) {
    if (qworld()) zcls::world_remove_light(qworld(), zn(l));
  };
  // impact_type (0x005426a0: [0x0074a2b0], read by nothing ported) stays unset.
  r.query_static = [](int32_t on) { zcls::segment_options().static_only = on; };           // 0x004ed9d0
  r.query_since = [](int32_t on, uint32_t frame) {                                         // 0x004ed9e0
    zcls::segment_options().since = on ? static_cast<int32_t>(frame) : -1;
  };
  r.query_mask = [](uint32_t mask) { zcls::segment_options().exclude = mask; };            // 0x004eda00
  r.segment_list = segment_list;                                                            // 0x004eed90
  r.segment = segment_nearest;                                                              // 0x004eece0

  zwep::HitHooks& h = zwep::hit_hooks();
  h.node_handler = [](zwep::Handle n) { return reinterpret_cast<uintptr_t>(zn(n)->damage); };  // +0xbc
  h.node_projectile = [](zwep::Handle n) { return static_cast<zwep::Projectile*>(zn(n)->owner); };
  h.handler_live = [](uintptr_t t) { return reinterpret_cast<zwep::NodeHandlers*>(t)->slot[0].fn != nullptr; };
  h.call_handler = [](uintptr_t t, zwep::WeaponDef* def, zwep::HitRecord* hit, float damage) {
    const auto& s = reinterpret_cast<zwep::NodeHandlers*>(t)->slot[0];
    return reinterpret_cast<zwep::DamageFn>(s.fn)(s.object, def, hit, damage);
  };
  h.soil = [](zwep::Handle m) { return m ? static_cast<int32_t>(static_cast<zmod::Material*>(m)->soil) : 0; };
  h.node_intersect = [](zwep::Handle n) { return (zn(n)->flags() & 0x10) != 0; };
  h.set_intersect = [](zwep::Handle n, int32_t on) { zcls::node_set_intersect(zn(n), on); };
  h.query = [](zwep::Handle w, const zwep::Vec3f3* at, float radius, zwep::HitList* out) {  // 0x004f0ac0
    static zcls::LineHits r;
    zcls::Node* node = w ? zn(w) : qworld();
    if (!node) return out->count = 0, 1;
    const int32_t res = zcls::sphere_query(node, &at->x, radius, nullptr, 1, 1, &r);
    to_hits(r, out);
    return res;
  };

  heat_calls().heat_option = [] { return heat() != 0; };  // 0x0040cfc0, the Heat option

  // Mech3.exe 1.2 @ 0x0040ea10 / 0x0040e9d0 (through 0x0040ef30 / 0x0040ef10):
  // what a node hit belongs to, by its +0x28 id and +0x40 owner (the build
  // marks each part's subtree, mech_part_build.cpp).
  DamageCalls& d = damage_calls();
  d.node_part = [](Handle n) -> MechPart* {
    zcls::Node* z = zn(n);
    return z && (z->get<uint32_t>(0x28) & 0xff) ? static_cast<MechPart*>(z->owner) : nullptr;
  };
  d.node_owner = [](Handle n) -> Handle {
    zcls::Node* z = zn(n);
    if (!z) return nullptr;
    const uint32_t id = z->get<uint32_t>(0x28);
    if (id & 0xf000) return z->owner;
    if (id & 0xff) return static_cast<MechPart*>(z->owner)->mech;  // 0x00460000
    return nullptr;
  };

  UpdateCalls& u = update_calls();
  u.weapons_fire = [](Mech& m, uint8_t fire) {  // 0x004b0c20 on +0xd4
    if (auto* w = static_cast<WeaponManager*>(m.ammo)) w->set_trigger(fire != 0);
  };
  u.weapons_update = [](Mech& m, float dt) {  // 0x004b0c30 on +0xd4
    if (auto* w = static_cast<WeaponManager*>(m.ammo)) w->update(dt);
  };
}

// The frame's step [0x0072c52c] into each module's copy of it, once a frame
// before the vehicles update (the exe has one global).
void sync_frame_step(float dt) {
  mech3::world().dt = dt;
  weapon_world().dt = dt;
  zwep::hit_state().dt = dt;
}

// What F1..F5 select (CMD_WEPGRP_1..5 through the mech's command handler,
// not ported): the first group holding a weapon that heats its mech; -1 none.
int32_t mech_select_hot_group(void* mech) {
  auto* wm = static_cast<WeaponManager*>(static_cast<Mech*>(mech)->ammo);
  if (!wm) return -1;
  for (int32_t g = 0; g < 5; ++g)
    for (uint32_t i = 0; wm->groups[g] && i < wm->groups[g]->count; ++i) {
      const Weapon* w = wm->groups[g]->weapons[i];
      if (w && w->def && w->def->type && w->def->type->heat > 0.0f) return wm->select_group(g) ? g : -1;
    }
  return -1;
}

// Mech3.exe 1.2 @ 0x0041aba0, the player tick's tail on the pilot's mech:
// its target's chosen part (0x0041d6f0: the HUD's, not ported, nothing
// without a target), the ammo gauge (0x004b0d80) and the last attacker
// cleared.
void mech_after_tick(void* mech, uint32_t (*unported)(const char*)) {
  Mech* m = static_cast<Mech*>(mech);
  if (m->target && m->xf8 != -1 && unported) unported("0x0041d6f0");
  if (auto* wm = static_cast<WeaponManager*>(m->ammo)) wm->ammo_gauge();
  m->attacker = nullptr;
}

void unwire_weapons() {
  weapon_calls().vehicle_entity = nullptr;
  zwep::run_hooks() = zwep::RunHooks{};
  zwep::hit_hooks() = zwep::HitHooks{};
  update_calls().weapons_fire = nullptr;
  update_calls().weapons_update = nullptr;
  heat_calls().heat_option = nullptr;
  damage_calls().node_part = nullptr;
  damage_calls().node_owner = nullptr;
}

// Mech3.exe 1.2 @ 0x005833b0(mech, node, 1, 0x004161a0): the mech's node
// takes hits into Mech::TakeDamage.
void mech_set_damage_handler(void* mech) {
  Mech* m = static_cast<Mech*>(mech);
  zwep::set_damage_handler(m, m->node, mech_damage, 1);
}

}  // namespace mech3
