// The world queries (zclass/cls_di) for test_di.py, on a world the port's
// loaders read (sw_load in probe_scenewalk.cpp). Records come back with
// their pointers as node and material indices.
#include <cstdint>
#include <cstring>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/cls_di.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/list.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_matl.h"

namespace {

int32_t node_index(const zcls::Node* n) {
  if (!n) return -1;
  const zcls::NodePool& p = zcls::nodes();
  for (size_t b = 0; b < p.blocks.size(); ++b) {
    const zcls::Node* base = p.blocks[b].get();
    if (n >= base && n < base + p.block_size) return static_cast<int32_t>(b * p.block_size + (n - base));
  }
  return -2;
}

int32_t material_index(const zmod::Material* m) {
  if (!m) return -1;
  const auto& s = zmod::materials().slots;
  if (m >= s.data() && m < s.data() + s.size()) return static_cast<int32_t>(m - s.data());
  return -2;
}

zcls::Node* world_node() { return zcls::camera_world(zcls::list_head(zcls::kCameras)->node); }

zcls::GroundHits g_ground[24];

}  // namespace

extern "C" {

// Clears the persistent record buffer, as the test clears the original's.
void di_reset() { std::memset(g_ground, 0, sizeof g_ground); }

// The world's partition mode (+0x50): wrapped cells off the grid.
void di_wrap(int32_t on) { static_cast<zcls::World*>(world_node()->data)->put<int32_t>(0x50, on); }

// One ground query; probes in and out (x, y, z), then per record: count,
// and for each of its 32 slots normal, x, height, z, zone[2] as bits, the
// material and node as indices (10 words).
int32_t di_ground(float* probes, int32_t count, float top, int32_t* out) {
  const int r = zcls::ground_query(world_node(), probes, count, top, g_ground);
  int32_t* o = out;
  for (int32_t i = 0; i < count && i < 24; ++i) {
    const zcls::GroundHits& g = g_ground[i];
    *o++ = g.count;
    for (const zcls::GroundHit& h : g.hit) {
      std::memcpy(o, h.normal, 12), o += 3;
      std::memcpy(o++, &h.x, 4);
      std::memcpy(o++, &h.height, 4);
      std::memcpy(o++, &h.z, 4);
      *o++ = static_cast<int32_t>(h.zone[0]);
      *o++ = static_cast<int32_t>(h.zone[1]);
      *o++ = material_index(h.material);
      *o++ = node_index(h.node);
    }
  }
  return r;
}

// Sets a polygon's flags (the test turns some into strips).
void di_set_poly(int32_t model, int32_t poly, uint32_t flags) { zmod::models().slots[model].polygons[poly].flags = flags; }

// Every model's cached polygon (+0x58) and every live node's flags (+0x24).
int32_t di_state(int32_t* models, int32_t* flags, int32_t cap) {
  int32_t n = 0;
  for (const zmod::Model& m : zmod::models().slots) {
    if (n >= cap) break;
    std::memcpy(models + n++, m.header + 0x58, 4);
  }
  const zcls::NodePool& p = zcls::nodes();
  int32_t k = 0;
  for (size_t b = 0; b < p.blocks.size(); ++b)
    for (int32_t i = 0; i < p.block_size && k < cap; ++i) flags[k++] = static_cast<int32_t>(p.blocks[b][i].flags());
  return n;
}

}  // extern "C"
