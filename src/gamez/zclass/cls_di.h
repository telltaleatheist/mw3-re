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
// 0x004eda10: the ground point query kept to one hit, the highest at or below
// `at` (count 1), else none (count 0).
int ground_below(Node* world, const float at[3], GroundHits* out);
void ground_keep_below(GroundHits* out, float y);  // its choice among the hits

// One crossing of the shot queries' segment, or a node the sphere query
// found (0x28 bytes): the surface's normal, the point, the squared distance
// the nearest-hit query and the sphere query write, the material and node.
struct LineHit {
  float normal[3];               // +0x00
  float point[3];                // +0x0c
  uint32_t x18;                  // +0x18  not written
  float dist2;                   // +0x1c
  zmod::Material* material;      // +0x20  0 for a box face
  Node* node;                    // +0x24
};
struct LineHits {                // the exe's 0x504-byte buffer
  uint8_t count;                 // +0  (after 0x004eece0: the nearest one's index)
  uint8_t total;                 // +1  (after 0x004eece0: the count)
  LineHit hit[32];               // +4
};

// What a segment query's caller sets: stop at the first crossing of
// segment 0, and node flags that exclude a node (cleared by every query).
struct SegmentOptions {
  int32_t first_hit = 0;         // 0x0060b968  (0x004ed9c0)
  uint32_t exclude = 0;          // 0x0060b974  (0x004eda00)
  // The shot queries' own (segment_list): only nodes that have not moved
  // since this frame (node +0xc4 at most frames().frame; 0x004ed9d0), and
  // models only on nodes stamped after `since` (-1 every one; 0x004ed9e0).
  int32_t static_only = 0;       // 0x0060b96c
  int32_t since = 0;             // 0x0060b970
};
SegmentOptions& segment_options();

// ECX world node, EDX segments (from, to: six floats each), stack the count
// of points (two per segment, at most 24) and the records, one per segment.
// Nodes need ACTIVE and INTERSECT (0x10); with INTERSECT_BBOX (0x20) their
// box is the whole shape. A segment outside the world's bounds is dropped
// unless the world wraps; off the grid, the segments are moved into the
// stored cell for its nodes and their crossings moved back after.
void segment_query(Node* world, float* segs, int32_t points, SegmentHits* out);  // 0x004efcd0

// ECX world node, EDX the record (its count reset), stack the segment
// from-to: every crossing, in the walk's order (the cells along the segment,
// then the world's children), one per node (its model's first polygon
// crossed, or with INTERSECT_BBOX its box). Nodes need ACTIVE and INTERSECT
// and none of the exclude flags (cleared after). Returns 1 when none.
int segment_list(Node* world, const float from[3], const float to[3], LineHits* out);  // 0x004eed90
// The same, then the nearest crossing's index in count and the count in
// total; with two or more, each crossing's squared distance from `from`.
int segment_nearest(Node* world, const float from[3], const float to[3], LineHits* out);  // 0x004eece0
// ECX world node, EDX the centre: the nodes with flag 0x40 and a box in the
// cells under the sphere's square (and the world's children), whose names
// start with `prefix` when it is set, as their bounding sphere's centre;
// `ranged` drops those farther than radius (the distance past their own
// radius, squared, in dist2); `sighted` drops those a segment_list from the
// centre finds blocked (nodes with flag 0x400000 only). Returns 1 when none.
int sphere_query(Node* world, const float centre[3], float radius, const char* prefix, int32_t ranged,
                 int32_t sighted, LineHits* out);  // 0x004f0ac0

}  // namespace zcls
