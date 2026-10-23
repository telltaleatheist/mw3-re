#include "mech3/app_vehicle.h"

#include <algorithm>
#include <cstring>
#include <set>

#include "gamez/zclass/class.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/list.h"
#include "gamez/zclass/cls_util.h"
#include "gamez/zclass/object3d.h"
#include "gamez/zeffect/zeff_anim_run.h"
#include "gamez/ztwod/twod.h"
#include "gamez/zweapon/zweapon.h"
#include "mech3/app_mech.h"
#include "mech3/mech_build.h"
#include "mech3/mission_world.h"
#include "mech3/sensors.h"
#include "mech3/veh_basic_object.h"
#include "mech3/veh_controls.h"
#include "mech3/veh_move.h"
#include "mech3/weapon.h"
#include "mech3/zai_brain.h"

// The vehicles in the program (docs/vehicles.md, In the program): the
// basic vehicle's and its controls' calls out wired to the ports, and what
// the mission start (player.cpp) asks of a vehicle. Ours, not the exe's.
namespace mech3 {

void* node_entity(void* node);  // app_weapons.cpp's 0x0040e920

namespace {
MechWiring g_w;
std::vector<Vehicle*> g_order;        // the same, as made
std::set<const Vehicle*> g_vehicles;  // the program's, this mission's
zcls::Node* zn(Handle h) { return static_cast<zcls::Node*>(h); }

struct WiredVehicles : VehBasicHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return g_w.unported ? g_w.unported(g_w.engine, key, args) : 0;
  }
  Handle node_named(const char* name) override { return zcls::list_find_by_name(6, name); }  // 0x004f7fe0
  // Each vehicle its own tree, as the mechs' (models and materials copied).
  Handle node_clone(Handle node) override { return zcls::tree_copy(zn(node), 1, 1); }
  void node_set_name(Handle node, const char* name) override { zcls::node_set_name(zn(node), name); }
  void world_add(Handle node) override {
    if (zcls::Node* w = world_hooks().world) zcls::node_add_child(w, zn(node));
  }
  void world_remove(Handle node) override {
    if (zcls::Node* w = world_hooks().world) zcls::node_remove_child(w, zn(node));
  }
  void node_free(Handle node) override { zcls::tree_destroy(zn(node)); }
  void node_detach(Handle node) override { zwep::remove_handlers(node); }  // 0x00583400
  void controls_set_owner(Handle c, BasicVehicle& v) override { static_cast<VehicleControls*>(c)->owner = &v; }
  void controls_delete(Handle c) override { delete static_cast<VehicleControls*>(c); }
  void entity_set_node(Vehicle& v, Handle node) override { vehicle_set_node(v, node); }  // 0x004d6c90
  void zentity_set_node(Vehicle& v, Handle node) override { v.node = node; }            // 0x005a8390 with none
  // 0x005a7f50 / 0x005a7f70: zEntity's list, the port's (mech3/sensors).
  void entity_register(Vehicle& v) override {
    ai::entity_list().push_back(g_w.handle_of(g_w.engine, &v));
    g_vehicles.insert(&v);
    g_order.push_back(&v);
  }
  void entity_unregister(Vehicle& v) override {
    auto& l = ai::entity_list();
    l.erase(std::remove(l.begin(), l.end(), g_w.handle_of(g_w.engine, &v)), l.end());
    g_vehicles.erase(&v);
    g_order.erase(std::remove(g_order.begin(), g_order.end(), &v), g_order.end());
  }
  void anim_stop(Handle a) override { if (a) zeff::anim_stop(static_cast<zeff::AnimDef*>(a)); }  // 0x00506ca0
  void shadow_release(Vehicle&) override {}  // the AiTarget's (no shadow is made for a vehicle here)
  // 0x005a85c0: a node in the scene takes the matrix (vt+0x1c, 0x005a85a0).
  void refresh(Vehicle& v) override {
    zcls::Node* n = zn(v.node);
    if (n && n->parent_count() != 0) v.entity_moved();
  }
  void set_position(BasicVehicle& v, const float pos[3]) override {  // 0x005a8550
    zcls::object3d_set_translation(zn(v.node), pos[0], pos[1], pos[2]);
    refresh(v);
  }
  Handle marker_create(const char* name) override { return new ztwod::Image(name, 0, 0, 1); }  // 0x0059b830
  // zWeapon's hit state and a kill (as the mechs').
  Handle hit_attacker() override { return zwep::attacker(); }
  void hit_health(float f) override { zwep::set_health_after(f); }
  void hit_killed() override { zwep::note_kill(reinterpret_cast<zwep::Handle>(1), nullptr); }
  Handle node_object(Handle node) override { return node_entity(node); }
  // obj.vt+0x80: a mech's kill credit (0x0041b420); a vehicle's 0x004a5bc0
  // does nothing.
  void credit_kill(Handle object, BasicVehicle& v) override {
    if (entity_is_mech(object)) return static_cast<Mech*>(object)->on_kill(&v);
    if (is_program_vehicle(object)) return;
    VehBasicHooks::credit_kill(object, v);
  }
  Handle anim_run(Handle def, Handle node) override {
    return def ? zeff::anim_start(static_cast<zeff::AnimDef*>(def), zn(node), 0, 0, 0) : nullptr;
  }
  void anim_start(Handle def, Handle node) override { anim_run(def, node); }
  void weapons_settle(void* w) override { static_cast<WeaponManager*>(w)->destroy_all(); }  // 0x004b1110
  const float* node_matrix(Handle node) override { return zcls::object3d_matrix(zn(node)); }
  float game_time() override { return mech_game_time(); }
  // The controls' vt+0x0c (the AI's and the base's 0x004acbc0).
  void controls_setup(Handle c, zrdr::Node* def, const char*, int32_t) override {
    static_cast<VehicleControls*>(c)->setup(def);
  }
};
WiredVehicles g_hooks;

