// The shot queries (zclass/cls_di: segment_list 0x004eed90, segment_nearest
// 0x004eece0, sphere_query 0x004f0ac0) for tests/differential/test_shotq.py,
// on a world the port's loaders read (sw_load in probe_scenewalk.cpp).
// Records come back with their pointers as node and material indices.
#include <cstdint>
#include <cstring>

#include "gamez/zclass/camera.h"
#include "gamez/zclass/cls_di.h"
#include "gamez/zclass/cls_node.h"
#include "gamez/zclass/list.h"
#include "gamez/zmodel/gmod_hit.h"
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
  const int32_t i = zmod::materials().index_of(m);
  return i >= 0 ? i : -2;
}

zcls::Node* world_node() { return zcls::camera_world(zcls::list_head(zcls::kCameras)->node); }

zcls::LineHits g_rec, g_seen;  // kept between queries, as the test keeps the original's buffers

// count, total, then per slot: normal, point, +0x18, dist2 as bits, the
// material and node as indices (10 words).
void put(int32_t* o) {
  *o++ = g_rec.count;
  *o++ = g_rec.total;
  for (const zcls::LineHit& h : g_rec.hit) {
    std::memcpy(o, h.normal, 12), o += 3;
    std::memcpy(o, h.point, 12), o += 3;
    *o++ = static_cast<int32_t>(h.x18);
    std::memcpy(o++, &h.dist2, 4);
    *o++ = material_index(h.material);
    *o++ = node_index(h.node);
  }
}

void options(int32_t first_hit, uint32_t exclude, int32_t static_only, int32_t since, uint32_t frame) {
  zcls::SegmentOptions& o = zcls::segment_options();
  o.first_hit = first_hit, o.exclude = exclude, o.static_only = static_only, o.since = since;
  zcls::frames().frame = frame;
}

void finish(int32_t* opts_out, float* uv_out) {
  const zcls::SegmentOptions& o = zcls::segment_options();
  opts_out[0] = o.first_hit, opts_out[1] = static_cast<int32_t>(o.exclude);
  std::memcpy(uv_out, zmod::uv_capture().uv, 8);
}

}  // namespace

extern "C" {

void sq_reset() {
  std::memset(&g_rec, 0, sizeof g_rec);
  std::memset(&g_seen, 0, sizeof g_seen);
  zmod::uv_capture() = zmod::UvCapture{};
}

// Words of a node's raw image (+0xc4 the frame it moved, +0x24 its flags).
void sq_set_node_word(int32_t index, int32_t off, uint32_t v) { zcls::node_at(index)->put<uint32_t>(off, v); }
// A material's flags byte (+0x01; 2 sends its polygons to the textured test).
void sq_set_material_flags(int32_t index, uint8_t flags) { zmod::materials().at(index).flags = flags; }

// One query; kind 0 segment_list, 1 segment_nearest, 2 sphere_query (a[0..2]
// the centre, radius, prefix, ranged, sighted). out: the record as put();
// opts_out the first-hit and exclude options after; uv_out the capture.
int32_t sq_query(int32_t kind, const float* a, const float* b, float radius, const char* prefix, int32_t ranged,
                 int32_t sighted, int32_t first_hit, uint32_t exclude, int32_t static_only, int32_t since,
                 uint32_t frame, int32_t* out, int32_t* opts_out, float* uv_out) {
  options(first_hit, exclude, static_only, since, frame);
  int32_t r;
  if (kind == 0) r = zcls::segment_list(world_node(), a, b, &g_rec);
  else if (kind == 1) r = zcls::segment_nearest(world_node(), a, b, &g_rec);
  else r = zcls::sphere_query(world_node(), a, radius, prefix, ranged, sighted, &g_rec);
  put(out);
  finish(opts_out, uv_out);
  return r;
}

}  // extern "C"
