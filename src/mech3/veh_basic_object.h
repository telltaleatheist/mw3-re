#pragma once
#include <cstdint>

#include "gamez/zgame/unported.h"
#include "mech3/mech.h"

// The basic vehicle (trucks, APCs, tanks; the field base and the stealth
// object are made over it), docs/vehicles.md. Original file: these sit just
// before mech3\veh_basic.cpp (0x004ad930 is its first named function), with
// ground_pick (mech3/veh_basic.h); kept apart from it because mech.h's Handle
// clashes with player.h's.
namespace mech3 {

// The engine sound at +0xe8 (vtable 0x005c8b90): the definition's
// sounds/engine entry, from the vehicles' shared cache at 0x0060b110
// (0x004adb20 -> 0x004ad930, step 1b), else null.
struct EngineSound {
  Handle entry = nullptr;  // +0x04
};

// The motion animation at +0xf0 (0x004aec00 zeroes it, a body the linker
// shares with IControls' hit record): the definition's motion_anim
// (0x004ade40, step 1b), stopped (0x00506ca0) as it goes (0x004addb0 ->
// 0x004ade70).
struct MotionAnim {
  uint32_t x00 = 0;        // +0x00
  Handle anim = nullptr;   // +0x04
  uint32_t x08 = 0;        // +0x08
};

// 0xfc bytes, vtable 0x005c88e8 (89 slots).
struct BasicVehicle : Vehicle {
  uint32_t xe0 = 0;              // +0xe0
  Handle controls = nullptr;     // +0xe4  given (the AI's), else its own made here (0x004ae270)
  EngineSound engine;            // +0xe8  (0x004adaf0)
  MotionAnim motion;             // +0xf0
};

// The calls out, by the original's address.
class VehBasicHooks : public zgame::Unported {
 public:
  static uint32_t low(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
  static Handle h(uint32_t v) { return reinterpret_cast<Handle>(static_cast<uintptr_t>(v)); }
  virtual uint32_t ref(const void* p) { return low(p); }
  // The vehicle's own block (operator new 0xfc; silent here).
  virtual void on_new(uint32_t size, const void* p) { (void)size, (void)p; }

  virtual Handle node_named(const char* name) { return h(unported("0x004f7fe0", {6, name})); }
  virtual Handle node_clone(Handle node) { return h(unported("0x004f9af0", {low(node), 0, 0})); }
  virtual void node_set_name(Handle node, const char* name) { unported("0x004eb300", {low(node), name}); }
  virtual void world_add(Handle node) { unported("world.vt+0x0", {low(node)}); }  // [0x00608c84]
  virtual void world_remove(Handle node) { unported("0x004ebb40", {low(node)}); }  // from [0x00608c84]+4
  virtual void node_free(Handle node) { unported("0x004f9050", {low(node)}); }
  virtual void node_detach(Handle node) { unported("0x00583400", {low(node)}); }
  // The vehicle's own controls (0x004ac860(0xa70) zeroed, then 0x004ae270(this, 0, 1); item 2a).
  virtual Handle controls_alloc(uint32_t size) { return h(unported("0x004ac860", {size})); }
  virtual Handle controls_make(Handle mem, BasicVehicle& v) {
    return h(unported("0x004ae270", {low(mem), ref(&v), 0, 1}));
  }
  virtual void controls_set_owner(Handle c, BasicVehicle& v) { unported("ctl.ictl.vt+0x8", {low(c), ref(&v)}); }
  virtual void controls_delete(Handle c) { unported("ctl.ictl.vt+0x0", {low(c), 1}); }
  virtual void cameras_once() { unported("0x004ace00", {}); }  // the vehicles' camera modes
  // 0x004d6c90(node, 0, 0, 0): zEntity's node and its markers (vehicle_set_node).
  virtual void entity_set_node(Vehicle& v, Handle node);
  // ZEntity's own 0x005a8390(node, 0, 0, 0) (the destructors' with null).
  virtual void zentity_set_node(Vehicle& v, Handle node) {
    unported("0x005a8390", {ref(&v), low(node), 0, 0, 0});
    v.node = node;
  }
  virtual void entity_register(Vehicle& v) { unported("0x005a7f50", {ref(&v)}); }
  virtual void entity_unregister(Vehicle& v) { unported("0x005a7f70", {ref(&v)}); }
  virtual void anim_stop(Handle anim) { unported("0x00506ca0", {low(anim)}); }
  virtual void weapons_free(void* w);  // 0x004b01d0 and the delete
  virtual void net_collision_free(void* n) { unported("delete", {low(n)}); }
  virtual void shadow_release(Vehicle& v) { unported("0x004d58a0", {ref(&v)}); }  // AiTarget's dtor
  // The three HUD markers (zTwoD images 0x0059b830, vtable 0x005bce78, +0x48 1).
  virtual Handle marker_create(const char* name) { return h(unported("0x0059b830", {name, 0, 0, 1})); }
};
void set_veh_basic_hooks(VehBasicHooks* h);  // null: the defaults

// Mech3.exe 1.2 @ 0x004ac320: the vehicle `name` of `type`, its node the
// world's `name` or a copy of `type`'s; null without either.
BasicVehicle* vehicle_create(Handle ctl, const char* type, const char* name);
void vehicle_construct(BasicVehicle& v, Handle ctl, Handle node);  // 0x004ac530
void vehicle_destroy(BasicVehicle& v);  // 0x004ac650 over 0x004aeb50
void vehicle_delete(BasicVehicle* v);   // 0x004ac630(1)

// Mech3.exe 1.2 @ 0x004ac3d0: the vehicles' HUD markers, ally, enemy, other
// ([0x0060b120], [0x0060b124], [0x0060b128]).
struct VehicleMarkers {
  Handle ally = nullptr, enemy = nullptr, other = nullptr;
};
VehicleMarkers& vehicle_markers();
void vehicle_markers_create();

}  // namespace mech3
