#pragma once
#include <cstdint>
#include <list>
#include <string>

#include "gamez/zgame/unported.h"
#include "mech3/zai_steps.h"

namespace mech3::ai {
struct NetNodeRec;
}

// The waynets (docs/vehicles.md, The mcbwaynet): numbered nets of named
// points with one selected, shown in the world by the "waypoint" node.
// mech3\mech_waynet.cpp (0x00479640..0x0047a950). The mcbwaynet
// ([0x0060b544]) holds the field bases' dock points, made from the
// arrangenodes nodes as the nets compile; the player's MFB command picks one
// and asks the dock. The multiplayer nav waynet ([0x00609980], net files 105
// and 107, 0x004797c0) and the HUD's waypoint arrows (0x00479dd0..) are not
// ported: no shipped single-player mission has a nav waynet.
namespace mech3 {

using Handle = void*;

// A point (0x0047a320, 0x38 bytes, vtable 0x005c3828): a Behaviour at a
// place, named twice (the record's name and its auxname, which it answers
// by), with its links' first.
struct WaynetPoint : ai::Behaviour {
  WaynetPoint(const char* name, const char* label, const float pos[3], const int32_t* links, int32_t n, uint8_t mfb,
              uint8_t xf, uint8_t xd, uint8_t xe, uint8_t hidden);  // 0x0047a320
  ~WaynetPoint() override = default;                                  // 0x0047a480
  const float* position() const override { return pos; }            // vt+0x18 0x00402b60
  const char* label_or_null() const { return has_label ? label.c_str() : nullptr; }  // vt+0x24 0x0047a930
  bool set_position(const float* p);                                  // vt+0x2c 0x0047a530
  void set_reached(uint8_t on);                                       // 0x0047a500: its line once

  uint8_t mfb = 0;            // +0x0c  a dock point
  uint8_t xd = 0, xe = 0, xf = 0;  // +0x0d..+0x0f
  uint8_t hidden = 0;         // +0x10  the record's hidden
  Handle sound = nullptr;     // +0x14  "nav<label>" (not a dock point's)
  uint32_t x18 = 0;           // +0x18
  float pos[3] = {};          // +0x1c
  std::string name;           // +0x28  strdup'd
  bool has_name = false;
  std::string label;          // +0x2c
  bool has_label = false;
  int32_t first_link = 0;     // +0x30
  uint8_t reached = 0;        // +0x34
};

// A waynet (0x00479640, vtable 0x005c37b0, over the compiled net's
// 0x004bfe40: element 0 the default behaviour, the points after).
class Waynet : public ai::CompiledNet {
 public:
  Waynet(int32_t id, const char* name);  // 0x00479640
  ~Waynet() override;                     // 0x004796f0

  virtual void begin();                   // vt+0x2c 0x00479a50: the marker found (once), point 0 selected
  virtual void next();                    // vt+0x60 0x00479b10
  virtual void prev();                    // vt+0x64 0x00479b30
  virtual void nearest(const float* pos);  // vt+0x68 0x00479b50
  bool select(int32_t i);                 // 0x00479b90: false (the marker hidden) out of range
  void reselect() { select(current); }   // 0x00479bd0
  void select_point(znet::Element* e) { select(index_of(e)); }  // 0x0047a250
  WaynetPoint* find(const char* label) const;  // 0x00479be0 (case not counted)
  const float* position_of(int32_t i) const;   // 0x00479aa0: the zero vector without an element
  void place_marker();                    // 0x00479ac0
  void add(znet::Element* e) { elems.push_back(e); }  // 0x0047a010
  bool remove_last(znet::Element* e);    // 0x0047a1e0: when it is the last
  void note(int32_t v, bool add);         // 0x0047a270: the +0x28 list

  std::string label;                      // +0x1c  strdup'd
  int32_t current = 0;                    // +0x20
  uint8_t x24 = 0;                        // +0x24  the id's low byte
  std::list<int32_t> x28;                 // +0x28
};

// The mcbwaynet (0x0047a550, 0x48 bytes, vtable 0x005c38e0) and the MFB
// command's state.
class McbWaynet : public Waynet {
 public:
  using Waynet::Waynet;
  void next() override;                   // vt+0x60 0x0047a5c0
  void prev() override;                   // vt+0x64 0x0047a620
  void nearest(const float* pos) override;  // vt+0x68 0x0047a680
  void clear();                           // 0x0047a6b0 (0x0047a720: the dock's marker clear)
  void command(Handle mech);              // 0x0047a7a0 (0x0047a910, MCB_COMMAND 0x73)

