#pragma once
#include <cstdint>

#include "mech3/zainet.h"
#include "mech3/zrdrainet.h"

// mech3\ZAiNetEdit.cpp, its run-time side (docs/ai.md, The net objects): a
// net file read into nodes, then built into the net the AI uses. In play it
// is always a short-lived builder (made with `build` set): the net it builds
// lives on in the registry, the builder is dropped. The editing side (the
// debug menu's path editor, `build` clear: edits written back through the
// record cache 0x004dc2b0) is not ported; decided with Owen, later behind
// --debug (ai.md, The developer tools).
namespace mech3::ai {

// Calls compiling a net makes into modules not ported, or ported elsewhere.
struct NetHooks {
  // 0x004edab0 / 0x004abd90 while a world is loaded ([0x00608c84]): a node's
  // height put on the ground under it. None: the file's height.
  void (*ground)(float pos[3]) = nullptr;
  // 0x004d5460, 0x00479be0, 0x0047a320, 0x0047a010 (mech_waynet.cpp): an
  // arrangenodes node also becomes a point of the multiplayer "mcbwaynet".
  // None: not ported.
  void (*waynet_point)(const NetNodeRec& rec, const float pos[3]) = nullptr;
  // 0x004be5a0 over the vehicles (0x0073655c): the ones following a net
  // that is rebuilt are let go of it, then given the new one.
  void (*rebuilt)(AiNet* old_net, AiNet* new_net) = nullptr;
};
NetHooks& net_hooks();

// 0x00402240: net_NN built (a short-lived builder), unless one of that
// number is registered; the compiled net, null for none.
AiNet* net_build(int32_t net);
void nets_build_all();  // 0x00402170: nets 1..99, as a mission starts (0x0049e810)

// 0x004bf000's answer: a region's loop of nodes.
struct NetLoop {
  int32_t count = 0;  // edges
  int32_t first = 0;  // the node it starts and ends at
};

// 0x44 bytes, vtable 0x005ca420 (+0x005ca388 for its placeholder node).
class CZAiNetEdit : public AiNet {
 public:
  // 0x004bcba0: net `net` (named `name`, net_NN when null) read into nodes;
  // with `build`, the net built from them (+0x34) for its file's version.
  CZAiNetEdit(int32_t net, const char* name, int32_t build);
  ~CZAiNetEdit() override;  // 0x004bd020: the built net is not freed

  NetNode* node(int32_t i) const;           // +0x0c 0x004bef40: the placeholder when out of range
  const char* name() override { return header.name; }  // +0x4c 0x004bf4d0
  AiNet* as_compiled() override { return net ? net->as_compiled() : nullptr; }  // +0x34 0x004bcf80
  Region* as_region() override { return net ? net->as_region() : nullptr; }     // +0x44 0x004bcfc0
  void drop_net();                          // 0x004bf4b0

  bool loop(NetLoop* out);                  // 0x004bf000: the nodes form one closed loop
  Region* make_region(Region* r, const NetLoop& l);  // 0x004bf1b0: the loop's edges into `r`
  Region* build_region();                   // 0x004bf3f0 (version 108)
  AiNet* build_compiled();                  // 0x004be670 (version 106; zainet_compile.cpp)

  NetHeader header;               // +0x18
  AiNet* net = nullptr;           // +0x34  what was built
  uint8_t editing = 0;            // +0x38  !build: edits are saved
  uint8_t dirty = 0;              // +0x39
  NetRef* ref = nullptr;          // +0x3c
  NetNode* placeholder = nullptr; // +0x40
};

}  // namespace mech3::ai
