#pragma once
#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <string>

#include "gamez/zgame/unported.h"

namespace zrdr {
struct Node;
}

// C:\zipper\Mech3\mech_objective.cpp: a mission's objectives (docs/objectives.md).
// objectives.zrd (mpinfo.zrd in multiplayer) has a block per mission with a
// description and four groups: primary, secondary, final and fail. A group
// holds conditions, one object per type; obj_list nests groups. This is the
// reading side (0x0045c2d0 and below); the start and the checks come later.
namespace mech3 {

using zgame::Handle;
class Store;

// The calls out of the reading, by the original's address.
class ObjectiveHooks : public zgame::Unported {
 public:
  virtual zrdr::Node* load(const char* name);                // 0x00556740: zrdr::load
  virtual void free_tree(zrdr::Node* root);                  // 0x00556820: zrdr::free_node
  virtual const char* localize(const char* name);            // 0x00575fb0: zutl::localize
  virtual const char* message(int32_t id) { unported("0x00576040", {id}); return nullptr; }
  // 0x00575fd0 into 0x40 bytes: message `id` with one string.
  virtual std::string message_with(int32_t id, const char* s) {
    unported("0x00575fd0", {0x40, id, s});
    return std::string();
  }
  virtual Handle sound_find(const char* name) { return unported("0x0056f6f0", {name}); }
  virtual Handle entity_find(const char* name) { return unported("0x004d7330", {name}); }
  virtual uint8_t entity_flag(Handle e) { return static_cast<uint8_t>(unported("entity+0x08", {e})); }
  // The entity's vt+0xa4: the name an objective shows (null: none).
  virtual const char* entity_label(Handle e) { unported("entity.vt+0xa4", {e}); return nullptr; }
  // The nav points ([0x00609980], 0x00479be0 by name; 0 without the list).
  virtual Handle nav_find(const char* name) { return unported("0x00479be0", {0x609980, name}); }
  virtual void nav_show(Handle nav, uint8_t on) { unported("0x0047a270", {0x609980, nav, on}); }
  virtual Handle node_find(const char* name) { return unported("0x004f7fe0", {6, name}); }
  virtual Handle subnode_find(Handle node, const char* name) { return unported("0x004f9d60", {node, name}); }
  virtual Handle valve_find(const char* name) { return unported("0x004d3c40", {name}); }
  virtual void valve_set(Handle valve, int32_t v) { unported("0x004d3c00", {valve, v}); }
  // The pilot's game ([0x0060861c]+0x6c) vt+0x2c (gametypes.md); -1 without one.
  virtual int32_t game_type() { return static_cast<int32_t>(unported("game.vt+0x2c", {})); }
  virtual void training_free() { unported("game.vt+0x44", {}); }  // training keeps its own
  virtual void report(int32_t level, int32_t line, const char* fmt, const char* a, Handle b) {  // 0x0049bfd0
    unported("0x0049bfd0", {level, "C:\\zipper\\Mech3\\mech_objective.cpp", line, fmt, a, b});
  }
  virtual void default_point(float out[3]) { out[0] = out[1] = out[2] = 0.0f; }  // 0x0063d870
};

// A node_state entry of cond_salvage (0x0045e9f0), 0x0c bytes.
struct NodeSalvage {
  Handle node = 0;                 // +0x00
  uint8_t active = 0;              // +0x04  its "state" is "active"
  std::unique_ptr<Store> salvage;  // +0x08
  NodeSalvage();
  NodeSalvage(NodeSalvage&&) noexcept;
  ~NodeSalvage();                  // 0x0045eb70
};

// The base (vtable 0x005c2660, 0x34 bytes; 0x0045b9e0, dtor 0x0045bb00).
class Objective {
 public:
  // Which vtable the original gives it.
  enum Kind : uint8_t { kGroup, kDestroy, kNodeChange, kReachArea, kValveChange };
  Objective(Kind k, const char* desc);
  virtual ~Objective();
  Objective(const Objective&) = delete;
  Objective& operator=(const Objective&) = delete;
  void set_desc(const char* text);  // 0x0045f110

