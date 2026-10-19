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

// ECX world node, EDX probes (x, y, z each), stack count, top, out (one
// record per probe, its count reset). A probe off the grid on a world with
// wrapping (+0x50) is moved into the stored cell while the cell's nodes are
// tested, then moved back, which can leave it an ulp off. Always 0.
int ground_query(Node* world, float* probes, int32_t count, float top, GroundHits* out);  // 0x004ee2f0

}  // namespace zcls
