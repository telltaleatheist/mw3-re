#pragma once
#include <cstdint>
#include <vector>

#include "gamez/zgame/unported.h"
#include "gamez/zclass/cls_di.h"
#include "mech3/mech.h"
#include "mech3/veh_controls.h"

namespace zrdr {
struct Node;
}

// The basic vehicle (trucks, APCs, tanks; the field base and the stealth
// object are made over it), docs/vehicles.md. Original file: these sit just
// before mech3\veh_basic.cpp (0x004ad930 is its first named function), with
// ground_pick (mech3/veh_basic.h); kept apart from it because mech.h's Handle
// clashes with player.h's.
namespace mech3 {

// One engine sound (CZEngineSound, 0xc bytes, 0x004adb90 / 0x004adbb0): the
// sound by its name and the voice playing it, shared by every vehicle with
// that engine.
struct EngineSoundEntry {
  uint32_t x00 = 0;         // +0x00
  Handle sound = nullptr;   // +0x04  zSound's (0x0056f6f0)
  Handle voice = nullptr;   // +0x08  set by the frame (step 2)
};

// The vehicles' engine sounds (CZEngineSoundList, the global at 0x0060b110):
// made at start-up (0x004ada90 -> 0x004ad840) with room for 64 and never
// grown, emptied at each mission's end (0x004d71f0 -> 0x004add10).
struct EngineSoundList {
  std::vector<EngineSoundEntry> entries;  // +0x04..+0x0c
  EngineSoundList() { entries.reserve(kCapacity); }
  static constexpr size_t kCapacity = 0x300 / 0xc;
};
EngineSoundList& engine_sounds();
EngineSoundEntry* engine_sound_find(const char* name);  // 0x004ad930; null when none or full
void engine_sounds_clear();                             // 0x004add10 (0x004d71f0)

// The engine sound at +0xe8 (vtable 0x005c8b90): the definition's
// sounds/engine entry, from the vehicles' shared cache at 0x0060b110
// (0x004adb20 -> 0x004ad930, step 1b), else null.
struct EngineSound {
  EngineSoundEntry* entry = nullptr;  // +0x04
  void set(const char* name);         // 0x004adb20
};

// The motion animation at +0xf0 (0x004aec00 zeroes it, a body the linker
// shares with IControls' hit record): the definition's motion_anim
// (0x004ade40, step 1b), stopped (0x00506ca0) as it goes (0x004addb0 ->
// 0x004ade70).
struct MotionAnim {
  Handle def = nullptr;    // +0x00  zEffect's anim def (0x0050b450)
  Handle anim = nullptr;   // +0x04  the one running (step 2)
  uint32_t x08 = 0;        // +0x08
  void set(const char* name);  // 0x004ade40
};

// 0xfc bytes, vtable 0x005c88e8 (89 slots).
struct BasicVehicle : Vehicle {
  uint32_t xe0 = 0;              // +0xe0
  VehicleControls* controls = nullptr;  // +0xe4  given (the AI's), else its own made here (0x004ae270)
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
  // The engine sounds and the motion animation (step 1b).
  virtual Handle sound_find(const char* name);           // 0x0056f6f0: zsnd::find
  virtual const char* sound_name(Handle sound);          // 0x0056f740: zsnd::name_of
  virtual void voice_unlock(Handle voice);               // 0x0056f410: zsnd::unlock
  virtual void voice_stop(Handle voice);                 // 0x0056e9f0: zsnd::stop
  virtual Handle anim_find(const char* name);            // 0x0050b450: zeff::anim_find
  // 0x0049bfd0, empty in this build (zerr::report here).
  virtual void report(int32_t level, const char* file, int32_t line, const char* message);
  // The definition (zReader): 0x005569e0, its node's type, 0x005569f0, 0x00557090.
  virtual zrdr::Node* reader_find(zrdr::Node* node, const char* key);
  virtual int32_t reader_type(zrdr::Node* node);
  virtual const char* reader_string(zrdr::Node* node, const char* key);
  virtual bool reader_string_value(zrdr::Node* node, const char* key, const char** out);
  // The controls' vt+0x0c (the AI's setup).
  virtual void controls_setup(Handle c, zrdr::Node* def, const char* name, int32_t kind) {
    unported("ctl.vt+0xc", {low(c), low(def), name, kind});
  }
  // The placement's (step 1c).
  virtual int32_t ground(float x, float y, float z, zcls::GroundHits* out);  // 0x004edab0 on [0x00608c84]+4
  virtual void set_position(BasicVehicle& v, const float pos[3]);  // vt+0x18, zEntity3d's 0x005a8550
  virtual void refresh(Vehicle& v) { unported("0x005a85c0", {ref(&v)}); }
  // The three HUD markers (zTwoD images 0x0059b830, vtable 0x005bce78, +0x48 1).
  virtual Handle marker_create(const char* name) { return h(unported("0x0059b830", {name, 0, 0, 1})); }
};
void set_veh_basic_hooks(VehBasicHooks* h);  // null: the defaults

// Mech3.exe 1.2 @ 0x004ac320: the vehicle `name` of `type`, its node the
// world's `name` or a copy of `type`'s; null without either.
BasicVehicle* vehicle_create(Handle ctl, const char* type, const char* name);
void vehicle_construct(BasicVehicle& v, Handle ctl, Handle node);  // 0x004ac530
// Mech3.exe 1.2 @ 0x0049ec00 (vt+0x158): the definition's sounds/engine and
// motion_anim, then the controls' own setup.
void vehicle_setup(BasicVehicle& v, zrdr::Node* def, const char* name, int32_t kind);
// Mech3.exe 1.2 @ 0x004a3630 (vt+0xdc): on the ground under `pos` (from 10
// above it, 0.1 over the first polygon) or where given, facing `yaw`.
void vehicle_place(BasicVehicle& v, const float pos[3], float yaw);
// The speeds and turns the AI asks a vehicle (vt+0x114..+0x13c, the slots
// the mechs answer in mech.h), from its motion block: the top speed is
// rates[1], the slowing rate_damping[1], the turn rate turns[1]. Without
// controls, 0 (the exe reads address 0x1c for the stopping distance).
float vehicle_move_speed(const BasicVehicle& v);                  // vt+0x114 0x004acfa0: the throttle's
float vehicle_speed_at(const BasicVehicle& v, float share);       // vt+0x118 0x004acf70
float vehicle_share_for(const BasicVehicle& v, float speed);      // vt+0x11c 0x004acfe0
float vehicle_stopping_distance(const BasicVehicle& v, float speed);  // vt+0x120 0x004ad020
float vehicle_stopping_now(const BasicVehicle& v);                // vt+0x124 0x004ad050
float vehicle_stopping_at(const BasicVehicle& v, float share);    // vt+0x128 0x004ad0b0
float vehicle_stop_share(const BasicVehicle& v, float d);         // vt+0x12c 0x004ad100, 0..1
float vehicle_turn_time(const BasicVehicle& v, float angle);      // vt+0x130 0x004ad180 (a second argument unread)
float vehicle_turn_time_now(const BasicVehicle& v);               // vt+0x134 0x004ad250
float vehicle_turn_time_at(const BasicVehicle& v, float share);   // vt+0x138 0x004ad290
float vehicle_turn_share(const BasicVehicle& v, float a, float b);  // vt+0x13c 0x004ad1c0, 0..1
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