  Kind kind;
  Handle nav = 0;                       // +0x04
  uint8_t done = 0;                     // +0x08
  std::optional<std::string> desc;      // +0x0c
  std::unique_ptr<Store> salvage;       // +0x10
  // +0x14: the low byte of the description's pointer (the constructor reads
  // its one argument twice); nothing reads it.
  std::list<NodeSalvage> node_salvage;  // +0x18, count +0x1c
  Handle sound = 0;                     // +0x20
  Handle valve = 0;                     // +0x24  set_valve's
  int32_t value = 1;                    // +0x28
  std::optional<std::string> persist;   // +0x2c
  uint8_t visible = 1;                  // +0x30
  uint8_t silent = 0;                   // +0x31
};

// A group: a block's list or obj_list (vtable 0x005c2678, 0x4c bytes;
// dtor 0x0045e410).
class ObjectiveGroup : public Objective {
 public:
  explicit ObjectiveGroup(const char* desc) : Objective(kGroup, desc) {}
  uint8_t b34 = 1;                      // +0x34
  uint8_t simultaneous = 0;             // +0x35
  int32_t goal_count = 0;               // +0x38
  int32_t x3c = 0;                      // +0x3c
  // +0x40: a byte of whatever the stack holds (a pointer's low byte in
  // 0x0045bf60, unset locals elsewhere); nothing reads it.
  std::list<std::unique_ptr<Objective>> items;  // +0x44, count +0x48
};

// destroy_entity (vtable 0x005c2668 over the named target 0x005c2680, 0x40 bytes).
class DestroyObjective : public Objective {
 public:
  DestroyObjective(const char* desc, const char* name, Handle entity);  // 0x0045f2b0 inline
  std::optional<std::string> name;      // +0x34
  Handle entity = 0;                    // +0x38
  uint8_t x3c = 0;                      // +0x3c
  void find_entity(ObjectiveHooks& h);  // 0x0045e630
  void relabel(ObjectiveHooks& h);      // 0x0045f340
};

// node_change (vtable 0x005c2730, 0x3c bytes).
class NodeChangeObjective : public Objective {
 public:
  NodeChangeObjective(const char* desc, Handle n, uint8_t a) : Objective(kNodeChange, desc), node(n), active(a) {}
  Handle node;                          // +0x34
  uint8_t active;                       // +0x38
};

// reach_area (vtable 0x005c2728, 0x4c bytes): a node, a nav point or a point.
class ReachAreaObjective : public Objective {
 public:
  ReachAreaObjective(const char* desc, Handle n, const float p[3], float r);
  Handle node;                          // +0x34
  float point[3];                       // +0x38
  float radius;                         // +0x44
  float radius_sq;                      // +0x48
};

// valve_change (vtable 0x005c26f8, 0x3c bytes).
class ValveChangeObjective : public Objective {
 public:
  ValveChangeObjective(const char* desc, Handle v, int32_t x) : Objective(kValveChange, desc), watch(v), target(x) {}
  Handle watch;                         // +0x34
  int32_t target;                       // +0x38
};

// The missions object's +0x78..+0x88.
struct Objectives {
  std::optional<std::string> desc;      // +0x78 (calloc'd)
  std::unique_ptr<ObjectiveGroup> primary, secondary, final, fail;  // +0x7c..+0x88
};

// Mech3.exe 1.2 @ 0x0045c2d0: `block` of `file` into `o`; false when the file
// or block is missing or no group has an objective.
bool objectives_load(const char* file, const char* block, Objectives& o, ObjectiveHooks& h);
bool read_objectives(zrdr::Node* block, Objectives& o, ObjectiveHooks& h);      // 0x0045c340
void read_group(zrdr::Node* block, const char* key, const char* default_desc,
                std::unique_ptr<ObjectiveGroup>& out, ObjectiveHooks& h);        // 0x0045bf60
void add_mobile_hq(const char* desc, std::unique_ptr<ObjectiveGroup>& fail, ObjectiveHooks& h);  // 0x0045bc90
// 0x0045c920: the condition whose type name is `slot` (its arguments follow
// it). Types the port does not read yet report themselves through unported().
std::unique_ptr<Objective> parse_condition(zrdr::Node* slot, ObjectiveHooks& h);
std::unique_ptr<Store> read_salvage(zrdr::Node* node);                           // 0x004711a0
void read_node_salvage(Objective& o, zrdr::Node* cond, ObjectiveHooks& h);       // 0x0045e9f0
Handle find_node(zrdr::Node* args, ObjectiveHooks& h);                           // 0x0045c810 ("node")
void objectives_free(Objectives& o, ObjectiveHooks& h);                          // 0x0045c1d0
void objectives_delete(Objectives& o);                                           // 0x0045c250

}  // namespace mech3
