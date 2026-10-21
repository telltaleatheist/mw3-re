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
    e->start = nullptr, e->end = nullptr, e->contour = nullptr;
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

// Mech3.exe 1.2 @ 0x00513930
int split_pair(Weiler& w, Edge* e1, Edge* e2, float* point, uint32_t f1, uint32_t f2) {
  Edge* e = e1;
  uint32_t f = f1;
  for (int k = 0; k < 2; ++k) {
    Edge* n = w.edges.alloc(1);
    n->prev = e;
    n->next = e->next;
    n->flags = e->flags | f;
    n->a = point;
    n->start = nullptr;
    n->b = e->b;
    n->end = e->end;
    n->contour = nullptr;
    edge_box(*n);
    e->next->prev = n;
    e->next = n;
    e = e2, f = f2;
  }
  return 1;
}

// Mech3.exe 1.2 @ 0x00510250 ("WeedOut"). Region edge R (and its twin R2 in
// the second ring) against polygon edge P (P2), walked in ring order over the
// edges as built (pieces split off are not visited). The case is which ends
// lie within the other edge: R.a on P (8), R.b on P (4), P.a on R (2), P.b
// on R (1). `back`: the two run opposite ways in x or in y.
int weed_out(Weiler& w) {
  const int32_t nr = w.region_count, np = w.poly_count;
  Edge* r = w.contours.at(0).edge;
  Edge* r2 = w.contours.at(1).edge;
  Edge* const p_first = w.contours.at(2).edge;
  Edge* const p2_first = w.contours.at(3).edge;
  const float* side_poly = w.side_poly.data();
  auto far = [](const float* u, const float* v) {
    return std::fabs(u[0] - v[0]) > kSame || std::fabs(u[1] - v[1]) > kSame;
  };
  for (int32_t i = 0; i < nr; ++i, ++r, ++r2, ++side_poly) {
    const float* side_region = w.side_region.data() + i;
    float* ra = r->a;
    float* rb = r->b;
    Edge* p = p_first;
    Edge* p2 = p2_first;
    for (int32_t j = 0; j < np; ++j, ++p, ++p2, side_region += nr + 1, ++side_poly) {
      float* pa = p->a;
      float* pb = p->b;
      if (!(std::fabs(side_region[0]) < kTie) || !(std::fabs(side_region[1]) < kTie) ||
          !(std::fabs(side_poly[0]) < kTie) || !(std::fabs(side_poly[1]) < kTie))
        continue;
      const int code = within_segment(ra, pa, pb) << 3 | within_segment(rb, pa, pb) << 2 |
                       within_segment(pa, ra, rb) << 1 | within_segment(pb, ra, rb);
      const bool back = ((rb[0] < ra[0] || pb[0] < pa[0]) && (ra[0] < rb[0] || pa[0] < pb[0])) ||
                        ((rb[1] < ra[1] || pb[1] < pa[1]) && (ra[1] < rb[1] || pa[1] < pb[1]));
      switch (code) {
        case 3:  // P within R
          if (!back) {
            split_pair(w, r, r2, pb, 0, 0);
            r2->b = pa, rb = pa, r->b = pa;
            p->flags |= r->flags, p2->flags |= r2->flags;
          } else {
            split_pair(w, r, r2, pa, 0, 0);
            rb = pb, r2->b = pb, r->b = pb;
            p->flags |= r2->flags, p2->flags |= r->flags;
          }
          edge_box(*r);
          break;
        case 5:
          if (far(rb, pb)) {
            split_pair(w, r, r2, pb, p2->flags, p->flags);
            r2->b = pb, r->b = pb;
            p2->b = rb, p->b = rb;
            rb = r->b;
            edge_box(*r), edge_box(*p);
          }
          break;
        case 6:
          if (far(rb, pa)) {
            split_pair(w, p, p2, rb, 0, 0);
            p2->b = rb, p->b = rb;
            rb = pa;
            r2->b = pa, r->b = pa;
            p->flags |= r->flags, p2->flags |= r2->flags;
            edge_box(*r), edge_box(*p);
          }
          break;
        case 7:
          if (!back) {
            rb = pb, r2->b = pb, r->b = pb;
            p->flags |= r2->flags, p2->flags |= r->flags;
          } else {
            rb = pa, r2->b = pa, r->b = pa;
            p->flags |= r->flags, p2->flags |= r2->flags;
          }
          edge_box(*r);
          break;
        case 9:
          if (far(ra, pb)) {
            split_pair(w, r, r2, pb, 0, 0);
            r2->b = pb, r->b = pb;
            p2->b = ra;
            rb = pb;
            p->b = ra;
            r->flags |= p->flags, r2->flags |= p2->flags;
            edge_box(*r), edge_box(*p);
          }
          break;
        case 10:
          if (far(ra, pa)) {
            split_pair(w, r, r2, pa, 0, 0);
            r2->b = pa, r->b = pa;
            p2->a = ra, p->a = ra;
            r->flags |= p2->flags;
            ra = pa;
            r2->flags |= p->flags;
            edge_box(*r), edge_box(*p);
          }
          break;
        case 11:
          if (!back) {
            ra = pb, r2->a = pb, r->a = pb;
            p->flags |= r->flags, p2->flags |= r2->flags;
          } else {
            ra = pa, r2->a = pa, r->a = pa;
            p->flags |= r2->flags, p2->flags |= r->flags;
          }
          edge_box(*r);
          break;
        case 12:  // R within P
          if (!back) {
            split_pair(w, p, p2, rb, 0, 0);
            p2->b = ra, p->b = ra;
          } else {
            split_pair(w, p, p2, ra, 0, 0);
            p2->b = rb, p->b = rb;
          }
          r->flags |= p->flags, r2->flags |= p2->flags;
          edge_box(*p);
          break;
        case 13:
          if (!back) {
            p2->b = ra, p->b = ra;
            r->flags |= p->flags, r2->flags |= p2->flags;
          } else {
            p2->b = rb, p->b = rb;
            r->flags |= p2->flags, r2->flags |= p->flags;
          }
          edge_box(*p);
          break;
        case 14:
          if (!back) {
            p2->a = rb, p->a = rb;
            r->flags |= p->flags, r2->flags |= p2->flags;
          } else {
            p2->a = ra, p->a = ra;
            r->flags |= p2->flags, r2->flags |= p->flags;
          }
          edge_box(*p);
          break;
        case 15:  // the same edge: the polygon's pair taken out of its rings
          if (!back) {
            r->flags |= p->flags, r2->flags |= p2->flags;
          } else {
            r2->flags |= p->flags, r->flags |= p2->flags;
          }
          if (p == p_first) {
            w.contours.at(2).edge = p->next;
            w.contours.at(3).edge = p2->next;
            p->next->contour = &w.contours.at(2);
            p2->next->contour = &w.contours.at(3);
          }
          p->prev->next = p->next, p->next->prev = p->prev;
          p2->prev->next = p2->next, p2->next->prev = p2->prev;
          break;
        default:
          break;
      }
    }
  }
  return 1;
}

