#pragma once
#include <cstdint>

#include "gamez/zgame/unported.h"
#include "mech3/veh_controls.h"
#include "mech3/zainet105.h"

// The basic vehicle's own controls (docs/vehicles.md, The basic vehicle's own controls): what a vehicle made
// without AI controls makes for itself (0x004ac860(0xa70), then 0x004ae270). Ported for preservation, not wired
// in: no shipped mission makes such a vehicle (the basic vehicle's constructor still answers through its hooks).
// Original file unknown: these sit after the basic vehicle (0x004ac750..) and before mech3\weapon.cpp.
namespace mech3 {

// The record a net number names: a version-105 AI net (mech3/zainet105.h, ai::Net105) on [0x005f4c4c].
using Net105Record = ai::Net105;

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

  // The path follower's (0x00401050..): an entity's place (0x005a85e0) and lock point (0x005a8630), active (+0x08)
  // and velocity (+0x30); its centroid's world matrix refreshed when stale (its data's flag 0x20: 0x0051e9d0,
  // 0x0051fde0, 0x004ed280, 0x0051ea00).
  virtual const float* entity_position(Handle e) { return owner_position(e); }
  virtual const float* entity_lock(Handle e) { return reinterpret_cast<const float*>(h(unported("0x005a8630", {ref(e)}))); }
  virtual bool entity_active(Handle e) { return unported("entity+0x08", {ref(e)}) != 0; }
  virtual Handle entity_velocity(Handle e) { return h(unported("entity+0x30", {ref(e)})); }
  virtual void entity_refresh(Handle e) { unported("entity matrix", {ref(e)}); }
  // The squared distance between two entities' centroids (0x0051dfa0 on their centroid nodes' data +0x38).
  virtual float centroid_distance_sq(Handle v, Handle target) {
    return zgame::from_bits(unported("0x0051dfa0 centroids", {ref(v), ref(target)}));
  }
  virtual uint32_t def_flags(Handle def) { return unported("def+0x6c", {ref(def)}); }  // a weapon def's flags
  virtual bool line_clear(Handle v, Handle target) { return unported("0x004d5980", {ref(v), ref(target), 0}) != 0; }
  // The vehicle's sweep (0x004a0910(points, 2)) and what it hit (the controls' +0x360 walls, +0x354 objects), then
  // its clear (0x004a0100).
  virtual void sweep(Handle v, const float points[6]) { unported("0x004a0910", {ref(v), ref(points), 2}); }
  virtual bool sweep_hit(BasicControls& c);
  virtual void sweep_clear(Handle v) { unported("0x004a0100", {ref(v)}); }
  virtual void beam_start(Handle beam, int32_t serial) { unported("0x00581230", {ref(beam), serial}); }  // ECX, EDX the controls' +0x14
  virtual float now() { return zgame::from_bits(unported("[0x0060991c]", {})); }
  virtual int32_t rand();                                          // the CRT's
  virtual void node_free(ai::Net105Node* n);                       // 0x004024a0: a node made at run time, freed
  virtual ai::Net105Node* node_make(BasicControls& c);             // new 0x34, 0x004ae8e0
  virtual void link_fill(ai::Net105Link* rec, const float* to, const float* from, float width);  // 0x004023d0
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
  // IControls vt+0x0c's (0x004ae850, through 0x004aea10): the frame counter [0x006099c0] and +0x2f8's bit 1 are
  // the caller's; not destroyed, the follower toward the player's mech ([0x0060861c]+0x14).
  void follow(Handle target);                                    // 0x00401050
  int32_t path_goal() override;                                  // vt+0x08 0x004ae350: the node's +0x2c
  bool destroyed() override { return state == 4; }               // vt+0x1c 0x004ae6a0

  int32_t net = 0;           // +0xa04  the net's number: the constructor's second argument, then the setup's third
  Net105Record* record = nullptr;  // +0xa08
  Handle net_node = nullptr; // +0xa0c  the record's net's node nearest the vehicle (its +0x2c the goal)
  int32_t mode = 0;          // +0xa14  0 the waypoints, 2 and 3 the record's other kinds, 4 stopped
  float xa18 = 0;            // +0xa18  no engage before this time (the dwell's end)
  float attack_dwell = 0;    // +0xa1c  the record's, else 10
  float not_pursuit_dwell = 0;  // +0xa20  the record's, else the definition's, else 3
  float attack_rad = 0;      // +0xa24  the record's squared, else 1500
  float pursuit_far = 0;     // +0xa28  pursuit_range's larger squared (the definition's second)
  float pursuit_near = 0;    // +0xa2c  its smaller squared (the two put in order by the setup)
  float return_range = 0;    // +0xa30  squared
  float activate_rad = 0;    // +0xa44  squared (the definition's "activation")
  int32_t link = 0;          // +0xa10  the net node's link being followed
  float chase[3] = {};       // +0xa34  the link node's place when the pursuit began
  float crumb_at = 0;        // +0xa40  the time after which the pursuit drops its next breadcrumb node
  uint32_t ahead = 0;        // +0xa54  the link node was ahead last frame
  // The base's fields the follower writes that VehicleControls does not name (as StealthControls' +0x40..+0x4c).
  float in40 = 0, pitch_in = 0, out48 = 0, pitch_out = 0;  // +0x040..+0x04c  submarine pitch, in and curved
  uint32_t x65c = 0;         // +0x65c  a locking weapon's lock on
  Handle x664 = nullptr;     // +0x664  its target's lock point (0x005a8630)
  Handle x668 = nullptr;     // +0x668  its target's velocity (+0x30)
  float x7ec = 0;            // +0x7ec  (the motion's +0x88) the submarine's depth offset
};

}  // namespace mech3
