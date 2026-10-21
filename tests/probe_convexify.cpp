// zGeometry's convexify (craters part 4) for tests/differential/test_convexify.py:
// convexify 0x005162d0 on a polygon list, and the ring triangles 0x00515be0
// with each triangle turned up by 0x00516190, as zgeo_model's clip uses them.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zgeometry/zgeo_convexify.h"

namespace {
std::vector<uint8_t> g_out;

void u32(uint32_t v) {
  for (int i = 0; i < 4; ++i) g_out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void floats(const float* p, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    uint32_t b;
    std::memcpy(&b, p + i, 4);
    u32(b);
  }
}
int finish(uint8_t* out, int cap) {
  const int n = static_cast<int>(g_out.size());
  if (n <= cap) std::memcpy(out, g_out.data(), g_out.size());
  return n;
}
}  // namespace

// list: count pairs (points, first float); total points.
extern "C" int probe_convexify(const int32_t* list, int count, int total, const float* points, uint8_t* out,
                               int cap) {
  g_out.clear();
  std::vector<zgeo::ResultContour> l(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) l[static_cast<size_t>(i)] = {list[2 * i], list[2 * i + 1]};
  zgeo::Convex c;
  u32(static_cast<uint32_t>(zgeo::convexify(l.data(), count, total, points, c)));
  u32(static_cast<uint32_t>(c.polys.size()));
  for (const zgeo::ResultContour& p : c.polys) u32(static_cast<uint32_t>(p.count)), u32(static_cast<uint32_t>(p.start));
  u32(static_cast<uint32_t>(c.points.size() / 3));
  floats(c.points.data(), c.points.size());
  return finish(out, cap);
}

// The ring triangles, the region after (its z set), then each triangle
// after face_up(3, 1).
extern "C" int probe_ring(int np, const float* poly, int nr, float* region, uint8_t* out, int cap) {
  g_out.clear();
  zgeo::RingTriangles r = zgeo::ring_triangles(np, poly, nr, region);
  u32(static_cast<uint32_t>(r.count));
  floats(r.points.data(), r.points.size());
  floats(region, 3 * static_cast<size_t>(nr));
  for (int32_t i = 0; i < r.count; ++i) {
    u32(static_cast<uint32_t>(zgeo::face_up(r.points.data() + 9 * i, 3, 1)));
    floats(r.points.data() + 9 * i, 9);
  }
  return finish(out, cap);
}

// The plane 0x00515f10 of n points (4 floats).
extern "C" void probe_plane(int n, const float* points, float* plane) { zgeo::plane_of(points, n, plane); }

// face_up 0x00516190 on n points with `turn`: the result and the points.
extern "C" int probe_face_up(float* points, int n, int turn, uint8_t* out, int cap) {
  g_out.clear();
  u32(static_cast<uint32_t>(zgeo::face_up(points, n, turn)));
  floats(points, 3 * static_cast<size_t>(n));
  return finish(out, cap);
}