// Mech3.exe 1.2 @ 0x00514280
uint32_t crossing_type(const float a[2], const float b[2], const float c[2], const float d[2], const Weiler& w) {
  static const uint8_t kTypes[81] = {  // 0x005ec348, by the signs of s1..s4 (0 -, 1 zero, 2 +)
      0, 0, 0, 0, 2, 0, 0, 0, 0, 0,  2,  2, 14, 2, 2, 8, 23, 2, 0, 2, 2, 13, 2, 2, 5,  22, 0,
      0, 18, 6, 2, 2, 15, 2, 2, 0, 0, 2,  2, 2,  3, 2, 2, 2, 0, 0, 2, 2, 12, 2, 2, 7, 21, 0,
      0, 19, 4, 2, 2, 16, 2, 2, 0, 2, 20, 9, 2,  2, 17, 2, 2, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0};
  const float dx = d[0] - c[0], dy = d[1] - c[1];
  float s1 = (a[0] - c[0]) * dy - (a[1] - c[1]) * dx;  // a and b against c d
  float s2 = (b[0] - c[0]) * dy - (b[1] - c[1]) * dx;
  if ((s1 < 0.0f && s2 < 0.0f) || (s1 > 0.0f && s2 > 0.0f)) return 0;
  const float ex = b[0] - a[0], ey = b[1] - a[1];
  float s3 = (c[0] - a[0]) * ey - (c[1] - a[1]) * ex;  // c and d against a b
  float s4 = (d[0] - a[0]) * ey - (d[1] - a[1]) * ex;
  if ((s3 < 0.0f && s4 < 0.0f) || (s3 > 0.0f && s4 > 0.0f)) return 0;
  if ((s1 == 0.0f) + (s2 == 0.0f) + (s3 == 0.0f) + (s4 == 0.0f) == 2) {
    // Two ends touch: a point just inside c d from the touching end, in the
    // region or not, settles which side the touch counts on.
    const float k = s3 == 0.0f ? -9.999999747378752e-06f : -0.9999899864196777f;  // 0x005cdd60 / 64
    const float q[2] = {c[0] - dx * k, c[1] - dy * k};
    const float s = s3 == 0.0f ? s4 : s3;
    const int in = point_in_polygon(q, w.region_count, w.region.data());
    if (s > 0.0f ? in > 0 : in < 0) {
      s1 = -s1, s2 = -s2;
      if (s3 == 0.0f) s4 = -s4;
      else s3 = -s3;
    }
  }
  auto cls = [](float v) { return v < 0.0f ? 0 : v == 0.0f ? 1 : 2; };
  return kTypes[27 * cls(s1) + 9 * cls(s2) + 3 * cls(s3) + cls(s4)];
}

