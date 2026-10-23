#include "mech3/veh_basic_object.h"

#include "mech3/mech_build.h"
#include "mech3/weapon.h"

namespace mech3 {

namespace {
VehBasicHooks g_default_hooks;
VehBasicHooks* g_hooks = &g_default_hooks;
VehicleMarkers g_markers;
}  // namespace

void set_veh_basic_hooks(VehBasicHooks* h) { g_hooks = h ? h : &g_default_hooks; }

void VehBasicHooks::entity_set_node(Vehicle& v, Handle node) {
  unported("0x004d6c90", {ref(&v), low(node), 0, 0, 0});
  vehicle_set_node(v, node);
}

void VehBasicHooks::weapons_free(void* w) { delete static_cast<WeaponManager*>(w); }

// Mech3.exe 1.2 @ 0x004ac320 (ECX the controls, EDX the type).
BasicVehicle* vehicle_create(Handle ctl, const char* type, const char* name) {
  VehBasicHooks& h = *g_hooks;
  Handle node = h.node_named(name);
  if (!node) {
    const Handle model = h.node_named(type);
    if (!model) return nullptr;
    node = h.node_clone(model);
    if (!node) return nullptr;
    h.node_set_name(node, name);
    h.world_add(node);
  }
  auto* v = new BasicVehicle;
  h.on_new(0xfc, v);
  vehicle_construct(*v, ctl, node);
  return v;
}

// Mech3.exe 1.2 @ 0x004ac530 (ctl, node).
void vehicle_construct(BasicVehicle& v, Handle ctl, Handle node) {
  VehBasicHooks& h = *g_hooks;
  vehicle_base(v);
  v.controls = ctl;
  v.engine = EngineSound{};  // 0x004adaf0
  v.motion = MotionAnim{};  // 0x004aec00
  v.node = nullptr;
  if (!v.controls) {
    const Handle mem = h.controls_alloc(0xa70);
    v.controls = mem ? h.controls_make(mem, v) : nullptr;
  } else {
    h.controls_set_owner(v.controls, v);
  }
  v.xe0 = 0;
  h.cameras_once();
  if (node) {
    h.entity_set_node(v, node);
    h.entity_register(v);
  }
}

namespace {

// Mech3.exe 1.2 @ 0x004aeb50, the Vehicle base, with AiTarget's 0x004d58a0
// and ZEntity3d's 0x005a7f90.
void vehicle_base_destroy(Vehicle& v) {
  VehBasicHooks& h = *g_hooks;
  if (v.node) {
    h.entity_set_node(v, nullptr);
    h.entity_unregister(v);
  }
  if (v.ammo) {
    h.weapons_free(v.ammo);
    v.ammo = nullptr;
  }
  if (v.net_collision) {
    h.net_collision_free(v.net_collision);
    v.net_collision = nullptr;
  }
  v.clear_contacts();  // 0x004d58a0: 0x004a7be0
  if (v.node) {
    h.entity_set_node(v, nullptr);
    h.entity_unregister(v);
  }
  v.net = 0;
  v.release_shadow();
  if (v.node) h.zentity_set_node(v, nullptr);  // ZEntity3d's 0x005a7f90
  h.entity_unregister(v);                      // there whether listed or not
  h.zentity_set_node(v, nullptr);              // ZEntity's 0x005a8370
}

}  // namespace

// Mech3.exe 1.2 @ 0x004ac650.
void vehicle_destroy(BasicVehicle& v) {
  VehBasicHooks& h = *g_hooks;
  if (v.engine.entry) v.engine.entry = nullptr;  // 0x004adb40
  if (v.controls) h.controls_delete(v.controls);
  const Handle node = v.node;
  v.controls = nullptr;
  if (node) {
    h.node_detach(node);
    h.world_remove(node);
    h.node_free(node);
    h.entity_set_node(v, nullptr);
    h.entity_unregister(v);
  }
  if (v.motion.anim) {  // 0x004addb0 -> 0x004ade70
    h.anim_stop(v.motion.anim);
    v.motion.anim = nullptr;
  }
  vehicle_base_destroy(v);
}

// Mech3.exe 1.2 @ 0x004ac630(1).
void vehicle_delete(BasicVehicle* v) {
  if (!v) return;
  vehicle_destroy(*v);
  delete v;
}

VehicleMarkers& vehicle_markers() { return g_markers; }

// Mech3.exe 1.2 @ 0x004ac3d0.
void vehicle_markers_create() {
  VehBasicHooks& h = *g_hooks;
  g_markers.ally = h.marker_create("hud_veh_ally_sm");
  g_markers.enemy = h.marker_create("hud_veh_enemy_sm");
  g_markers.other = h.marker_create("hud_veh_other_sm");
}

}  // namespace mech3