// The controls' (veh_controls): the owner's refresh.
struct WiredControls : VehControlsHooks {
  uint32_t unported(const char* key, std::initializer_list<zgame::Arg> args) override {
    return g_w.unported ? g_w.unported(g_w.engine, key, args) : 0;
  }
  void owner_refresh(void* owner) override { g_hooks.refresh(*static_cast<Vehicle*>(owner)); }
};
WiredControls g_controls;

// zWeapon's hit handler on a vehicle's node (0x004af550 through 0x005833b0).
uint32_t vehicle_damage_handler(zwep::Handle object, zwep::WeaponDef* def, zwep::HitRecord* hit, float damage) {
  auto* v = static_cast<BasicVehicle*>(object);
  return static_cast<uint32_t>(vehicle_hit(*v, def, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hit)), damage));
}
}  // namespace

void wire_vehicles(const MechWiring* w) {
  if (!w) {
    g_w = MechWiring{};
    set_veh_basic_hooks(nullptr);
    set_veh_controls_hooks(nullptr);
    return;
  }
  g_w = *w;
  set_veh_basic_hooks(&g_hooks);
  set_veh_controls_hooks(&g_controls);
}

bool is_program_vehicle(const void* p) { return p && g_vehicles.count(static_cast<const Vehicle*>(p)) != 0; }
void* program_vehicle_create(void* ctl, const char* type, const char* name) { return vehicle_create(ctl, type, name); }
void* program_vehicle_controls(void* v) { return static_cast<BasicVehicle*>(v)->controls; }
void* program_vehicle_node(void* v) { return static_cast<BasicVehicle*>(v)->node; }
void* program_vehicle_centroid(void* v) { return static_cast<BasicVehicle*>(v)->centroid; }
void program_vehicle_place(void* v, const float pos[3], float yaw) { vehicle_place(*static_cast<BasicVehicle*>(v), pos, yaw); }
void program_vehicle_setup(void* v, void* def, const char* name, int32_t kind) {
  vehicle_setup(*static_cast<BasicVehicle*>(v), static_cast<zrdr::Node*>(def), name, kind);
}
void program_vehicle_motion(void* v, void* mode, const char* name, const char* type) {
  auto* b = static_cast<BasicVehicle*>(v);
  if (!b->controls) return;
  vehicle_motion_read(*b->controls, b->node, static_cast<zrdr::Node*>(mode), name, type, &b->controls->point_count);
}
void program_vehicle_points(void* v) { vehicle_points_place(*static_cast<BasicVehicle*>(v)); }
void program_vehicle_touch(void* v) {
  auto* b = static_cast<BasicVehicle*>(v);
  zwep::set_damage_handler(b, b->node, vehicle_damage_handler, 1);
}
void program_vehicle_refresh(void* v) { g_hooks.refresh(*static_cast<BasicVehicle*>(v)); }
bool program_vehicle_mask(void* v, uint32_t off, uint32_t* get, const uint32_t* set) {
  auto* b = static_cast<Vehicle*>(v);
  uint32_t* f = off == 0xc0 ? &b->team : off == 0xc4 ? &b->allies : off == 0xc8 ? &b->enemies : nullptr;
  if (!f) return false;
  if (set) *f = *set;
  if (get) *get = *f;
  return true;
}
namespace {
BasicVehicle& bv(void* v) { return *static_cast<BasicVehicle*>(v); }
}  // namespace
float program_vehicle_health(void* v) { return vehicle_health_share(bv(v)); }
float program_vehicle_speed_at(void* v, float share) { return vehicle_speed_at(bv(v), share); }
float program_vehicle_stopping_distance(void* v, float s) { return vehicle_stopping_distance(bv(v), s); }
float program_vehicle_stop_share(void* v, float d) { return vehicle_stop_share(bv(v), d); }
float program_vehicle_move_speed(void* v) { return vehicle_move_speed(bv(v)); }
bool program_vehicle_can_face(void* v, const float d[3]) { return vehicle_gun_within_world(bv(v), d); }
bool program_vehicle_in_reach(void* v, float cosine) { return vehicle_can_aim(bv(v), cosine); }
bool program_vehicle_aim_clamp(void* v, const float d[3], float out[3]) { return vehicle_gun_clamp(bv(v), d, out); }
void program_vehicle_set_aim(void* v, const float d[3]) { vehicle_aim_at(bv(v), d, 0); }
void program_vehicle_velocity(void* v, float out[3]) {
  const Vec3& u = bv(v).velocity;
  out[0] = u.x, out[1] = u.y, out[2] = u.z;
}
const char* program_vehicle_name(void* v) {
  const auto* n = static_cast<const zcls::Node*>(bv(v).node);
  return n ? n->name() : nullptr;
}
void* program_vehicle_target(void* v) { return static_cast<ai::AiTarget*>(&bv(v)); }
void program_vehicle_hit_an_ally(void* v, void* victim) {
  if (auto* c = dynamic_cast<ai::AiVehicleControls*>(bv(v).controls)) c->hit_an_ally(victim);
}
void program_vehicle_frame(void* v, float dt) {
  if (bv(v).x09) vehicle_frame(bv(v), dt);
}
namespace {
ai::Brain* brain_of(void* v) {
  auto* c = dynamic_cast<ai::AiVehicleControls*>(bv(v).controls);
  return c ? c->brain.get() : nullptr;
}
}  // namespace
bool program_vehicle_awake(void* v) {
  if (!bv(v).active) return false;
  const ai::Brain* b = brain_of(v);
  return !b || b->p6_clear();
}
bool program_vehicle_sees(void* v, uint32_t self) {
  ai::Brain* b = brain_of(v);
  return !b || b->sees_target(self);
}
void program_vehicle_set_sensed(void* v, bool on) { bv(v).x09 = on; }
void program_vehicle_sense_refresh(void* v) { bv(v).engine.release(); }
void program_vehicle_engine_play(void* v, const float pos[3]) { bv(v).engine.play(pos, nullptr, 0); }
void* program_vehicle_named(const char* name) {
  for (Vehicle* v : g_order) {
    const auto* n = static_cast<const zcls::Node*>(v->node);
    if (name && n && n->name() && std::strcmp(n->name(), name) == 0) return v;
  }
  return nullptr;
}
void program_vehicle_destroy(void* v) { vehicle_die(bv(v)); }
void* program_vehicle_brain(void* controls) {
  for (Vehicle* v : g_order)
    if (static_cast<BasicVehicle*>(v)->controls == controls)
      if (auto* c = dynamic_cast<ai::AiVehicleControls*>(static_cast<BasicVehicle*>(v)->controls)) return c->brain.get();
  return nullptr;
}
bool program_vehicle_kill(void* v, void* killer, const char* weapon) {
  zwep::WeaponDef* def = zwep::find_def(weapon);
  if (!is_program_vehicle(v) || !def || !bv(v).controls) return false;
  zwep::hit_state().attacker = killer ? static_cast<Vehicle*>(killer)->node : nullptr;
  vehicle_damage(bv(v), def, 0, bv(v).controls->health + 1.0f);
  zwep::hit_state().attacker = nullptr;
  return !bv(v).active;
}
bool program_vehicle_free(void* v) {
  if (!is_program_vehicle(v)) return false;
  vehicle_delete(static_cast<BasicVehicle*>(v));  // vt+0x00 0x004ac630(1)
  g_vehicles.erase(static_cast<Vehicle*>(v));     // its zEntity entry goes in the destructor
  g_order.erase(std::remove(g_order.begin(), g_order.end(), static_cast<Vehicle*>(v)), g_order.end());
  return true;
}
void program_vehicles_forget() { g_vehicles.clear(), g_order.clear(); }
std::vector<void*> program_vehicles() { return {g_order.begin(), g_order.end()}; }
bool describe_vehicle(const void* p, VehicleSummary* out) {
  if (!is_program_vehicle(p)) return false;
  const auto* v = static_cast<const BasicVehicle*>(p);
  const auto* n = static_cast<const zcls::Node*>(v->node);
  *out = VehicleSummary{};
  out->name = n ? n->name() : nullptr;
  out->in_world = n && n->parent_count() > 0;
  out->active = v->active;
  out->team = v->team;
  if (v->transform) out->pos[0] = v->transform[9], out->pos[1] = v->transform[10], out->pos[2] = v->transform[11];
  if (const VehicleControls* c = v->controls) {
    out->ai_controls = dynamic_cast<const ai::AiVehicleControls*>(c) != nullptr;
    out->owned = c->owner == v;
    out->health = c->health, out->health_max = c->health_max;
    out->mode = c->motion.mode, out->supports = c->motion.support_count, out->points = c->point_count;
  }
  return true;
}

}  // namespace mech3