// Mech3.exe 1.2 @ 0x00513f20. The points come by value (12 floats).
uint32_t intersect(Weiler& w, const float a[3], const float b[3], const float c[3], const float d[3],
                   Crossing** out) {
  const uint32_t t = crossing_type(a, b, c, d, w);
  Crossing* x = nullptr;
  switch (t) {
    case 4:
    case 5: {
      const float u = c[0] - d[0], v = c[1] - d[1];
      const float ex = b[0] - a[0], ey = b[1] - a[1];
      const float det = v * ex - u * ey;
      if (det == 0.0f) {
        *out = nullptr;
        return 0;
      }
      x = w.crossings.alloc(1);
      const float inv = 1.0f / det;
      const float s = (c[0] - a[0]) * (v * inv) + (c[1] - a[1]) * -(u * inv);
      x->point[0] = ex * s + a[0];
      const float y = ey * s + a[1];
      x->point[1] = y;
      const float cd = c[0] - d[0];
      if (cd != 0.0f) x->point[2] = (d[2] - c[2]) * ((c[0] - x->point[0]) / cd) + c[2];
      else x->point[2] = (d[2] - c[2]) * ((c[1] - y) / (c[1] - d[1])) + c[2];
      break;
    }
    case 6:
    case 7:
      x = w.crossings.alloc(1), std::memcpy(x->point, a, 12);
      break;
    case 8:
    case 9:
      x = w.crossings.alloc(1), std::memcpy(x->point, b, 12);
      break;
    case 12: case 13: case 14: case 15: case 16: case 17:
      x = w.crossings.alloc(1), std::memcpy(x->point, c, 12);
      break;
    case 18: case 19: case 20: case 21: case 22: case 23:
      x = w.crossings.alloc(1), std::memcpy(x->point, d, 12);
      break;
    default:
      break;
  }
  if (x) x->type = t;
  *out = x;
  return t;
}

