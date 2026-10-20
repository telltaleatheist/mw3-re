#pragma once
#include <cstdint>

#include "gamez/zclass/cls_di.h"
#include "gamez/zmodel/gmod_const.h"

// A model's polygons against the world queries' probes and segments
// (zclass/cls_di.c calls these per node). Original file unknown: the code
// sits between zModel\gmod_puff.cpp and zReader\zreader.cpp; named gmod_hit
// here.
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

// The texture coordinates of the last crossing of a textured polygon, kept
// while `on` is set (0x0074a2ac, set by 0x00542700; read with 0x005426d0).
struct UvCapture {
  int32_t on = 0;                // 0x0074a2ac
  float uv[2] = {};              // 0x0074a2c0, 0x0074a2c4 (0x005426e0)
};
UvCapture& uv_capture();

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
