#pragma once
#include <cstdint>
#include <deque>
#include <vector>

// zGeometry's Weiler–Atherton polygon clipping (zGeometry\zgeo_weiler.cpp,
// 0x0050f950..0x005158c0; docs/craters.md, section 5). A Weiler holds the
// clip region (the crater's outline); 0x0050faf0 clips a polygon against it
// into the parts inside, outside and both. Ported so far (part 3a): the
// state, its rings and contours, the trivial cases, the whole-polygon
// results and the geometric leaves.
namespace zgeo {

struct Contour;
struct Crossing;

// One side of a polygon ring, 0x3c bytes. A polygon is two rings over the
// same points: the first flagged 1 (the region) or 2 (the clipped polygon),
// the second 4.
struct Edge {
  Edge* prev = nullptr;            // +0x00
  Edge* next = nullptr;            // +0x04
  uint32_t flags = 0;              // +0x08
  float* a = nullptr;              // +0x0c  its points (x, y, z)
  float* b = nullptr;              // +0x10
  int32_t divided = 0;             // +0x14  0 once its box is set
  float box[4] = {};               // +0x18 min x, +0x1c min y, +0x24 max x, +0x28 max y
  uint32_t unk_20 = 0;             // +0x20
  uint32_t unk_2c = 0;             // +0x2c
  Crossing* crossing = nullptr;    // +0x30
  float* mark = nullptr;           // +0x34
  Contour* contour = nullptr;      // +0x38  set on a ring's first edge
};

// A ring of edges, 0x0c bytes.
struct Contour {
  uint32_t flags = 0;              // +0x00  its first edge's, then the union of its edges'
  Edge* edge = nullptr;            // +0x04
  int32_t count = 0;               // +0x08  edges, once counted (0x00513390)
};

// A crossing of two edges, 0x30 bytes (filled by 0x00510250, part 3b).
struct Crossing {
  uint32_t raw[12] = {};
};

// The exe's growable buffers (0x14 bytes: element size, capacity, count,
// base, cursor; 0x005128e0 / 0x00512940 / 0x00512910 / 0x00514dc0).
// FIX (bug): the exe reallocs them while edges hold pointers into them (past
// 128 entries the clip reads freed memory); a deque keeps them in place.
// Count resets keep the old entries, as the exe's do.
template <typename T>
struct Pool {
  std::deque<T> items;
  int32_t count = 0;
  T* alloc(int32_t n) {
    while (static_cast<int32_t>(items.size()) < count + n) items.emplace_back();
    T* first = &items[static_cast<size_t>(count)];
    count += n;
    return first;
  }
  T& at(int32_t i) { return items[static_cast<size_t>(i)]; }
  void reset(int32_t n) {  // 0x00514dc0
    while (static_cast<int32_t>(items.size()) < n) items.emplace_back();
    count = n;
  }
};

// A list of result contours: point count and first float in Result::points.
struct ResultContour {
  int32_t count = 0;
  int32_t start = 0;               // an index into the floats (3 per point)
};
// What a clip leaves (0x20 bytes, 0x0050fe10 frees it): the contours inside
// the region, outside it, and the third list, and their points.
struct Result {
  std::vector<ResultContour> lists[3];  // +0x00/+0x04, +0x08/+0x0c, +0x10/+0x14
  std::vector<float> points;            // +0x18 count (points), +0x1c
  int32_t count(int i) const { return static_cast<int32_t>(lists[i].size()); }
  int32_t point_count() const { return static_cast<int32_t>(points.size() / 3); }
};

// 0x28cc bytes.
struct Weiler {
  uint32_t want = 0;               // +0x00  1 inside, 2 outside, 4 the third list
  int32_t axes = 0;                // +0x04  0: x, y, z as given; 1: (z, y, x); 2: (x, z, y)
  Result* out = nullptr;           // +0x08
  std::vector<float> region;       // +0x0c pool: +0x14 count, +0x18 points
  int32_t region_count = 0;        // +0x14
  int32_t poly_count = 0;          // +0x28  the polygon being clipped
  float* poly = nullptr;           // +0x2c  (the caller's buffer)
  Pool<Edge> edges;                // +0x34
  Pool<Contour> contours;          // +0x48
  Pool<Crossing> crossings;        // +0x5c
  // Each polygon's points against the other's edges. FIX (bug): 1,280
  // floats each in the exe, overrun past that.
  std::vector<float> side_region;  // +0x0c0  rows: polygon edges, columns: region points
  std::vector<float> side_poly;    // +0x14c0 rows: region edges, columns: polygon points
  float offset[2] = {};            // +0x28c0  the region's first point when far out
  uint8_t offset_on = 0;           // +0x28c8
  uint8_t unk_28c9 = 0;            // +0x28c9
};

// a and b within `eps` in x and in y.
int points_near(const float a[2], const float b[2], float eps);  // 0x00515130 ECX a, EDX b

// The repeats are dropped from the caller's points in place, and the first
// rings are built over them (reset() rebuilds them over the copy before any
// clip). The axes are swapped before `axes` is set: x and z, whatever it is
// (every caller passes 0).
Weiler* weiler_new(float* points, int32_t n, int32_t axes);        // 0x0050f960 ECX points, EDX n
// *w made again over new points with its axes.
int weiler_renew(Weiler** w, float* points, int32_t n);             // 0x0050fa70 ECX w, EDX points
void weiler_free(Weiler* w);                                         // 0x0050fab0
// The region's points (in the Weiler's axes) and their count.
int32_t weiler_region(const Weiler* w, const float** points);       // 0x0050f950 ECX w, EDX points

// Consecutive points within 0.01 (the last against the first) merged.
int32_t drop_repeats(float* points, int32_t n);                     // 0x00515360 ECX points, EDX n
// The region's points, or the polygon's when set, between the caller's
// axes and the clip's (both ways).
void swap_axes(Weiler& w);                                           // 0x00513680
// A region far from the origin is clipped moved to it: its first point
// (0x00514c40), and the polygon by the same; back with the results (0x00514dd0).
void recenter(Weiler& w);                                            // 0x00514c40
void uncenter(Weiler& w);                                            // 0x00514dd0

// n edges from `first` round the points (0x005146a0 ECX first, EDX points).
void ring(Edge* first, float* points, int32_t n, uint32_t flags);
void edge_box(Edge& e);                                              // 0x005136f0
void edge_boxes(Edge* first, int32_t n);                             // 0x00514680 ECX first, EDX n
int new_contour(Weiler& w, Edge* e);                                 // 0x005129a0 EDX e
// A polygon's two rings and their contours (0x0050fe70 EDX points).
int add_rings(Weiler& w, float* points, int32_t n, uint32_t flags);
void reset(Weiler& w);                                               // 0x00514d10
void side_tables(Weiler& w);                                         // 0x00513750

// Point q against a closed polygon: 1 inside, -1 outside, 0 on its outline.
int point_in_polygon(const float q[2], int32_t n, const float* poly);  // 0x00513cf0 ECX q, EDX n
// Every point of a inside b: `code`, 4 for two equal polygons (code 2
// only), else 0.
int all_inside(int32_t na, const float* a, int32_t nb, const float* b, int code);  // 0x00510180
// By the boxes and containment: 1 apart, 2 the region inside the polygon,
// 3 the polygon inside the region, 4 the same, 0 they cross.
int trivial_case(Weiler& w);                                         // 0x0050ff70

// One whole polygon as the first list's only contour (`which` 3 the
// polygon, else the region), when wanted.
int output_whole(Weiler& w, int which);                              // 0x005139e0 EDX which
// The region inside the polygon: the polygon with the region as a hole,
// joined at their rightmost points, as the second list's contour.
int output_with_hole(Weiler& w);                                     // 0x00513a90
// The polygon point the bridge from q may reach without crossing the
// region (0x00515040 ECX q, EDX *bridge).
void bridge_point(const float q[3], const float** bridge, const Weiler& w);
// The results' z from the plane through the polygon's first three points.
void plane_z(Weiler& w);                                             // 0x00514e40

// q within a b's extent along its longer axis (0x00514f80 ECX q, EDX a).
int within_segment(const float q[2], const float a[2], const float b[2]);
// p moved onto the segment a b when within eps of it (strictly between).
int snap_to_segment(const float a[2], const float b[2], float p[3], float eps);  // 0x00515170 ECX a, EDX b
// Each point of p snapped onto a's points (within eps_point) or edges.
int snap(const float* a, int32_t na, float* p, int32_t np, float eps_point, float eps_edge);  // 0x00515410

}  // namespace zgeo