// Mech3.exe 1.2 @ 0x00513860
int divide(Weiler& w, Crossing* x, Edge* e, int at_end) {
  Edge* n;
  if (points_near(e->b, x->point, 0.001f)) {
    n = e->next;
  } else {
    n = w.edges.alloc(1);
    n->a = x->point;
    n->b = e->b;
    e->b = x->point;
    n->next = e->next;
    n->prev = e;
    e->next->prev = n;
    e->next = n;
    n->flags = e->flags;
    n->contour = nullptr;
    n->end = e->end;
    n->divided = e->divided = 1;
  }
  if (at_end) {
    e->end = x;
    n->start = x;
  } else {
    n->start = nullptr;
  }
  return 1;
}

// Mech3.exe 1.2 @ 0x00514730
int corner_side(const Edge* e1, const Edge* e2, const Edge* e3) {
  const float* v = e1->b;
  if (v != e2->a) return 0;
  const float *a = e1->a, *c = e2->b, *p = e3->a;
  const float dy = e3->b[1] - p[1], dx = e3->b[0] - p[0];
  const float s1 = (a[0] - p[0]) * dy - (a[1] - p[1]) * dx;
  const float s2 = (c[0] - p[0]) * dy - (c[1] - p[1]) * dx;
  if ((0.0f <= s1 && 0.0f <= s2) || (s1 <= 0.0f && s2 <= 0.0f))
    return (v[1] - a[1]) * (c[0] - a[0]) - (c[1] - a[1]) * (v[0] - a[0]) <= 0.0f ? 2 : 1;
  return 7;
}

// Mech3.exe 1.2 @ 0x00514840
int corner_pair(const Edge* e1, const Edge* e2, const Edge* e3, const Edge* e4) {
  const float* v1 = e1->b;
  const float* v2 = e2->a;
  if (v1 != v2) return 0;
  const float* v3 = e3->b;
  const float* q = e4->a;
  if (v3 != q) return 0;
  const float* a = e1->a;
  const float* p = e3->a;
  const float fx = v3[0] - p[0], fy = v3[1] - p[1];
  const float* r = e4->b;
  auto side_q = [&](const float* u) { return (u[0] - q[0]) * (r[1] - q[1]) - (u[1] - q[1]) * (r[0] - q[0]) < 0.0f; };
  auto side_p = [&](const float* u) { return (u[0] - p[0]) * fy - (u[1] - p[1]) * fx; };
  const float rr = (r[1] - p[1]) * fx - (r[0] - p[0]) * fy;
  int s1;
  bool neg = side_q(a);
  if (0.0f <= side_p(a)) s1 = neg && rr < 0.0f ? 1 : -1;
  else s1 = neg || rr < 0.0f ? 1 : -1;
  const float* c = e2->b;
  neg = side_q(c);
  int s2;
  if (0.0f <= side_p(c)) s2 = neg && rr < 0.0f ? 1 : -1;
  else s2 = neg || rr < 0.0f ? 1 : -1;
  auto cross = [](const float* o, const float* u, const float* t) {
    return (u[0] - o[0]) * (t[1] - o[1]) - (u[1] - o[1]) * (t[0] - o[0]);
  };
  const bool tight = cross(a, p, v1) < 0.0f && cross(v2, p, c) < 0.0f && cross(a, r, v1) < 0.0f &&
                     cross(v2, r, c) < 0.0f;
  if (s1 == -1 && s2 == -1) return tight ? 6 : 5;
  if (s1 == 1 && s2 == 1) return tight ? 4 : 3;
  return 9 - (s1 != 1);
}