  int32_t passed = 0;                     // +0x30  how often the picked point came round
  znet::Element* picked = nullptr;        // +0x34
  znet::Element* chosen = nullptr;        // +0x38  the point the request went with
  znet::Element* nav_point = nullptr;     // +0x3c  the mission's waynet's point before
  Handle nav = nullptr;                   // +0x40  the mission's waynet before ([0x00609980])
  int32_t step = 0;                       // +0x44  0 to pick, 1 to ask, 2 to cancel
};

WaynetPoint* as_point(znet::Element* e);  // vt+0x78: a point itself, else none (0x0040bae0)
McbWaynet* mcb_waynet();       // [0x0060b544]
McbWaynet* mcb_waynet_make();  // 0x004d5460: made once (an id of 10000 up, kept by znet's clear)
void mcb_waynet_free();        // 0x004d54f0 (the mission's cleanup)
// 0x0047a730: a mech (its vt+0x50) made the dock's requester, while a base
// is up; 0x0047a780 is the same through the mcbwaynet (the AI's 0x004ea2b7).
bool mfb_set_requester(Handle mech);
// The net compile's (0x004bde20): an arrangenodes node's point in the
// mcbwaynet, unless one of its auxname is there.
void waynet_point_from_node(const ai::NetNodeRec& r, const float pos[3]);

// The calls out.
class WaynetHooks : public zgame::Unported {
 public:
  static uint32_t low(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
  static Handle h(uint32_t v) { return reinterpret_cast<Handle>(static_cast<uintptr_t>(v)); }
  virtual uint32_t ref(const void* p) { return low(p); }
  virtual void on_new(uint32_t size, const void* p) { (void)size, (void)p; }
  virtual int32_t alloc_id();  // 0x0058b1c0: znet's first free in 10000..10099 ([0x005f2a48])
  // The icons' count (0x004798f0 / 0x00479a30): the HUD's two waypoint
  // icons and eight images made with the first waynet, freed with the last
  // (0x00479850, which also forgets the marker).
  virtual void icons_make() { unported("0x004798f0 (the waypoint icons)", {}); }
  virtual void icons_free() { unported("0x00479850 (the waypoint icons)", {}); }
  // The "waypoint" node (0x004f7fe0(6)), its parents (+0x54), the world's
  // add ([0x00608c84] vt+0x00), shown (0x004eb170), its place (0x004fc0b0).
  virtual Handle node_named(const char* name) { return h(unported("0x004f7fe0", {6, name})); }
  virtual int32_t node_parents(Handle node) { return static_cast<int32_t>(unported("node+0x54", {low(node)})); }
  virtual void world_add(Handle node) { unported("world.vt+0x0", {low(node)}); }
  virtual void node_set_active(Handle node, int32_t on) { unported("0x004eb170", {low(node), on}); }
  virtual void node_set_translation(Handle node, float x, float y, float z) {
    unported("0x004fc0b0", {low(node), zgame::bits(x), zgame::bits(y), zgame::bits(z)});
  }
  // A point's nav line: zSound's by name (0x0056f6f0) and played (0x0056e3d0).
  virtual Handle sound_find(const char* name) { return h(unported("0x0056f6f0", {name})); }
  virtual void sound_play(Handle s) { unported("0x0056e3d0", {low(s), 0x3f800000}); }
  // The MFB command's: the mission's waynet ([0x00609980], the missions'
  // +0xb0: 0x00491d00 sets it and reselects), a mech's place (0x005a85e0)
  // and vehicle (vt+0x50), the dock (the registry 0x00605ae0: its alive
  // count, idle state, request 0x0045a500, front's abort 0x0045a540, its
  // requester 0x0045b740, the radio lines +0x98 / +0x9c, 0x004e6d50).
  virtual Handle mission_waynet() { return h(unported("[0x00609980]", {})); }
  virtual void set_mission_waynet(Handle w) { unported("0x00491d00", {0x6098d0, low(w)}); }
  virtual const float* entity_position(Handle e) { return reinterpret_cast<const float*>(h(unported("0x005a85e0", {low(e)}))); }
  virtual Handle entity_vehicle(Handle e) { return h(unported("entity.vt+0x50", {low(e)})); }
  virtual int32_t bases_alive();
  virtual bool dock_idle();
  virtual void dock_message(Handle mech);
  virtual void dock_abort();
  virtual bool dock_set_requester(Handle mech);
  virtual void say(int32_t line);  // the registry's sounds[line]
};
void set_waynet_hooks(WaynetHooks* h);  // null: the defaults
WaynetHooks& waynet_hooks();
// The marker ([0x00608ba4]) and the icons' count ([0x00608ba0]).
Handle& waynet_marker();
int32_t& waynet_icons_count();

}  // namespace mech3
