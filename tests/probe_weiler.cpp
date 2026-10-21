// Weiler–Atherton (craters part 3) for tests/differential/test_weiler.py:
// the state 0x0050f960 builds dumped by value (pointers as array and
// index), the driver's first steps run as 0x0050faf0 runs them, and the
// leaves on the test's inputs.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zerror/zerr.h"
#include "gamez/zgeometry/zgeo_weiler.h"

namespace {
std::vector<uint8_t> g_out;
const float* g_caller = nullptr;
int g_caller_n = 0;

void u32(uint32_t v) {
  for (int i = 0; i < 4; ++i) g_out.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void f32(float v) {
  uint32_t b;
  std::memcpy(&b, &v, 4);
  u32(b);
}
void floats(const float* p, size_t n) {
  for (size_t i = 0; i < n; ++i) f32(p[i]);
}
int finish(uint8_t* out, int cap) {
  const int n = static_cast<int>(g_out.size());
  if (n <= cap) std::memcpy(out, g_out.data(), g_out.size());
  return n;
}
void sink(int level, const char*, int line, const char*) { u32(0x70), u32(static_cast<uint32_t>(level)), u32(static_cast<uint32_t>(line)); }

// A point pointer as (array, index): 1 the caller's points, 2 the region
// copy, 3 the polygon.
void point_ref(const zgeo::Weiler& w, const float* p) {
  auto in = [&](const float* base, int n) { return base && p >= base && p < base + 3 * n; };
  if (in(g_caller, g_caller_n)) return u32(1), u32(static_cast<uint32_t>((p - g_caller) / 3));
  if (in(w.region.data(), w.region_count)) return u32(2), u32(static_cast<uint32_t>((p - w.region.data()) / 3));
  if (in(w.poly, w.poly_count)) return u32(3), u32(static_cast<uint32_t>((p - w.poly) / 3));
  u32(p ? 9 : 0), u32(0);
}
uint32_t edge_ref(zgeo::Weiler& w, const zgeo::Edge* e) {
  for (int32_t i = 0; i < static_cast<int32_t>(w.edges.items.size()); ++i)
    if (&w.edges.at(i) == e) return static_cast<uint32_t>(i);
  return e ? 0xfffe : 0xffff;
}
uint32_t contour_ref(zgeo::Weiler& w, const zgeo::Contour* c) {
  for (int32_t i = 0; i < static_cast<int32_t>(w.contours.items.size()); ++i)
    if (&w.contours.at(i) == c) return static_cast<uint32_t>(i);
  return c ? 0xfffe : 0xffff;
}
void dump(zgeo::Weiler& w) {
  u32(w.want), u32(static_cast<uint32_t>(w.axes)), u32(static_cast<uint32_t>(w.region_count));
  floats(w.region.data(), 3 * static_cast<size_t>(w.region_count));
  floats(w.offset, 2), u32(w.offset_on);
  u32(static_cast<uint32_t>(w.edges.count));
  for (int32_t i = 0; i < w.edges.count; ++i) {
    zgeo::Edge& e = w.edges.at(i);
    u32(edge_ref(w, e.prev)), u32(edge_ref(w, e.next)), u32(e.flags);
    point_ref(w, e.a), point_ref(w, e.b);
    u32(static_cast<uint32_t>(e.divided));
    floats(e.box, 4);
    u32(contour_ref(w, e.contour));
  }
  u32(static_cast<uint32_t>(w.contours.count));
  for (int32_t i = 0; i < w.contours.count; ++i) {
    zgeo::Contour& c = w.contours.at(i);
    u32(c.flags), u32(edge_ref(w, c.edge));
  }
}
void dump_result(const zgeo::Result& r) {
  for (int k = 0; k < 3; ++k) {
    u32(static_cast<uint32_t>(r.count(k)));
    for (const zgeo::ResultContour& c : r.lists[k]) u32(static_cast<uint32_t>(c.count)), u32(static_cast<uint32_t>(c.start));
  }
  u32(static_cast<uint32_t>(r.point_count()));
  floats(r.points.data(), r.points.size());
}
}  // namespace

// weiler_new over the points (changed in place), then the state; then
// reset() and the state again.
extern "C" int probe_weiler_new(float* points, int n, uint8_t* out, int cap) {
  g_out.clear();
  zerr::set_sink(sink);
  g_caller = points, g_caller_n = n;
  zgeo::Weiler* w = zgeo::weiler_new(points, n, 0);
  u32(static_cast<uint32_t>(w->region_count)), floats(points, 3 * static_cast<size_t>(n));
  dump(*w);
  const float* r = nullptr;
  u32(static_cast<uint32_t>(zgeo::weiler_region(w, &r))), u32(r == w->region.data());
  zgeo::reset(*w);
  dump(*w);
  zgeo::weiler_free(w);
  return finish(out, cap);
}

// The driver's first steps over a region and a polygon: the polygon set and
// moved by the region's offset, the case, its whole-polygon results, the
// results' z, then the offset taken back; then reset() and the side tables.
extern "C" int probe_weiler_case(float* region, int nr, float* poly, int np, uint32_t want, uint8_t* out,
                                 int cap) {
  g_out.clear();
  zerr::set_sink(sink);
  g_caller = region, g_caller_n = nr;
  zgeo::Weiler* w = zgeo::weiler_new(region, nr, 0);
  zgeo::Result result;
  w->poly = poly, w->poly_count = np, w->want = want, w->out = &result;
  if (w->offset_on) zgeo::recenter(*w);
  const int code = zgeo::trivial_case(*w);
  u32(static_cast<uint32_t>(code));
  if (code == 4 || code == 3) u32(static_cast<uint32_t>(zgeo::output_whole(*w, 3)));
  if (code == 2) {
    u32(static_cast<uint32_t>(zgeo::output_with_hole(*w)));
    u32(static_cast<uint32_t>(zgeo::output_whole(*w, 2)));
  }
  if (code >= 2) zgeo::plane_z(*w);
  if (w->offset_on) zgeo::uncenter(*w);
  dump_result(result);
  floats(poly, 3 * static_cast<size_t>(np));
  zgeo::reset(*w);
  zgeo::side_tables(*w);
  floats(w->side_region.data(), w->side_region.size());
  floats(w->side_poly.data(), w->side_poly.size());
  zgeo::weiler_free(w);
  return finish(out, cap);
}

// Leaves: 0 point_in_polygon (q, poly), 1 all_inside(a, b, code), 2 snap
// (a, p), 3 snap_to_segment (a, b, p), 4 within_segment (q, a, b), 5
// drop_repeats (points).
extern "C" int probe_weiler_leaf(int op, const float* a, int na, float* b, int nb, float eps1, float eps2, int code,
                                 uint8_t* out, int cap) {
  g_out.clear();
  if (op == 0) u32(static_cast<uint8_t>(zgeo::point_in_polygon(a, nb, b)));
  if (op == 1) u32(static_cast<uint32_t>(zgeo::all_inside(na, a, nb, b, code)));
  if (op == 2) u32(static_cast<uint32_t>(zgeo::snap(a, na, b, nb, eps1, eps2))), floats(b, 3 * static_cast<size_t>(nb));
  if (op == 3) u32(static_cast<uint32_t>(zgeo::snap_to_segment(a, a + 3, b, eps1))), floats(b, 3);
  if (op == 4) u32(static_cast<uint32_t>(zgeo::within_segment(b, a, a + 3)));
  if (op == 5) {
    const int32_t n = zgeo::drop_repeats(b, nb);
    u32(static_cast<uint32_t>(n)), floats(b, 3 * static_cast<size_t>(nb));
  }
  return finish(out, cap);
}
