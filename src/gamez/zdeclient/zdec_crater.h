#pragma once
#include <cstdint>
#include <deque>
#include <set>
#include <vector>

#include "gamez/zdeclient/zdec_init.h"
#include "gamez/zweapon/zweapon.h"

namespace zcls { struct Node; struct Partition; }
namespace zgeo { struct ClipSet; }

// zDEClient's craters (zdec_crater.cpp; docs/craters.md, section 2): a ring
// on the ground where a shot lands, the ground under it clipped away and a
// sloped ring and floor put in.
namespace zdec {

// A crater being made (0x50 bytes, 0x004ffe80; freed by 0x004ff8f0).
struct CraterRecord {
  int32_t type = 1;                  // +0x00  1 crater, 3 quicksand
  zwep::CraterOrder order;           // +0x04..+0x28  (+0x08 the ring's point count)
  std::vector<float> ring;           // +0x2c
  float box[4] = {};                 // +0x30 min x, +0x34 min z, +0x38 max x, +0x3c max z
  zcls::Partition* cell = nullptr;   // +0x40
  zgeo::ClipSet* set = nullptr;      // +0x44
  CraterTexture* texture = nullptr;  // +0x4c  the ground's crater material and dust
};

// What a crater node (named ZDEC_FEATURE) keeps of its record for the next
// crater's overlap test (0x004ffad0): its type and box.
struct Feature {
  int32_t type = 0;
  float box[4] = {};
};

// One crater or quicksand pit as made (0x34 bytes): type and order.
struct HistoryEntry {
  int32_t type = 0;
  uint32_t words[12] = {};
};

struct CraterState {
  int (*veto)(const zwep::CraterOrder* order) = nullptr;  // 0x0060bf88 (multiplayer's 0x0046fd30)
  std::vector<HistoryEntry> history;                      // 0x0060bf94..0x0060bf9c
  std::set<zcls::Node*> touched;                          // 0x0060bfa0  nodes whose models craters replaced
  std::deque<Feature> features;                           // the port's: the crater nodes' records (FIX, see .cpp)
};
CraterState& craters();

// What a crater calls outside zDEClient and zGeometry.
struct CraterHooks {
  // 0x00508aa0: an animation at a point (the dust), ECX the def, EDX 0.
  void (*play_anim)(zeff::AnimDef* anim, const float at[3]) = nullptr;
};
CraterHooks& crater_hooks();

zcls::Node* world_node();                                   // 0x005020b0
zcls::Partition* cell_at(int32_t x, int32_t z);             // 0x00502090 ECX x, EDX z

// The order's crater, unless the veto says no (then -1).
int32_t crater_make(const zwep::CraterOrder* order);        // 0x004ffaa0 ECX order
// 0 made, -1 not (no cell, the cell full, near another crater, a no-crater
// ground node in the way, or the clip or tessellation failed). With `dust`
// the ground's dust animation plays at the crater.
int32_t crater_instance(const zwep::CraterOrder* order, int dust);  // 0x004ff940 ECX order, EDX dust

CraterRecord* record_new(const zwep::CraterOrder* order);   // 0x004ffe80 ECX order
void record_free(CraterRecord* r);                          // 0x004ff8f0 ECX r
CraterRecord* crater_build(const zwep::CraterOrder* order); // 0x004ffad0 ECX order
int32_t crater_clip(CraterRecord* r);                       // 0x004fff20 ECX r: nodes changed, 0 none or failed
int32_t crater_tessellate(CraterRecord* r);                 // 0x004fff80 ECX r: 0, or -1
// The order as the exe's dwords (pointers as their low 32 bits).
void order_words(const zwep::CraterOrder& o, uint32_t out[10]);
int32_t history_push(int32_t type, const uint32_t* words);   // 0x00500850 ECX type, EDX order
void note_touched(const zgeo::ClipSet& set);                // 0x00500600 ECX set

}  // namespace zdec
