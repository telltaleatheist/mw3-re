#pragma once
#include <cstdint>

#include "gamez/znetwork/znet_object.h"
#include "mech3/zainet_node.h"

// The AI's nets at run time (docs/ai.md, The net objects): the net base every
// kind derives from, and the node a net file's record becomes. Original files
// unknown: the base sits between zaitarget.cpp and zrdrainet.cpp
// (0x004d5120..0x004d5440), the node between zainetmechinout.cpp and
// zaitarget.cpp (0x004c6d30..0x004c77b0).
namespace mech3::ai {

class CZAiNetEdit;
float normalize_load(float v[3]);  // 0x004020d0 at load-time precision
class NetNode;
class Region;
struct Behaviour;

// An element of an AI net: the nodes of the builder, the edges of a region,
// the steps of a compiled net. In the exe two casts (+0x60, then +0x88 or
// +0x74) take an element to the node or the behaviour it is; anything else
// answers null.
struct AiElem : znet::Element {
  virtual NetNode* as_node() { return nullptr; }
  virtual Behaviour* as_behaviour() { return nullptr; }  // +0x60 then +0x74
};

// 0x004d5120: a numbered object (vtable 0x005ccda8) with the casts the AI
// asks every net for. Only the derived classes say yes to theirs.
class AiNet : public znet::Object {
 public:
  explicit AiNet(int32_t id) : znet::Object(id) {}  // 0x004d5120
  ~AiNet() override = default;                       // 0x004d5160

  AiElem* at(int32_t i) const { return static_cast<AiElem*>(elem(i)); }
  virtual AiNet* as_net() { return this; }       // +0x28 0x004d2a70
  virtual AiNet* as_compiled() { return nullptr; }  // +0x34: a net_NN, compiled (0x004bf910)
  virtual Region* as_region() { return nullptr; }   // +0x44: a reg_* loop (0x004c5b00)
  virtual const char* name();                       // +0x4c 0x004d52a0: "net_%02d" of the id
  void link_pass();                                 // +0x30 0x004d53f0
};

// One node of a net file at run time (0xbc bytes, vtables 0x005cae60 /
// 0x005cae58, ctor 0x004c6d30): the file's record, copied, and its net. The
// net's placeholder for slots without a record is one too (0x004c6da0 with
// the default record, vtables 0x005ca388 / 0x005ca380), whose link and
// position setters do nothing.
class NetNode : public AiElem {
 public:
  NetNode(CZAiNetEdit* owner, const NetNodeRec& rec);  // 0x004c6d30
  explicit NetNode(CZAiNetEdit* owner);                // 0x004c6da0: the placeholder
  ~NetNode() override = default;                       // 0x004c6e10

  NetNode* as_node() override { return this; }
  int32_t link_count() const { return int32_t(rec.links.size()); }  // +0x04 0x004c7240
  NetNode* link_node(int32_t i) const;                 // +0x08 0x004c71f0
  float* pos() { return rec.pos; }                     // +0x18 0x004c9170
  bool set_pos(const float p[3]);                      // +0x2c 0x004c71a0
  void relink();                                       // +0x38 0x00410590
  void link_from(NetNode* src, int32_t i);             // +0x50 0x004c7170
  bool set_link(NetNode* target, int32_t i);           // +0x54 0x004c6fc0
  void arrange();                                      // +0x8c 0x004c74a0

  NetNodeRec rec;                // +0x04
  CZAiNetEdit* owner = nullptr;  // +0xb8
  bool placeholder = false;      // the 0x005ca388 vtable
};

}  // namespace mech3::ai
