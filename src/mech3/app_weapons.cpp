// The weapons' hooks wired to the program (docs/weapons.md, In the program):
// the weapon manager's calls into the mech (mech3/weapon), zWeapon's into
// the scene graph, object3d and cls_di.c's queries (gamez/zweapon), and the
// mechs' damage handler. Not original code. Visuals (lights, animations,
// craters, marks) and sounds stay unset: the shots fly and hit without them.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "gamez/zdeclient/zdec_crater.h"
#include "gamez/zdeclient/zdec_init.h"
#include "gamez/zclass/cls_di.h"
#include "gamez/zeffect/zeff_anim_init.h"
#include "gamez/zeffect/zeff_anim_run.h"
#include "gamez/zeffect/zeff_init.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmath/zmth_vector.h"
#include "gamez/zsound/zsnd.h"
#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/cls_util.h"
#include "gamez/zclass/cls_world.h"
#include "gamez/zclass/light.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zmodel/gmod_matl.h"
#include "gamez/zmodel/gmod_hit.h"
#include "gamez/zweapon/zweapon.h"
#include "gamez/zweapon/zwep_init.h"
#include "mech3/app_view.h"
#include "mech3/cockpit_sound.h"
#include "mech3/mech.h"
#include "mech3/mech_move.h"
#include "mech3/mission_world.h"
#include "mech3/mech_skis.h"
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
uint32_t (*g_unported)(const char*) = nullptr;
void unported(const char* key) {
  if (g_unported) g_unported(key);
}
zeff::AnimDef* anim_of(zwep::Handle a) { return static_cast<zeff::AnimDef*>(a); }

// What the weapons start, for the tests (ours): "anim <name>", "sound <name>".
std::vector<std::string> g_fx;
void note(const char* kind, const char* name) {
  if (g_fx.size() < 4096) g_fx.push_back(std::string(kind) + " " + (name ? name : "?"));
}
void note_anim(zwep::Handle a) {
  if (a) note("anim", anim_of(a)->name());
}
void note_sound(zwep::Handle s) {
  const auto* o = static_cast<const zsnd::Object*>(s);
  if (!o) return;
  note("sound", o->kind == 0 ? static_cast<const zsnd::Sound*>(o)->name : static_cast<const zsnd::Group*>(o)->name);
}

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
int32_t g_hits = 0;                  // the handler's calls, for the program's tests
zwep::Handle g_last_attacker = nullptr;  // zWeapon's shooter node at the last ([0x009a0edc])

uint32_t mech_damage(zwep::Handle object, zwep::WeaponDef* def, zwep::HitRecord* hit, float damage) {
  ++g_hits;
  g_last_attacker = zwep::attacker();
  Collision c;
  c.x20 = hit->material;
  c.node = hit->node;
  return static_cast<uint32_t>(static_cast<Mech*>(object)->take_damage(def, &c, damage));
}


uint32_t g_zone;  // the spawn's zone word: the entity's +0x64 byte
}  // namespace

// Mech3.exe 1.2 @ 0x0040e9d0: a vehicle's node (+0x28 id 0xf000) its owner,
// a part's (0xff) the part's mech (0x00460000).
void* node_owner(void* n) {
  zcls::Node* z = zn(n);
  if (!z) return nullptr;
  const uint32_t id = z->get<uint32_t>(0x28);
  if (id & 0xf000) return z->owner;
  if (id & 0xff) return static_cast<MechPart*>(z->owner)->mech;
  return nullptr;
}

// Mech3.exe 1.2 @ 0x0040e920 (0x0040eee0's target): the game object a node
// belongs to, by its +0x28 bits: 16, 18, then a vehicle's or a part's
// (0x0040e9d0), 19, 17, 20, 21, 22 have their owner at +0x40.
void* node_entity(void* n) {
  zcls::Node* z = zn(n);
  if (!z) return nullptr;
  const uint32_t id = z->get<uint32_t>(0x28);
  auto bit = [&](int b) { return (id >> b & 1) != 0; };
  if (bit(16) || bit(18)) return z->owner;  // 0x0040e960 tests 18 first of its own
  if (void* o = node_owner(n)) return o;
  for (int b : {19, 17, 20, 21, 22})
    if (bit(b)) return z->owner;
  return nullptr;
}

