#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "gamez/zreader/zreader.h"
#include "mech3/zainet.h"

// The version-105 AI nets (docs/ai.md 17.9): the older net class a version-105 net file builds (0x004025b0, through
// 0x00402590), the records' list it keeps ([0x005f4c4c] / [0x005f4c50]) and its 0x34-byte nodes. No shipped net file
// is version 105, so nothing in the game reaches them; ported for preservation, not wired in. Original file: the
// exe's first source file (0x004021c0..0x00402b20); the node's own methods sit with the basic vehicle's controls
// (0x00403cd0..0x00403e70) and zaivehicle's (0x004ae8e0..0x004ae980).
namespace mech3::ai {

// One link's values (malloc(0x3c), zeroed; filled by the basic controls' 0x004023d0): the way to the linked node.
struct Net105Link {
  float offset[3] = {};  // +0x00  to the linked node, then flat-normalised (0x0051e210)
  float length = 0;      // +0x0c  the flat length less the width, or half the width when that is less
  float side[3] = {};    // +0x10  across the offset (0x00522460)
  float left[3] = {};    // +0x1c  the side turned +45 degrees (0x00523e70)
  float right[3] = {};   // +0x28  and -45
  float width = 0;       // +0x34
  uint8_t x38[4] = {};   // +0x38
};
static_assert(sizeof(Net105Link) == 0x3c);

// A node (0x34 bytes, vtable 0x005c8cb8, constructor 0x004ae8e0): its place, three links to other nodes (by number
// in the file until 0x00402300 resolves them) with a link record each, and its number. The null node, which every
// "none" answer gives, is [0x005e8654] (`none()`); its link methods do nothing.
class Net105Node : public AiElem {
 public:
  // Mech3.exe 1.2 @ 0x004ae8e0: all 0, the number -1.
  Net105Node() = default;
  ~Net105Node() override;  // 0x004024a0's frees: each link record

  static Net105Node* none();  // [0x005e8654], 0x004ae920 (vt+0x10)
  bool is_none() const { return this == none(); }
  int32_t link_count() const { return 3; }  // vt+0x04 (0x0041ec30)
  // Mech3.exe 1.2 @ 0x004ae950 (vt+0x08, through vt+0x7c)
  // Mech3.exe 1.2 @ 0x004ae960 (vt+0x7c): link i, the null node for none or out of 0..2.
  Net105Node* link(int32_t i) const { return i < 0 || i > 2 || !links[i] ? none() : links[i]; }
  // Mech3.exe 1.2 @ 0x004ae980 (vt+0x0c): the slot holding `to` (the last one first), -1 for the null node or none.
  int32_t link_index(const Net105Node* to) const {
    if (to == none()) return -1;
    for (int32_t i = 2; i >= 0; --i)
      if (links[i] == to) return i;
    return -1;
  }
  const float* pos() const { return place; }  // vt+0x18 (0x00409fc0, +4)
  // Mech3.exe 1.2 @ 0x004ae930 (vt+0x2c)
  bool set_pos(const float p[3]) {
    place[0] = p[0], place[1] = p[1], place[2] = p[2];
    return true;
  }
  // The link methods (0x00403cd0..0x00403e70; mech3/zainet105.cpp), each nothing on the null node. A link's record
  // is measured by the basic controls' 0x004023d0 (`net105_link_fill`), here and by the reader's 0x00402300.
  bool add_link(Net105Node* other);                // vt+0x30 0x00403cd0: this's first free slot i, other's link_pair
  void remeasure_all();                            // vt+0x38 0x00403db0: each linked node remeasures, last first
  void remeasure_to(Net105Node* to);               // vt+0x3c 0x00403df0: the slot holding `to`, remeasure(this, it)
  bool link_pair(Net105Node* from, int32_t i);     // vt+0x40 0x00403d10: this's first free slot j: set_link both ways
  void set_link(Net105Node* to, int32_t i);        // vt+0x44 0x00403d60: slot i, a fresh record, measured
  bool remeasure(Net105Node* to, int32_t i);       // vt+0x50 0x00403e20: the slot holding `to` and `to`'s slot i
  void measure(Net105Node* to, int32_t i);         // vt+0x54 0x00403e70: record i zeroed, filled to `to` at width 0

  float place[3] = {};                      // +0x04
  Net105Node* links[3] = {};                // +0x10  (the file's numbers, -1 none, until 0x00402300)
  Net105Link* link_rec[3] = {};             // +0x1c
  int32_t x28 = 0;                          // +0x28  the file's node_NN first value
  int32_t number = -1;                      // +0x2c  node_NN's NN; -1 for one made at run time
  Net105Node* next = nullptr;               // +0x30  the record's node list
};

// The net (0x5c bytes, vtable 0x005bb8d0, 0x004021c0 over AiNet(-1)): the nodes by number in the base's elements
// (+0x0c..+0x14, null where a number is missing), the reader's caller (+0x18), the file's name and its settings.
class Net105 : public AiNet {
 public:
  Net105() : AiNet(-1) {}  // 0x004021c0 (0x00402190, 0x004d5120(-1))
  ~Net105() override;      // 0x004024e0: each node, then the net

  int32_t x18 = 0;              // +0x18  the reader's ECX (the caller's word; dev 2's controls read it)
  char name_[20] = {};          // +0x1c  "name"
  int32_t kind = 0;             // +0x30  "type": "ST" 0, "HI" 1, "FI" 2 (by the first two letters, upper-cased)
  float path_width = 10.0f;     // +0x34  "path_width" (10 without one)
  float activate_rad = 0;       // +0x38  "activate_rad"
  float attack_rad = 0;         // +0x3c  "attack_rad"
  float attack_dwell = 0;       // +0x40  "attack_dwell"
  float not_pursuit_dwell = 0;  // +0x44  "not_pursuit_dwell"
  float pursuit_far = 0;        // +0x48  "pursuit_range": the larger of its two
  float pursuit_near = 0;       // +0x4c  and the smaller
  float return_range = 0;       // +0x50  "return_range"
  Net105Node* first = nullptr;  // +0x54  the nodes in file order, by +0x30
  Net105* next = nullptr;       // +0x58  the records' list

  // Nodes made at run time (the basic controls' 0x00401ab0: new 0x34, 0x004ae8e0, its place, link 0, number -1):
  // the port owns them here; the exe leaks them.
  Net105Node* make_node();
  std::vector<std::unique_ptr<Net105Node>> made;
};

// [0x005f4c4c] (the head) and [0x005f4c50] (the tail): every Net105 made, in order, by +0x58.
Net105* nets105();
void nets105_append(Net105* n);  // 0x004021c0's link onto the tail
void nets105_free();             // 0x00402560's walk: each record (0x004024e0), the list emptied

// Mech3.exe 1.2 @ 0x004025b0 (through 0x00402590): a version-105 net file's block read into a new Net105 on the list:
// its settings, then node_00..node_98, then each link resolved and measured (0x00402300). `x18` is the caller's
// ECX, `default_name` the name without a "name" key.
Net105* net105_read(zrdr::Node* block, const char* default_name, int32_t x18);

// The basic controls' 0x004023d0 (ECX the record, EDX `from`, then `to` and the width): the record measured from one
// place to another. Theirs to set (dev 2's part); none leaves the record zeroed.
extern void (*net105_link_fill)(Net105Link& rec, const float from[3], const float to[3], float width);

}  // namespace mech3::ai
