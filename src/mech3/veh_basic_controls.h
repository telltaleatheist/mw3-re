#pragma once
#include <cstdint>

#include "gamez/zgame/unported.h"
#include "mech3/veh_controls.h"

// The basic vehicle's own controls (docs/vehicles.md, The basic vehicle's own controls): what a vehicle made
// without AI controls makes for itself (0x004ac860(0xa70), then 0x004ae270). Ported for preservation, not wired
// in: no shipped mission makes such a vehicle (the basic vehicle's constructor still answers through its hooks).
// Original file unknown: these sit after the basic vehicle (0x004ac750..) and before mech3\weapon.cpp.
namespace mech3 {

// A record of the version-105 AI nets ([0x005f4c4c], 0x5c bytes, made by 0x004021c0; docs/ai.md 17.9), as the
// basic controls read it. A stand-in until that group (dev 1's ai::Net105, zainet105.h) lands: only the fields
// these controls read, by its names.
struct Net105Record {
  int32_t id = 0;                 // +0x18  the net's number, the controls' +0xa04
  int32_t kind = 0;               // +0x30  type "ST" 0 (the waypoints, mode 0), "HI" 1 (mode 2), "FI" 2 (mode 3)
  float activate_rad = 0;         // +0x38  each of these 0: the vehicle definition's, else the default
  float attack_rad = 0;           // +0x3c
  float attack_dwell = 0;         // +0x40
  float not_pursuit_dwell = 0;    // +0x44
  float pursuit_range[2] = {};    // +0x48, +0x4c  the larger first
  float return_range = 0;         // +0x50
  Net105Record* next = nullptr;   // +0x58
};
Net105Record*& net105_records();  // [0x005f4c4c], the list's head

struct BasicControls;

// The calls out, by the original's address (the defaults the ports).
class BasicControlsHooks : public zgame::Unported {
 public:
  using Handle = void*;
  static uint32_t low(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
  static Handle h(uint32_t v) { return reinterpret_cast<Handle>(static_cast<uintptr_t>(v)); }
  virtual uint32_t ref(const void* p) { return low(p); }
  virtual void base_init(BasicControls& c);                                      // 0x004ac8a0
  virtual void base_setup(BasicControls& c, zrdr::Node* def, uint32_t arg2);     // 0x004acbc0(def, arg2, 0)
  virtual Net105Record* record(int32_t id);                                      // 0x004022c0
  virtual const float* owner_position(void* owner) { return reinterpret_cast<const float*>(h(unported("0x005a85e0", {ref(owner)}))); }
  // The record's net's element nearest a place (0x004d51d0) and its node (the element's vt+0x80).
  virtual Handle nearest_element(Net105Record* r, const float* pos) { return h(unported("0x004d51d0", {ref(r), ref(pos)})); }
  virtual Handle element_node(Handle e) { return h(unported("element.vt+0x80", {ref(e)})); }
  // The net's node's +0x2c (its goal's number; the v105 node class 0x005c8cb8, set -1 by 0x004ae8e0).
  virtual int32_t node_goal(Handle n) { return static_cast<int32_t>(unported("node+0x2c", {ref(n)})); }
  virtual Handle node_find(Handle root, const char* name) { return h(unported("0x004f9d60", {ref(root), name})); }
  virtual void node_set_intersect_bbox(Handle n, int32_t on) { unported("0x004eb750", {ref(n), on}); }
  virtual void node_set_flag8(Handle n, int32_t on) { unported("0x004eb620", {ref(n), on}); }
  virtual zrdr::Node* reader_find(zrdr::Node* def, const char* key) {
    return static_cast<zrdr::Node*>(h(unported("0x005569e0", {ref(def), key})));
  }
  // A found value's float, the list's item i (the list at its +0x04, 8 bytes an item: item i's value at +4 + 8 i).
  virtual float reader_float(zrdr::Node* n, int32_t i);
};
BasicControlsHooks& basic_controls_hooks();
void set_basic_controls_hooks(BasicControlsHooks* h);  // null: the defaults

// 0xa70 bytes over the base's 0xa04 (vtables 0x005c8c50 and IControls 0x005c8b98), zeroed by 0x004ac860; the
// defaults in the comments are init's.
struct BasicControls : VehicleControls {
  BasicControls(void* vehicle, int32_t net_number);  // 0x004ae270: the base's, then init
  ~BasicControls() override = default;      // 0x004ae380

  void init() override;                     // vt+0x10 0x004ae3c0
  void setup_with(zrdr::Node* def, uint32_t arg2, int32_t net);  // vt+0x0c 0x004ae6b0 (def, arg2, the net's number)
  int32_t path_mode() override { return mode; }                  // vt+0x04 0x004ae340
  int32_t path_goal() override;                                  // vt+0x08 0x004ae350: the node's +0x2c
  bool destroyed() override { return state == 4; }               // vt+0x1c 0x004ae6a0

  int32_t net = 0;           // +0xa04  the net's number: the constructor's second argument, then the setup's third
  Net105Record* record = nullptr;  // +0xa08
  Handle net_node = nullptr; // +0xa0c  the record's net's node nearest the vehicle (its +0x2c the goal)
  int32_t mode = 0;          // +0xa14  0 the waypoints, 2 and 3 the record's other kinds, 4 stopped
  uint32_t xa18 = 0;         // +0xa18
  float attack_dwell = 0;    // +0xa1c  the record's, else 10
  float not_pursuit_dwell = 0;  // +0xa20  the record's, else the definition's, else 3
  float attack_rad = 0;      // +0xa24  the record's squared, else 1500
  float pursuit_far = 0;     // +0xa28  pursuit_range's larger squared (the definition's second)
  float pursuit_near = 0;    // +0xa2c  its smaller squared (the two put in order by the setup)
  float return_range = 0;    // +0xa30  squared
  float activate_rad = 0;    // +0xa44  squared (the definition's "activation")
};

}  // namespace mech3