// Mech3.exe 1.2 @ 0x00510da0. R and R2 step round the region's two rings,
// P and P2 round the polygon's (by their links, so pieces split off are
// visited too, except the one just made when `skip`).
uint32_t walk(Weiler& w) {
  Edge* const r_first = w.contours.at(0).edge;
  Edge* r = r_first;
  Edge* r2 = w.contours.at(1).edge;
  Edge* p = w.contours.at(2).edge;
  Edge* p2 = w.contours.at(3).edge;
  uint32_t result = 0;
  bool skip = false;
  Crossing* x = nullptr;
  auto at_end = [&](Edge* e) { e->next->start = x, e->end = x; };      // x at e's end
  auto at_start = [&](Edge* e) { e->prev->end = x, e->start = x; };    // x at e's start
  auto shares = [](const Crossing* c, const Edge* e1, const Edge* e2) {
    return c && (c == e1->start || c == e1->end || c == e2->start || c == e2->end);
  };
  auto near = [](const float* u, const float* v) {
    return std::fabs(u[0] - v[0]) <= kSame && std::fabs(u[1] - v[1]) <= kSame;
  };
  do {
    Edge* const p_first = p;
    float* ra = r->a;
    float* rb = r->b;
    int32_t steps = 0;
    do {
      // FIX (bug): a polygon corner on a region edge to within a float's
      // error makes a new crossing each pass and the pieces' links cycle
      // past the ring's start: the exe loops (and allocates) forever. Give up
      // as its intersection errors do.
      if (++steps > 64 + 16 * w.edges.count) {
        zerr::report(0x100, kFile, 0, "Weiler walk gave up: the ring does not close");
        return 1;
      }
      float* pb = p->b;
      float* pa = p->a;
      if (r->divided) edge_box(*r);
      if (p->divided) edge_box(*p);
      if (p->box[2] < r->box[0] || r->box[2] < p->box[0] || p->box[3] < r->box[1] || r->box[3] < p->box[1] ||
          shares(p->start, r, r2) || shares(p->end, r, r2) || shares(p2->start, r, r2) || shares(p2->end, r, r2)) {
        p = p->next, p2 = p2->next;
        continue;
      }
      const uint32_t t = intersect(w, ra, rb, pa, pb, &x);
      if (t == 1) {
        zerr::report(0x100, kFile, 0x72b, "weilerIntersect Error: %s");
        return 1;
      }
      if (t != 0) {
        if (x) std::memset(x->links, 0, sizeof x->links);
        int s;
        switch (t) {
          case 3: {  // the two run along each other
            const int code = within_segment(ra, pa, pb) << 3 | within_segment(rb, pa, pb) << 2 |
                             within_segment(pa, ra, rb) << 1 | within_segment(pb, ra, rb);
            if (code == 5 || code == 6 || code == 9 || code == 10) {
              x = w.crossings.alloc(1);
              std::memset(x->links, 0, sizeof x->links);
            }
            if (code == 5) {
              if (p != p->next && near(p->b, p->next->a)) x->type = 0x14, p2->end = x, r->end = x;
              else x->type = 0x17, p->end = x, r2->end = x;
              std::memcpy(x->point, rb, 12);
            } else if (code == 6) {
              x->type = 0x11;
              std::memcpy(x->point, rb, 12);
              p2->start = x, r2->end = x;
            } else if (code == 9) {
              x->type = 0x15;
              std::memcpy(x->point, pb, 12);
              p2->end = x, r2->start = x;
            } else if (code == 10) {
              if (p != p->prev && near(p->a, p->prev->b)) x->type = 0xc, p2->start = x, r->start = x;
              else x->type = 0xf, p->start = x, r2->start = x;
              std::memcpy(x->point, ra, 12);
            }
            break;
          }
          case 4:
          case 5:  // across
            divide(w, x, r, 1), divide(w, x, r2, 1), divide(w, x, p, 1), divide(w, x, p2, 1);
            rb = r->b;
            skip = true;
            break;
          case 6:
            divide(w, x, p, 1), divide(w, x, p2, 0);
            skip = true;
            switch (corner_side(r->prev, r, p)) {
              case 0: r2->start = x, r->start = x; break;
              case 1: x->type = 10, at_end(p), at_start(r), r->flags |= 3, r->prev->flags |= 3; break;
              case 2: x->type = 10, at_end(p), at_start(r2), r->flags |= 3, r->prev->flags |= 3; break;
              case 7: x->type = 4, at_end(p2), at_start(r2), at_start(r); break;
            }
            break;
          case 7:
            divide(w, x, p, 0), divide(w, x, p2, 1);
            skip = true;
            switch (corner_side(r->prev, r, p2)) {
              case 0: r2->start = x, r->start = x; break;
              case 1: x->type = 0xb, at_end(p2), at_start(r); break;
              case 2: x->type = 0xb, at_end(p2), at_start(r2); break;
              case 7: x->type = 5, at_end(p), at_start(r2), at_start(r); break;
            }
            break;
          case 8:
            divide(w, x, p, 1), divide(w, x, p2, 0);
            skip = true;
            switch (corner_side(r, r->next, p)) {
              case 0: r2->end = x, r->end = x; break;
              case 1: x->type = 10, at_end(p), at_end(r), r->flags |= 3, r->next->flags |= 3; break;
              case 2: x->type = 10, at_end(p), at_end(r2), r->flags |= 3, r->next->flags |= 3; break;
              case 7: x->type = 5, at_end(p2), at_end(r2), at_end(r); break;
            }
            break;
          case 9:
            divide(w, x, p, 0), divide(w, x, p2, 1);
            skip = true;
            switch (corner_side(r, r->next, p2)) {
              case 0: r2->end = x, r->end = x; break;
              case 1: x->type = 0xb, at_end(p2), at_end(r); break;
              case 2: x->type = 0xb, at_end(p2), at_end(r2); break;
              case 7: x->type = 4, at_end(p), at_end(r2), at_end(r); break;
            }
            break;
          case 12:
            switch (s = corner_pair(p->prev, p, r->prev, r)) {
              case 0: p2->start = x, r->start = x; break;
              case 3: x->type = 0x18, at_start(p2), at_start(r); break;
              case 4: x->type = 0x18, at_start(p), at_start(r); break;
              case 8: case 9:
                x->type = s == 8 ? 5 : 4, at_start(r2), at_start(r), at_start(p2), at_start(p);
                break;
            }
            break;
          case 13:
            divide(w, x, r, 1), divide(w, x, r2, 0);
            switch (corner_side(p->prev, p, r2)) {
              case 0: p2->start = x, p->start = x; break;
              case 1: x->type = 0x18, at_end(r), at_start(p); break;
              case 2: x->type = 0x18, at_end(r), at_start(p2); break;
              case 7: x->type = 5, at_end(r2), at_start(p2), at_start(p); break;
            }
            rb = r->b;
            break;
          case 14:
            switch (s = corner_pair(p->prev, p, r2, r2->next)) {
              case 0: p->start = x, r->end = x; break;
              case 3: x->type = 0x18, at_start(p2), at_end(r); break;
              case 4: x->type = 0x18, at_start(p), at_end(r); break;
              case 8: case 9:
                x->type = s == 8 ? 5 : 4, at_end(r2), at_end(r), at_start(p2), at_start(p);
                break;
            }
            break;
          case 15:
            switch (s = corner_pair(p->prev, p, r2->prev, r2)) {
              case 0: p->start = x, r2->start = x; break;
              case 5: x->type = 0x19, at_start(p2), at_start(r2); break;
              case 6: x->type = 10, at_start(p), at_start(r2), r->flags |= 2, r->prev->flags |= 2; break;
              case 8: case 9:
                x->type = s == 8 ? 5 : 4, at_start(r2), at_start(r), at_start(p2), at_start(p);
                break;
            }
            break;
          case 16:
            divide(w, x, r, 0), divide(w, x, r2, 1);
            switch (corner_side(p->prev, p, r2)) {
              case 0: p2->start = x, p->start = x; break;
              case 1: x->type = 0x19, at_end(r2), at_start(p); break;
              case 2: x->type = 0x19, at_end(r2), at_start(p2); break;
              case 7: x->type = 4, at_end(r), at_start(p2), at_start(p); break;
            }
            rb = r->b;
            break;
          case 17:
            switch (s = corner_pair(p->prev, p, r2, r2->next)) {
              case 0: p2->start = x, r2->end = x; break;
              case 5: case 6: x->type = 0x19, at_end(r2), at_start(p2); break;
              case 8: case 9:
                x->type = s == 8 ? 5 : 4, at_end(r2), at_end(r), at_start(p2), at_start(p);
                break;
            }
            break;
          case 18:
            switch (s = corner_pair(p, p->next, r2->prev, r2)) {
              case 0: p->end = x, r->start = x; break;
              case 3: x->type = 0x18, at_end(p2), at_start(r); break;
              case 4: x->type = 0x18, at_end(p), at_start(r); break;
              case 8: case 9:
                x->type = s == 8 ? 5 : 4, at_start(r2), at_start(r), at_end(p2), at_end(p);
                break;
            }
            break;
          case 19:
            divide(w, x, r, 1), divide(w, x, r2, 0);
            switch (corner_side(p, p->next, r)) {
              case 0: p2->end = x, p->end = x; break;
              case 1: x->type = 0x18, at_end(r), at_end(p); break;
              case 2: x->type = 0x18, at_end(r), at_end(p2); break;
              case 7: x->type = 4, at_end(r2), at_end(p2), at_end(p); break;
            }
            rb = r->b;
            break;
          case 20:
            switch (s = corner_pair(p, p->next, r2, r2->next)) {
              case 0: p2->end = x, r->end = x; break;
              case 3: x->type = 0x18, at_end(p2), at_end(r); break;
              case 4: x->type = 0x18, at_end(p), at_end(r); break;
              case 8: case 9:
                x->type = s == 8 ? 5 : 4, at_end(r2), at_end(r), at_end(p2), at_end(p);
                break;
            }
            break;
          case 21:
            switch (s = corner_pair(p, p->next, r2->prev, r2)) {
              case 0: p2->end = x, r2->start = x; break;
              case 3: x->type = 0x18, at_end(p2), at_start(r); break;
              case 4: x->type = 0x18, at_end(p), at_start(r); break;
              case 8: case 9:
                x->type = s == 8 ? 5 : 4, at_start(r2), at_start(r), at_end(p2), at_end(p);
                break;
            }
            break;
          case 22:
            divide(w, x, r, 0), divide(w, x, r2, 1);
            switch (corner_side(p, p->next, r2)) {
              case 0: p2->end = x, p->end = x; break;
              case 1: x->type = 0x19, at_end(r2), at_end(p); break;
              case 2: x->type = 0x19, at_end(r2), at_end(p2); break;
              case 7: x->type = 5, at_end(r), at_end(p2), at_end(p); break;
            }
            rb = r->b;
            break;
          case 23:
            switch (s = corner_pair(p, p->next, r2, r2->next)) {
              case 0: p->end = x, r2->end = x; break;
              case 5: x->type = 0x19, at_end(r2), at_end(p2); break;
              case 6: x->type = 10, at_end(r2), at_end(p), r->flags |= 2, r->next->flags |= 2; break;
              case 8: case 9:
                x->type = s == 8 ? 5 : 4, at_end(r2), at_end(r), at_end(p2), at_end(p);
                break;
            }
            break;
          default:
            break;
        }
        if (skip) p = p->next, p2 = p2->next, skip = false;
      }
      result |= t;
      p = p->next, p2 = p2->next;
    } while (p != p_first);
    r = r->next, r2 = r2->next;
  } while (r != r_first);
  if (w.crossings.count != 0) {
    Edge* e = r_first;
    do {
      if (e->start) e->start->links[2] = e;
      if (e->end) e->end->links[0] = e;
      if (r2->start) r2->start->links[3] = r2;
      if (r2->end) r2->end->links[1] = r2;
      r2 = r2->next, e = e->next;
    } while (e != r_first);
    Edge* const p_first = p;
    e = p;
    do {
      if (e->start) e->start->links[6] = e;
      if (e->end) e->end->links[4] = e;
      if (p2->start) p2->start->links[7] = p2;
      if (p2->end) p2->end->links[5] = p2;
      p2 = p2->next, e = e->next;
    } while (e != p_first);
  }
  return result;
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
