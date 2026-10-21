#pragma once
#include <cstdint>
#include <vector>

#include "gamez/zgeometry/zgeo_weiler.h"

// zGeometry's convexify (zGeometry\zgeo_convexify.cpp, 0x005158c0..0x00516e90;
// docs/craters.md, section 5): the clipped (concave) ground pieces split
// into triangles and convex quads, and the ground ring around a crater
// that fell inside one ground polygon cut into triangles.
namespace zgeo {

// What convexify leaves (0x10 bytes; 0x00516290 frees it): the polygons,
// each a point count and its first float, and their points.
struct Convex {
  std::vector<ResultContour> polys;  // +0x00 count, +0x04
  std::vector<float> points;         // +0x08 count (points), +0x0c
};

// Each of the list's polygons (3 or more points, the others skipped) as
// convex pieces: triangles kept, a convex quad kept whole, a concave quad
// split at its first reflex corner, larger ones triangulated (0x005166c0).
// `total` is the list's point count (room for total - 2 triangles). 0 for
// no input.
int convexify(const ResultContour* list, int32_t count, int32_t total, const float* points, Convex& out);  // 0x005162d0 ECX list, EDX total

// The triangles of a simple polygon of n vertices, each vertex `stride`
// ints (2 when flag is 1, else 3) whose first two index its x and y in f
// (null idx: vertex k is 3k, 3k + 1, 3k + 2). [0] is the count (n - 2),
// then the triangles' vertex records; empty on failure.
std::vector<int32_t> triangulate(const float* f, const int32_t* idx, int32_t n, int32_t flag);  // 0x005166c0 ECX n, EDX f
// The polygon cut in two along a diagonal from its leftmost vertex (to the
// nearest vertex inside the corner there, else across the corner):
// out = n1, n2, then the two vertex lists.
int split_polygon(const int32_t* v, int32_t n, int32_t stride, const float* f, std::vector<int32_t>& out);  // 0x00516a50 ECX n, EDX f

// The plane through n points by Newell's method (normal x, y, z and d),
// the normal scaled by the exe's bit-trick square root.
void plane_of(const float* points, int32_t n, float plane[4]);  // 0x00515f10 ECX n, EDX points
// Each point's z set on the plane.
void plane_z(const float plane[4], float* points, int32_t n);   // 0x005160e0 ECX n, EDX points
// The n points reversed after the first.
void reverse_points(float* points, int32_t n);                   // 0x00516120 ECX n, EDX points
// 1 when the first three points turn left (counterclockwise from above);
// else, with `turn`, the points reversed and 1, without it 0.
int face_up(float* points, int32_t n, int turn);                 // 0x00516190 ECX n, EDX points

// The ring between a polygon and a region inside it in triangles (9
// floats each). The region's z is first set on the polygon's plane (in the
// caller's buffer too).
struct RingTriangles {
  int32_t count = 0;
  std::vector<float> points;
};
RingTriangles ring_triangles(int32_t np, const float* poly, int32_t nr, float* region);  // 0x00515be0 ECX np, EDX poly

}  // namespace zgeo