std::vector<std::string>& weapon_fx_log() { return g_fx; }

void wire_weapons(uint32_t (*log)(const char* key)) {
  g_unported = log;
  g_fx.clear();
  WeaponCalls& c = weapon_calls();
  // The weapons' own sounds (U_LOOPED, U_RECHARGE_SND ..) through zSound; the
  // cockpit's (0x0041e770's table) and Betty's voice are not ported.
  c.play_loop = [](Handle s) -> Handle {                                                   // 0x0056e3d0 (1.0)
    note_sound(s);
    return s ? zsnd::play(static_cast<zsnd::Object*>(s), 1.0f) : nullptr;
  };
  c.play_at = [](Handle s, const Vec3f* p) -> Handle {                                     // 0x0056e7e0 (1.0, 0)
    note_sound(s);
    return s ? zsnd::play_at(static_cast<zsnd::Object*>(s), &p->x, 1.0f, nullptr) : nullptr;
  };
  c.stop_sound = [](Handle v) { if (v) zsnd::stop(static_cast<zsnd::VoiceBase*>(v)); };  // 0x0056e9f0
  c.mech_sound = [](int32_t i) { cockpit_sound_play(i, 1.0f); };  // 0x0041e770
  c.destroyed_sounds = [](Handle, Handle) { unported("0x004e6dc0"); };
  c.voice_destroyed = [](int32_t) { unported("0x004853d0"); };
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
  // TAG's strike on the mech its beam marked (FIX (bug), weapons.md "TAG"):
  // the hit node's damage handler is a mech's: its live position and velocity.
  c.strike_target = [](Handle node, zwep::LockRef* out) {
    zwep::DamageFn fn = nullptr;
    Handle obj = zwep::damage_object(node, &fn);
    if (!obj || fn != mech_damage || !skis_calls().position) return false;
    Mech* m = mech_of(obj);
    out->entity = obj;
    out->at = reinterpret_cast<const zwep::Vec3f3*>(skis_calls().position(*m));
    out->velocity = reinterpret_cast<const zwep::Vec3f3*>(&m->velocity);
    return out->at != nullptr;
  };
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
  // The surface marks' impact type ([0x0074a2b0]), read only by 0x00542060.
  // Craters and quicksand are zDEClient's.
  r.impact_type = zmod::mark_set_type;  // 0x005426a0
  r.crater_defaults = zdec::crater_defaults;        // 0x004ff920
  r.crater_make = zdec::crater_make;                // 0x004ffaa0
  r.quicksand_defaults = zdec::quicksand_defaults;  // 0x005020f0
  r.quicksand_make = [](const zwep::QuicksandOrder* q) { zdec::quicksand_make(q); };  // 0x00502110
  r.pool_live = []() { return !zcls::nodes().blocks.empty(); };                            // 0x004f8f50: [0x0060bf48]
  // The animations (zeffect): started at a node, following one, on a model.
  r.anim_attached = [](zwep::Handle a, zwep::Handle n, const void* off) -> zwep::Handle {  // 0x00508da0
    note_anim(a);
    return a ? zeff::anim_start_connector(anim_of(a), nullptr, zn(n), static_cast<const float*>(off), nullptr)
             : nullptr;
  };
  r.anim_follow = [](zwep::Handle a, zwep::Handle n) -> zwep::Handle {                    // 0x00508ee0
    note_anim(a);
    return a ? zeff::anim_start_connector_at(anim_of(a), nullptr, zn(n), nullptr, zn(n), nullptr) : nullptr;
  };
  r.anim_model = [](zwep::Handle a, zwep::Handle m) -> zwep::Handle {                     // 0x00508c10
    note_anim(a);
    return a ? zeff::anim_start(anim_of(a), zn(m), 0.0f, 0.0f, 0.0f) : nullptr;
  };
  r.anim_on_done = [](zwep::Handle a, zwep::Projectile* p) {                              // 0x00508f00 with 0x00583120
    if (a)
      zeff::anim_set_callback(
          anim_of(a), [](zeff::AnimDef*, void* arg, int32_t) { zwep::model_anim_done(static_cast<zwep::Projectile*>(arg)); },
          p);
  };
  r.anim_stop = [](zwep::Handle a) { zeff::anim_restart(anim_of(a)); };                   // 0x00508460: to stopping
  r.anim_free = [](zwep::Handle a) { zeff::anim_stop(anim_of(a)); };                      // 0x00506ca0
  r.effect_handle = [](zwep::Handle a, const float v[9]) -> zwep::Handle {                // 0x00508aa0
    note_anim(a);
    return a ? zeff::anim_start_at(anim_of(a), nullptr, v, v + 3, v + 6) : nullptr;
  };
  // zSound.
  r.sound_at = [](zwep::Handle s, const zwep::Vec3f3* at, float volume) -> zwep::Handle {  // 0x0056e7e0
    note_sound(s);
    return s ? zsnd::play_at(static_cast<zsnd::Object*>(s), &at->x, volume, nullptr) : nullptr;
  };
  r.sound_stop = [](zwep::Handle v) { if (v) zsnd::stop(static_cast<zsnd::VoiceBase*>(v)); };  // 0x0056e9f0
  r.sound_frequency = [](zwep::Handle v, float t) {                                         // 0x0056f280
    if (v) zsnd::set_frequency(static_cast<zsnd::VoiceBase*>(v), t);
  };
  // 0x004eda10 on the world: the ground below a guided shot (one record kept).
  r.ground = [](const zwep::Vec3f3* at, zwep::HitList* out) {
    static zcls::GroundHits g;
    out->count = 0;
    if (!qworld()) return;
    zcls::ground_below(qworld(), &at->x, &g);
    out->count = g.count;
    if (g.count) std::memcpy(&out->rec[0], &g.hit[0], sizeof g.hit[0]);
  };
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
  // The impact's sound, animation turned to the surface, craters; zEffect's
  // table (empty in both games' effects.zrd); the surface marks not ported.
  h.sound = zwep::sound;                                                                    // 0x00582f60
  h.effect = [](zwep::Handle a, const float v[9]) {                                         // 0x00508aa0
    note_anim(a);
    if (a) zeff::anim_start_at(anim_of(a), nullptr, v, v + 3, v + 6);
  };
  h.surface_angles = [](const zwep::Vec3f3* n, float out[3]) {
    const float up[3] = {0.0f, 1.0f, 0.0f};  // 0x005eda70
    float q[4];
    zmth::quat_between(up, &n->x, q);  // 0x00525a50
    zmth::quat_angles(q, out);         // 0x00525f80
  };
  h.crater = [](zwep::WeaponDef* d, zwep::HitRecord* hit, int32_t, zwep::Projectile* p) {  // 0x005825b0
    return zwep::crater(d, hit, p);
  };
  h.quicksand = [](zwep::WeaponDef* d, zwep::HitRecord* hit, int32_t soil, zwep::Projectile* p) {  // 0x005824b0
    return zwep::quicksand(d, hit, soil, p), 0;
  };
  h.impact_effect = [](int32_t e, const zwep::Vec3f3*) { zeff::effect_spawn(e); };        // 0x0050d650
  // 0x00542060: nothing unless a script gave a mark image (none ships); the
  // painting into the hit texture is not ported (weapon-effects.md).
  h.surface_mark = [](zwep::HitRecord*) {
    if (zmod::mark_on()) unported("0x00542060");
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
  // The aim's view test (0x0041bc20): [0x005f4c8c] +0x64 is the mech and its
  // camera (+0x58) +4 is 1, the cockpit.
  aim_calls().cockpit_view = [](Mech& m) {
    return program_view_target() == static_cast<void*>(&m) && program_view_mode_id() == 1;
  };

  // Mech3.exe 1.2 @ 0x0040ea10 / 0x0040e9d0 (through 0x0040ef30 / 0x0040ef10):
  // what a node hit belongs to, by its +0x28 id and +0x40 owner (the build
  // marks each part's subtree, mech_part_build.cpp).
  DamageCalls& d = damage_calls();
  d.node_part = [](Handle n) -> MechPart* {
    zcls::Node* z = zn(n);
    return z && (z->get<uint32_t>(0x28) & 0xff) ? static_cast<MechPart*>(z->owner) : nullptr;
  };
  d.node_owner = node_owner;
  d.attacker = [] { return node_entity(zwep::hit_state().attacker); };  // 0x0040eee0 on [0x009a0edc]

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
void sync_frame_step(float dt, float clock) {
  mech3::world().dt = dt;
  weapon_world().dt = dt;
  weapon_world().clock = clock;  // [0x0072c530], the timer's scaled total: the AMS's and the turrets' clock
  zwep::hit_state().dt = dt;
}

int32_t mech_hits_taken(void** attacker_node) {
  if (attacker_node) *attacker_node = g_last_attacker;
  return g_hits;
}

void* mech_node(void* mech) { return mech ? static_cast<Mech*>(mech)->node : nullptr; }

bool mech_group_reloading(void* mech, int32_t g) {
  auto* wm = static_cast<WeaponManager*>(static_cast<Mech*>(mech)->ammo);
  if (!wm || g < 0 || g > 4 || !wm->groups[g]) return false;
  for (uint32_t i = 0; i < wm->groups[g]->count; ++i)
    if (const Weapon* w = wm->groups[g]->weapons[i]; w && (w->reload > 0.0f || w->is_firing())) return true;
  return false;
}

// What F1..F5 select (CMD_WEPGRP_1..5 through the mech's command handler,
// not ported): the first group holding a weapon that heats its mech; -1 none.
std::string mech_group_names(void* mech, int32_t g) {
  auto* wm = static_cast<WeaponManager*>(static_cast<Mech*>(mech)->ammo);
  std::string s;
  for (uint32_t i = 0; wm && g >= 0 && g < 5 && wm->groups[g] && i < wm->groups[g]->count; ++i)
    if (const Weapon* w = wm->groups[g]->weapons[i]) s += std::string(s.empty() ? "" : " ") + (w->def ? w->def->name : "?");
  return s;
}
bool mech_select_group(void* mech, int32_t g) {
  auto* wm = static_cast<WeaponManager*>(static_cast<Mech*>(mech)->ammo);
  return wm && wm->select_group(g);
}
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
// its target's chosen part's point (0x0041d6f0), the ammo gauge (0x004b0d80)
// and the last attacker cleared.
void mech_after_tick(void* mech, uint32_t (*)(const char*)) {
  Mech* m = static_cast<Mech*>(mech);
  m->aim_part();
  if (auto* wm = static_cast<WeaponManager*>(m->ammo)) wm->ammo_gauge();
  m->attacker = nullptr;
}

// Everything wire_weapons set, back to the defaults.
void unwire_weapons() {
  weapon_calls() = WeaponCalls{};
  aim_calls() = AimCalls{};
  zwep::run_hooks() = zwep::RunHooks{};
  zwep::hit_hooks() = zwep::HitHooks{};
  update_calls().weapons_fire = nullptr;
  update_calls().weapons_update = nullptr;
  heat_calls().heat_option = nullptr;
  damage_calls().node_part = nullptr;
  damage_calls().node_owner = nullptr;
  damage_calls().attacker = nullptr;
}

// Mech3.exe 1.2 @ 0x005833b0(mech, node, 1, 0x004161a0): the mech's node
// takes hits into Mech::TakeDamage.
void mech_set_damage_handler(void* mech) {
  Mech* m = static_cast<Mech*>(mech);
  zwep::set_damage_handler(m, m->node, mech_damage, 1);
}

}  // namespace mech3
