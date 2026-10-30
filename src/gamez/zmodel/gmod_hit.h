#pragma once
#include <cstdint>

#include "gamez/zclass/cls_di.h"
#include "gamez/zmodel/gmod_const.h"

// A model's polygons against the world queries' probes and segments
// (zclass/cls_di.c calls these per node). Original file unknown: the code
// sits between zModel\gmod_puff.cpp and zReader\zreader.cpp; named gmod_hit
// here.
namespace zimg { struct TextureEntry; }

namespace zmod {

// ECX node, EDX its model; the probes (x, y, z each), their mask (0 skips a
// probe), count, top and the out records. The model's vertices go through
// the top of the matrix stack (morphed first when the node set a morph);
// each polygon facing up (n.y > 0) that holds a probe in x and z, at a
// height at most top, is added to the probe's record. The polygon that held
// every probe is cached (header +0x58) and tried first next time; a hit
// there ends the search.
void model_ground(zcls::Node* node, Model* model, const float* probes, const int32_t* mask, int32_t count,
                  float top, zcls::GroundHits* out);  // 0x0054c5f0

// One polygon (its vertices in world space, `poly` for the flags, zone and
// material): 1 when every masked probe is inside it and under top.
int32_t polygon_ground(zcls::Node* node, zcls::GroundHits* out, const float* probes, const int32_t* mask,
                       int32_t count, float top, const float* verts, const Polygon& poly);  // 0x0054c330

// ECX the model, EDX the record's next slot; the point (x, y, z). The
// first polygon (the cached one first) that holds the point in x and z, at
// a height at most its y, gives the slot its normal, height, zone and
// material and becomes the cached one: 1. Facing down counts too; a slot a
// polygon held above y keeps that polygon's normal and height.
int32_t model_ground_point(Model* model, zcls::GroundHit* slot, const float point[3]);  // 0x0054be40

// One polygon of n vertices (world space) at (x, z): 1 when inside, with
// the slot's normal and height (the plane at x, z; a vertical polygon's
// first vertex's).
int32_t polygon_ground_point(zcls::GroundHit* slot, const float* verts, float x, float z, int32_t n);  // 0x0054d640

// The shot queries' model test (cls_di.c's segment list, 0x004eed90): ECX
// the model, EDX the record's next slot, the segment a-b. The segment goes
// into the model's space (the inverse of the stack's top, 0x00523c50; a
// singular top finds nothing), the cached polygon is tried first, then the
// others; the first crossed gives the slot its normal, point (back in world
// space) and material. Polygons whose material has flag 2 are tested with
// their texture coordinates.
int32_t model_segment(Model* model, zcls::LineHit* slot, const float a[3], const float b[3]);  // 0x0054cad0

// One polygon of n vertices against the segment a-b, crossed from its front
// (either side when two_sided): 1 with the slot's unit normal and point.
int32_t polygon_segment(zcls::LineHit* slot, const float a[3], const float b[3], const float* verts, int32_t n,
                        int32_t two_sided);  // 0x0054d760
// The same with the polygon's texture coordinates (3 pairs) at the point,
// into uv_out and the capture (0x005426e0), whether or not it is on.
int32_t polygon_segment_uv(zcls::LineHit* slot, const float a[3], const float b[3], const float* verts,
                           const float* uvs, float uv_out[2], int32_t n, int32_t two_sided);  // 0x0054dd30
// The six faces of a box (its eight corners as node_box_corners gives
// them), one-sided; the slot's material is 0.
int32_t box_segment(const float corners[8][3], zcls::LineHit* slot, const float a[3], const float b[3]);  // 0x0054d2f0

// The surface marks' state (0x005426a0..0x00542710, the gmod_const region;
// the file is unknown): the texture coordinates of the last crossing of a
// textured polygon, kept while `on` is set, and the mark images a weapon's
// impact type picks. Only the script command WriteTextureSetMap turns it on,
// and no shipped script uses it, so no mark is ever painted
// (docs/weapon-effects.md).
struct UvCapture {
  int32_t on = 0;                // 0x0074a2ac
  int32_t type = 0;              // 0x0074a2b0  the impact type (a weapon's IMPACT_TYPE)
  zimg::TextureEntry* maps[3] = {};  // 0x0074a2b4  WriteTextureSetMap's images by type
  float uv[2] = {};              // 0x0074a2c0, 0x0074a2c4 (0x005426e0)
};
UvCapture& uv_capture();
void mark_set_type(int32_t type);              // 0x005426a0
void mark_set_map(zimg::TextureEntry* image);  // 0x005426b0: the type's image, and on
int32_t mark_on();                             // 0x005426d0
void mark_set_uv(float u, float v);            // 0x005426e0
void mark_enable(int32_t on);                  // 0x00542700, never called
bool mark_is_map(const zimg::TextureEntry* e); // 0x00542710: one of the three (kept as an image when loaded)
// 0x00542730 (ECX `target`, EDX `from`, stack `cookie`): the cookie's alpha
// stamps `from` into the target entry's image, centred at the captured UV
// (wrapped into -0.01..1.01 and written back), kept inside the image. Mask
// values blend (565: over 3, 555: over 7), from 0xfc copy. The two images
// must match in size and be unpalettered; false (reported) otherwise.
// docs/damage-decals.md.
bool cookie(zimg::TextureEntry* target, const zimg::Texture* from, const zimg::Texture* cookie);

// ECX node, EDX its model; the segments (from, to each), their mask, count
// and records. Each polygon a segment crosses from its front (either side
// with flag 0x100) gets the crossing, when the point lies inside the polygon;
// the polygon all of them crossed is cached as in model_ground.
void model_segments(zcls::Node* node, Model* model, const float* segs, const int32_t* mask, int32_t count,
                    zcls::SegmentHits* out);  // 0x0054f450

// One polygon against the segments; *all is set when every segment crossed
// it. Returns whether any did. The textured form (0x0054ea30) also clears
// *all for a segment that stays on one side, and records texture
// coordinates while uv_capture().on.
int32_t polygon_segments(zcls::Node* node, zcls::SegmentHits* out, const float* segs, const int32_t* mask,
                         int32_t count, const float* verts, const Polygon& poly, int32_t* all);  // 0x0054e2b0
void polygon_segments_uv(zcls::Node* node, zcls::SegmentHits* out, const float* segs, const int32_t* mask,
                         int32_t count, const float* verts, const float* uvs, float uv_out[2], const Polygon& poly,
                         int32_t* all);  // 0x0054ea30

// The six faces of a box (its eight corners as node_box_corners gives them)
// as untextured, one-sided quads; 1 as soon as a face takes every segment,
// else whether any face was crossed.
int32_t box_segments(zcls::Node* node, zcls::SegmentHits* out, const float* segs, const int32_t* mask,
                     int32_t count, const float corners[8][3]);  // 0x0054fa10

}  // namespace zmod
