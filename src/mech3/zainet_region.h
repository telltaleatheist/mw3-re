#pragma once
#include <cstdint>
#include <list>

#include "mech3/zainet.h"

// The AI's regions (docs/ai.md, Regions): a reg_*.zrd's closed loop of nodes
// made into the edges of an area, which the vehicles' orders test positions
// against. Original file unknown: the code sits between zainetmechinout.cpp
// and zaitarget.cpp (0x004c5640..0x004c5fa0).
namespace mech3::ai {

// One side of the loop (0x1c bytes, vtable 0x005cad70, ctor 0x004c5960): its
// start and the normal of its vertical plane, (dz, 0, -dx) of the side,
// normalized; (0, 1, 0) for a side of no length on the ground.
struct RegionEdge : AiElem {
  RegionEdge(const float* a, const float* b);  // 0x004c5960
  ~RegionEdge() override = default;            // 0x004d08f0
  bool outside(const float* p) const;          // 0x004c5a30: behind the plane

  float point[3] = {};   // +0x04
  float normal[3] = {};  // +0x10
};

// 0x004c5640 (0x1c bytes, vtable 0x005cac90): a net of edges, with where the
// last test stopped (+0x18).
class EdgeNet : public AiNet {
 public:
  EdgeNet(int32_t id, int32_t reserve);  // 0x004c5640
  ~EdgeNet() override;                   // 0x004c5750: the edges freed
  RegionEdge* edge(int32_t i) const { return static_cast<RegionEdge*>(elems[size_t(i)]); }
  // 0x004c57d0: on the inner side of every edge. The test starts at the edge
  // that failed last time and goes round; a pass sends it back to the first.
  bool inside(const float* p);

  int32_t cursor = 0;  // +0x18  an index here; a pointer into the edges in the exe
};

// 0x004c5b00 (0x20 bytes, vtable 0x005cae00).
class Region : public EdgeNet {
 public:
  Region(const char* name, int32_t id, int32_t reserve);  // 0x004c5b00
  ~Region() override;                                     // 0x004c5ba0
  Region* as_region() override { return this; }           // +0x44 0x004d2a70
  const char* name() override { return region_name; }     // +0x1c 0x0047a920 (the lists' name)

  char* region_name = nullptr;  // +0x1c  strdup'd
};

// The regions loaded, a std::list at 0x0060b3f0 (head 0x0060b3f4, size
// 0x0060b3f8), in load order. They outlive znet::clear (ids from 10000).
std::list<Region*>& regions();
Region* region_named(const char* name);  // 0x004c5d20 -> 0x004c5e50
// 0x004c5c00: the region of that name, loaded from its file on first use
// (net id from 10000); null when the file is not one closed loop of a
// version-108 region whose own name is the one asked for.
Region* region_find(const char* name);
void regions_clear();                    // 0x004c5fa0 -> 0x004c5f00: freed, at mission end

}  // namespace mech3::ai
