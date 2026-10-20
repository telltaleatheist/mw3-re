#pragma once
#include <cstdint>

#include "gamez/zclass/cls_node.h"

namespace zmod { struct Material; }

// zClass\cls_di.c: the world queries the mechs move by (docs/movement-ground.md,
// The terrain query; docs/movement-collision.md). Not rays: the ground query
// is a flat point-in-polygon test per probe, the segment query crosses each
// segment with the polygons' planes.
namespace zcls {

// One polygon under a probe (0x28 bytes in the exe). The query writes the
// normal, height, zone set's first word, material and node; x, z and the
// zone's second word are left as they were (the skis fill x and z).
struct GroundHit {
  float normal[3];               // +0x00
  float x;                       // +0x0c
  float height;                  // +0x10
  float z;                       // +0x14
  uint32_t zone[2];              // +0x18  the polygon's +0x20 in [0]
  zmod::Material* material;      // +0x20
  Node* node;                    // +0x24
};
struct GroundHits {              // 0x504 bytes in the exe
  uint8_t count;
  GroundHit hit[32];
};

// One crossing of a segment (the same 0x28-byte slot): the polygon's normal
// and the point where the segment meets its plane.
struct SegmentHit {
  float normal[3];               // +0x00
  float point[3];                // +0x0c
  uint32_t zone[2];              // +0x18  not written
  zmod::Material* material;      // +0x20  0 for a box face
  Node* node;                    // +0x24
};
struct SegmentHits {
  uint8_t count;
  SegmentHit hit[32];
};

// ECX world node, EDX probes (x, y, z each), stack count, top, out (one
// record per probe, its count reset). A probe off the grid on a world with
// wrapping (+0x50) is moved into the stored cell while the cell's nodes are
// tested, then moved back, which can leave it an ulp off. Always 0.
int ground_query(Node* world, float* probes, int32_t count, float top, GroundHits* out);  // 0x004ee2f0

// ECX world node, EDX the record (its count reset), stack the point: the
// polygons under one point, at most its y, one per node, in the walk's
// order (a node's model before its children). The world's zone mode
// applies; a point off the grid on a world without wrapping tests only the
// world's own children. Returns 1 when none was found.
int ground_point(Node* world, float x, float y, float z, GroundHits* out);  // 0x004edab0

// What a segment query's caller sets: stop at the first crossing of
// segment 0, and node flags that exclude a node (cleared by every query).
struct SegmentOptions {
  int32_t first_hit = 0;         // 0x0060b968
  uint32_t exclude = 0;          // 0x0060b974
};
SegmentOptions& segment_options();

// ECX world node, EDX segments (from, to: six floats each), stack the count
// of points (two per segment, at most 24) and the records, one per segment.
// Nodes need ACTIVE and INTERSECT (0x10); with INTERSECT_BBOX (0x20) their
// box is the whole shape. A segment outside the world's bounds is dropped
// unless the world wraps; off the grid, the segments are moved into the
// stored cell for its nodes and their crossings moved back after.
void segment_query(Node* world, float* segs, int32_t points, SegmentHits* out);  // 0x004efcd0

}  // namespace zcls
