#include "mech3/zainet_region.h"

#include <cstdlib>
#include <cstring>

#include "mech3/zainetedit.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3::ai {

// Mech3.exe 1.2 @ 0x004c5960 (no null ends from its one caller; the exe takes
// a zero vector for one). The side's length is the load's, at 53 bits.
RegionEdge::RegionEdge(const float* a, const float* b) {
  point[0] = a[0], point[1] = a[1], point[2] = a[2];
  float d[3] = {b[0] - a[0], 0.0f, b[2] - a[2]};
  if (normalize_load(d) == 0.0f) {
    normal[0] = 0.0f, normal[1] = 1.0f, normal[2] = 0.0f;  // 0x005eda70
    return;
  }
  normal[0] = d[2], normal[1] = 0.0f, normal[2] = -d[0];
}

// Mech3.exe 1.2 @ 0x004c5a30 (a per-frame test: floats, the exe's order)
bool RegionEdge::outside(const float* p) const {
  const float d[3] = {p[0] - point[0], p[1] - point[1], p[2] - point[2]};
  const float dot = (d[0] * normal[0] + d[1] * normal[1]) + d[2] * normal[2];
  return dot < 0.0f;
}

// Mech3.exe 1.2 @ 0x004c5640: room for `reserve` edges (none made yet).
EdgeNet::EdgeNet(int32_t id, int32_t reserve) : AiNet(id) {
  if (reserve > 0) elems.reserve(size_t(reserve));
}

// Mech3.exe 1.2 @ 0x004c5750 (each through 0x004c5740)
EdgeNet::~EdgeNet() {
  for (znet::Element*& e : elems) {
    delete e;
    e = nullptr;
  }
}

// Mech3.exe 1.2 @ 0x004c57d0
bool EdgeNet::inside(const float* p) {
  const int32_t end = count();
  int32_t at = cursor;
  bool in = true;
  if (at != end) {
    for (; at != end; ++at)
      if (edge(at)->outside(p)) break;
    in = at == end;
  }
  if (in) {
    int32_t i = 0;
    if (i != cursor) {
      for (;;) {
        const bool out = edge(i)->outside(p);
        at = i;
        if (out) break;
        at = ++i;
        if (i == cursor) break;
      }
      in = at == cursor;
    }
    if (in) {
      cursor = 0;
      return true;
    }
  }
  cursor = at;
  return in;
}

// Mech3.exe 1.2 @ 0x004c5b00. FIX (bug): a file without a name gave the exe a
// null to strdup (a crash); the region keeps none and region_find drops it.
Region::Region(const char* n, int32_t id, int32_t reserve) : EdgeNet(id, reserve) {
  region_name = n ? strdup(n) : nullptr;
}

// Mech3.exe 1.2 @ 0x004c5ba0
Region::~Region() { std::free(region_name); }

std::list<Region*>& regions() {
  static std::list<Region*> list;  // 0x0060b3f0 (ctor 0x004c5d80)
  return list;
}

// Mech3.exe 1.2 @ 0x004c5e50 (strcmp: case counts)
Region* region_named(const char* name) {
  for (Region* r : regions()) {
    const char* n = r->name();
    if (n && std::strcmp(name, n) == 0) return r;
  }
  return nullptr;
}

// Mech3.exe 1.2 @ 0x004c5c00. A region file is read by a short-lived builder
// whose net id is the first free one from 10000; the region keeps it.
Region* region_find(const char* name) {
  if (!name) return nullptr;
  if (Region* r = region_named(name)) return r;
  const int32_t id = znet::alloc_id(znet::kKeptIds, znet::kKeptIds, znet::kKeptIds + 100);
  CZAiNetEdit edit(id, name, 1);
  Region* r = edit.net ? edit.net->as_region() : nullptr;
  if (!r || !r->region_name || std::strcmp(r->region_name, name) != 0) {
    edit.drop_net();
    r = nullptr;
  }
  if (r) regions().push_back(r);
  return r;
}

// Mech3.exe 1.2 @ 0x004c5f00
void regions_clear() {
  for (Region*& r : regions()) {
    delete r;
    r = nullptr;
  }
  regions().clear();
}

}  // namespace mech3::ai
