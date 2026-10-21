#include "gamez/zgeometry/zgeo_weiler.h"

#include <cmath>
#include <cstring>

#include "gamez/zerror/zerr.h"

#pragma STDC FP_CONTRACT OFF

namespace zgeo {
namespace {
const char kFile[] = "C:\\zipper\\GameZMech3\\zGeometry\\zgeo_weiler.cpp";
constexpr float kRepeat = 0.01f;     // 0x00515360's push
constexpr float kFar = 65536.0f;     // 0x00514c40
constexpr double kSame = 0.0010000000474974513;  // 0x005cdd38, (double)0.001f
constexpr double kTie = 9.999999747378752e-06;   // 0x005cdd40, (double)1e-5f

void copy3(float* to, const float* from) { std::memcpy(to, from, 12); }

// The sign of v - w as 0x00513cf0 takes it: -1 below, 0 equal, 1 above.
int sign_of(float v, float w) {
  if (v < w) return -1;
  return v <= w ? 0 : 1;
}
}  // namespace

// Mech3.exe 1.2 @ 0x00515130
int points_near(const float a[2], const float b[2], float eps) {
  return std::fabs(a[0] - b[0]) <= eps && std::fabs(a[1] - b[1]) <= eps;
}

// Mech3.exe 1.2 @ 0x00515360
int32_t drop_repeats(float* points, int32_t n) {
  for (int32_t i = 0; i < n; ++i) {
    if (!points_near(points + 3 * i, points + 3 * ((i + 1) % n), kRepeat)) continue;
    if (i != n - 1) {
      std::memmove(points + 3 * i, points + 3 * (i + 1), 12 * static_cast<size_t>(n - i - 1));
      --i;
    }
    --n;
  }
  return n;
}

// Mech3.exe 1.2 @ 0x00513680
void swap_axes(Weiler& w) {
  float* p = w.poly ? w.poly : w.region.data();
  const int32_t n = w.poly ? w.poly_count : w.region_count;
  const int k = w.axes == 2 ? 1 : 0;
  for (int32_t i = 0; i < n; ++i, p += 3) {
    const float t = p[k];
    p[k] = p[2];
    p[2] = t;
  }
}

// Mech3.exe 1.2 @ 0x00514c40
void recenter(Weiler& w) {
  if (!w.poly) {
    float* p = w.region.data();
    if (p[0] < kFar && -kFar < p[0] && p[1] < kFar && -kFar < p[1]) {
      w.offset_on = 0;
      return;
    }
    w.offset[0] = p[0], w.offset[1] = p[1];
    w.offset_on = 1;
    for (int32_t i = 0; i < w.region_count; ++i, p += 3) p[0] -= w.offset[0], p[1] -= w.offset[1];
  } else {
    float* p = w.poly;
    for (int32_t i = 0; i < w.poly_count; ++i, p += 3) p[0] -= w.offset[0], p[1] -= w.offset[1];
  }
}

// Mech3.exe 1.2 @ 0x00514dd0
void uncenter(Weiler& w) {
  const float dx = w.offset[0], dy = w.offset[1];
  float* p = w.poly;
  for (int32_t i = 0; i < w.poly_count; ++i, p += 3) p[0] = dx + p[0], p[1] = dy + p[1];
  std::vector<float>& r = w.out->points;
  for (size_t i = 0; i + 2 < r.size(); i += 3) r[i] = dx + r[i], r[i + 1] = dy + r[i + 1];
}

// Mech3.exe 1.2 @ 0x005146a0
void ring(Edge* first, float* points, int32_t n, uint32_t flags) {
  Edge* e = first;
  for (int32_t i = 0; i < n; ++i, ++e) {
    e->prev = e - 1;
    e->next = e + 1;
    e->flags = flags;
    e->a = points + 3 * i;
    e->b = points + 3 * (i + 1);
    e->crossing = nullptr, e->mark = nullptr, e->contour = nullptr;
  }
  first->prev = first + n - 1;
  Edge* last = first + n - 1;
  last->next = first;
  last->b = points;
}

// Mech3.exe 1.2 @ 0x005136f0
void edge_box(Edge& e) {
  const float *a = e.a, *b = e.b;
  if (a[0] <= b[0]) {
    e.box[0] = a[0], e.box[2] = b[0];
  } else {
    e.box[0] = b[0], e.box[2] = a[0];
  }
  if (b[1] < a[1]) {
    e.box[1] = b[1], e.box[3] = a[1];
  } else {
    e.box[1] = a[1], e.box[3] = b[1];
  }
  e.divided = 0;
}

// Mech3.exe 1.2 @ 0x00514680
void edge_boxes(Edge* first, int32_t n) {
  for (int32_t i = 0; i < n; ++i) edge_box(first[i]);
}

// Mech3.exe 1.2 @ 0x005129a0
int new_contour(Weiler& w, Edge* e) {
  if (e->contour) return 1;
  Contour* c = w.contours.alloc(1);
  c->edge = e;
  c->flags = e->flags;
  e->contour = c;
  return 1;
}

// Mech3.exe 1.2 @ 0x0050fe70
int add_rings(Weiler& w, float* points, int32_t n, uint32_t flags) {
  Edge* first = w.edges.alloc(2 * n);
  ring(first, points, n, flags);
  first->contour = nullptr;
  new_contour(w, first);
  edge_boxes(first, n);
  Edge* second = first + n;
  ring(second, points, n, 4);
  second->contour = nullptr;
  new_contour(w, second);
  return 1;
}

// Mech3.exe 1.2 @ 0x0050f960
Weiler* weiler_new(float* points, int32_t n, int32_t axes) {
  if (!n || !points) zerr::report(0x200, kFile, 0x203, "Bad clip region passed to Weiler");
  n = drop_repeats(points, n);
  Weiler* w = new Weiler;
  w->region.assign(points, points + 3 * n);
  w->region_count = n;
  w->poly_count = 0, w->poly = nullptr;
  if (axes) swap_axes(*w);
  w->axes = axes;
  recenter(*w);
  add_rings(*w, points, n, 1);
  return w;
}

// Mech3.exe 1.2 @ 0x0050fa70
int weiler_renew(Weiler** w, float* points, int32_t n) {
  if (!points || !n) return 0;
  Weiler* fresh = weiler_new(points, n, (*w)->axes);
  weiler_free(*w);
  *w = fresh;
  return 1;
}

// Mech3.exe 1.2 @ 0x0050fab0
void weiler_free(Weiler* w) { delete w; }

// Mech3.exe 1.2 @ 0x0050f950
int32_t weiler_region(const Weiler* w, const float** points) {
  if (!w) return 0;
  *points = w->region.data();
  return w->region_count;
}

// Mech3.exe 1.2 @ 0x00514d10
void reset(Weiler& w) {
  const int32_t n = w.region_count;
  w.edges.reset(2 * n);
  w.contours.reset(2);
  w.crossings.reset(0);
  Edge* first = &w.edges.at(0);
  Contour* c = &w.contours.at(0);
  ring(first, w.region.data(), n, 1);
  first->contour = c;
  c->edge = first;
  edge_boxes(first, n);
  Edge* second = &w.edges.at(n);
  ring(second, w.region.data(), n, 4);
  second->contour = c + 1;
  c[1].edge = second;
  edge_boxes(second, n);
}

// Mech3.exe 1.2 @ 0x00513750. Row i holds each point's side of edge i
// (from point i to i + 1), then the row's first value again.
void side_tables(Weiler& w) {
  for (int pass = 0; pass < 2; ++pass) {
    const float* edges = pass == 0 ? w.poly : w.region.data();
    const int32_t ne = pass == 0 ? w.poly_count : w.region_count;
    const float* pts = pass == 0 ? w.region.data() : w.poly;
    const int32_t np = pass == 0 ? w.region_count : w.poly_count;
    std::vector<float>& t = pass == 0 ? w.side_region : w.side_poly;
    t.assign(static_cast<size_t>(ne) * (np + 1), 0.0f);
    float* out = t.data();
    for (int32_t i = 0; i < ne; ++i) {
      const float* a = edges + 3 * i;
      float* row = out;
      if (i != ne - 1) {
        const float* b = a + 3;
        for (int32_t k = 0; k < np; ++k) {
          const float* q = pts + 3 * k;
          *out++ = (q[0] - a[0]) * (b[1] - a[1]) - (q[1] - a[1]) * (b[0] - a[0]);
        }
      } else {
        const float* b = edges;
        for (int32_t k = 0; k < np; ++k) {
          const float* q = pts + 3 * k;
          *out++ = (b[1] - a[1]) * (q[0] - a[0]) - (b[0] - a[0]) * (q[1] - a[1]);
        }
      }
      *out++ = row[0];
    }
  }
}

// Mech3.exe 1.2 @ 0x00513cf0. Counts the polygon's crossings of the ray
// from q towards +x.
int point_in_polygon(const float q[2], int32_t n, const float* poly) {
  float px = poly[3 * n - 3], py = poly[3 * n - 2];
  int sx = sign_of(px, q[0]), sy = sign_of(py, q[1]);
  if (sx == 0 && sy == 0) return 0;
  int crossings = 0;
  for (int32_t i = 0; i < n; ++i) {
    const float cx = poly[3 * i], cy = poly[3 * i + 1];
    const int old_sx = sx, old_sy = sy;
    sx = sign_of(cx, q[0]), sy = sign_of(cy, q[1]);
    if (sx == 0 && sy == 0) return 0;
    if (sx != old_sx) {
      if ((sy >= 0) != (old_sy >= 0)) {
        const float x = (px - cx) * ((q[1] - cy) / (py - cy)) + cx;
        const int s = sign_of(x, q[0]);
        if (s == 0) return 0;
        if (s == 1) ++crossings;
      } else if (sy == 0 && old_sy == 0) {
        return 0;
      }
    } else if ((sy >= 0) != (old_sy >= 0)) {
      if (sx == 1) ++crossings;
      else if (sx == 0) return 0;
    }
    px = cx, py = cy;
  }
  return crossings & 1 ? 1 : -1;
}

// Mech3.exe 1.2 @ 0x00510180 ECX na, EDX a
int all_inside(int32_t na, const float* a, int32_t nb, const float* b, int code) {
  if (code == 2 && na == nb) {
    bool same = true;
    for (int32_t j = 0; j < nb && same; ++j) {
      same = false;
      for (int32_t i = 0; i < na; ++i)
        if (std::fabs(a[3 * i] - b[3 * j]) <= kSame && std::fabs(a[3 * i + 1] - b[3 * j + 1]) <= kSame) {
          same = true;
          break;
        }
    }
    if (same) return 4;
  }
  for (int32_t i = 0; i < na; ++i)
    if (point_in_polygon(a + 3 * i, nb, b) < 0) return 0;
  return code;
}

// Mech3.exe 1.2 @ 0x0050ff70
int trivial_case(Weiler& w) {
  auto box = [](const float* p, int32_t n, float b[4]) {
    b[0] = b[2] = p[0], b[1] = b[3] = p[1];
    for (int32_t i = 1; i < n; ++i) {
      const float* q = p + 3 * i;
      if (q[0] < b[0]) b[0] = q[0];
      if (b[2] < q[0]) b[2] = q[0];
      if (q[1] < b[1]) b[1] = q[1];
      if (b[3] < q[1]) b[3] = q[1];
    }
  };
  float c[4], r[4];  // the polygon's, the region's
  box(w.poly, w.poly_count, c);
  box(w.region.data(), w.region_count, r);
  if (r[2] <= c[0] || c[2] <= r[0] || r[3] <= c[1] || c[3] <= r[1]) return 1;
  if (c[0] <= r[0] && r[2] <= c[2] && c[1] <= r[1] && r[3] <= c[3])
    return all_inside(w.region_count, w.region.data(), w.poly_count, w.poly, 2);
  if (r[0] <= c[0] && c[2] <= r[2] && r[1] <= c[1] && c[3] <= r[3])
    return all_inside(w.poly_count, w.poly, w.region_count, w.region.data(), 3);
  return 0;
}

// Mech3.exe 1.2 @ 0x005139e0
int output_whole(Weiler& w, int which) {
  if (!(w.want & 1)) return 1;
  const float* src = which == 3 ? w.poly : w.region.data();
  const int32_t n = which == 3 ? w.poly_count : w.region_count;
  Result& out = *w.out;
  out.lists[0].assign(1, ResultContour{n, out.point_count() * 3});
  out.points.insert(out.points.end(), src, src + 3 * n);
  return 1;
}

// Mech3.exe 1.2 @ 0x00515040. Only the region's closing edge is ever tested
// (the loop never moves on); harmless from the rightmost points.
void bridge_point(const float q[3], const float** bridge, const Weiler& w) {
  const float* first = w.region.data();
  const float* last = first + 3 * w.region_count - 3;
  for (int32_t i = w.region_count; i != 0;) {
    --i;
    const float *p = last, *c = first;
    const float ex = c[0] - p[0], ey = c[1] - p[1];
    const float s = (q[0] - p[0]) * ey - (q[1] - p[1]) * ex;
    const float t = ((*bridge)[0] - p[0]) * ey - ((*bridge)[1] - p[1]) * ex;
    if ((s > 0.0f && t < 0.0f) || (s < 0.0f && t > 0.0f)) {
      *bridge = c[0] < q[0] ? p : c;
      i = w.region_count;
    }
  }
}

// Mech3.exe 1.2 @ 0x00513a90
int output_with_hole(Weiler& w) {
  if (!(w.want & 2)) return 1;
  Result& out = *w.out;
  const int32_t nr = w.region_count, np = w.poly_count;
  const int32_t count = np + 2 + nr;
  out.lists[1].push_back(ResultContour{count, out.point_count() * 3});
  const float* region = w.region.data();
  const float* rbest = region;
  for (int32_t i = 1; i < nr; ++i) {
    const float* c = region + 3 * i;
    if (c[0] > rbest[0] || (std::fabs(c[0] - rbest[0]) < kTie && c[1] > rbest[1])) rbest = c;
  }
  const float* pbest = w.poly;
  for (int32_t i = 1; i < np; ++i) {
    const float* c = w.poly + 3 * i;
    if (c[0] > pbest[0] || (std::fabs(c[0] - pbest[0]) < kTie && pbest[1] < c[1])) pbest = c;
  }
  bridge_point(pbest, &rbest, w);
  size_t at = out.points.size();
  out.points.resize(at + 3 * static_cast<size_t>(count));
  const float* p = pbest;
  for (int32_t i = 0; i < np; ++i, at += 3) {
    copy3(&out.points[at], p);
    p = p == w.poly + 3 * np - 3 ? w.poly : p + 3;
  }
  copy3(&out.points[at], p), at += 3;
  const float* r = rbest;
  for (int32_t i = 0; i < nr; ++i, at += 3) {
    copy3(&out.points[at], r);
    r = r == region ? region + 3 * nr - 3 : r - 3;
  }
  copy3(&out.points[at], r);
  return 1;
}

// Mech3.exe 1.2 @ 0x00514e40
void plane_z(Weiler& w) {
  const float* p = w.poly;
  const float ax = p[0] - p[3], bx = p[6] - p[3];
  const float det = ax * (p[7] - p[4]) - (p[1] - p[4]) * bx;
  if (det == 0.0f) return;
  const float gx = ((p[1] - p[4]) * (p[8] - p[5]) - (p[2] - p[5]) * (p[7] - p[4])) / det;
  const float gy = ((p[2] - p[5]) * bx - ax * (p[8] - p[5])) / det;
  const float x0 = p[0], y0 = p[1], z0 = p[2];
  std::vector<float>& r = w.out->points;
  for (size_t i = 0; i + 2 < r.size(); i += 3)
    r[i + 2] = -(r[i + 1] * gy + gx * r[i] + -(y0 * gy + gx * x0 + z0));
}

// Mech3.exe 1.2 @ 0x00514f80
int within_segment(const float q[2], const float a[2], const float b[2]) {
  if (std::fabs(a[0] - b[0]) >= 1e-05f) {
    if (a[0] < b[0]) return a[0] <= q[0] && q[0] <= b[0];
    return b[0] <= q[0] && q[0] <= a[0];
  }
  if (a[1] < b[1]) return a[1] <= q[1] && q[1] <= b[1];
  return b[1] <= q[1] && q[1] <= a[1];
}

// Mech3.exe 1.2 @ 0x00515170
int snap_to_segment(const float a[2], const float b[2], float p[3], float eps) {
  const float dx = b[0] - a[0], px = p[0] - a[0];
  const float dy = b[1] - a[1], py = p[1] - a[1];
  if (eps <= std::fabs(dx)) {
    if (eps <= std::fabs(dy)) {
      const float tx = px / dx, ty = py / dy;
      if (!(std::fabs(tx - ty) < eps) || !(0.0f < tx) || !(0.0f < ty) || !(tx < 1.0f) || !(ty < 1.0f)) return 0;
      const float x = tx * dx + a[0];
      if (std::fabs(x - p[0]) > eps) return 0;
      const float y = ty * dy + a[1];
      if (std::fabs(y - p[1]) > eps) return 0;
      p[1] = y, p[0] = x;
      return 1;
    }
    if (std::fabs(py) <= eps && 0.0f < px / dx && px / dx < 1.0f) {
      p[1] = a[1];
      return 1;
    }
    return 0;
  }
  if (std::fabs(px) <= eps && 0.0f < py / dy && py / dy < 1.0f) {
    p[0] = a[0];
    return 1;
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x00515410 ECX a, EDX na
int snap(const float* a, int32_t na, float* p, int32_t np, float eps_point, float eps_edge) {
  int changed = 0;
  for (int32_t i = 0; i < na; ++i)
    for (int32_t j = 0; j < np; ++j) {
      float* q = p + 3 * j;
      if (points_near(a + 3 * i, q, eps_point)) {
        copy3(q, a + 3 * i);
        changed = 1;
      } else if (snap_to_segment(a + 3 * i, a + 3 * ((i + 1) % na), q, eps_edge)) {
        changed = 1;
      }
    }
  return changed;
}

}  // namespace zgeo
