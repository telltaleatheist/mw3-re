#include "gamez/zgeometry/zgeo_convexify.h"

#include <cstdio>
#include <cstring>

#pragma STDC FP_CONTRACT OFF

namespace zgeo {
namespace {
// An edge of 0x00515be0's table (0x0c bytes): its two points and how many
// more triangles may use it (1 for the outlines, 2 for the bridges).
struct RingEdge {
  int32_t a, b, uses;
};

// The exe's "equal to 0" tests (fcom, test ah, 0x40) take NaN as 0.
bool zeroish(float v) { return !(v < 0.0f || v > 0.0f); }

// Mech3.exe 1.2 @ 0x00515990. A B and C D cross strictly inside both.
int segments_cross(const float* a, const float* b, const float* c, const float* d) {
  const float den = (c[1] - d[1]) * (b[0] - a[0]) - (b[1] - a[1]) * (c[0] - d[0]);
  if (den == 0.0f) return 0;
  const float t = ((c[0] - a[0]) * (c[1] - d[1]) - (c[1] - a[1]) * (c[0] - d[0])) / den;
  const float u = ((c[1] - a[1]) * (b[0] - a[0]) - (c[0] - a[0]) * (b[1] - a[1])) / den;
  return 0.0f < t && 0.0f < u && t < 1.0f && u < 1.0f;
}

// Mech3.exe 1.2 @ 0x00515ae0. The edge a b (either way) when it may still
// be used; the first match decides.
RingEdge* find_edge(int32_t a, int32_t b, std::vector<RingEdge>& edges) {
  for (RingEdge& e : edges) {
    if ((e.a == a && e.b == b) || (e.b == a && e.a == b)) return e.uses ? &e : nullptr;
  }
  return nullptr;
}

// Mech3.exe 1.2 @ 0x005158c0. The bridge added unless it is there or
// crosses an edge that does not share a point with it.
void add_bridge(const RingEdge& bridge, std::vector<RingEdge>& edges, const std::vector<float>& pts) {
  if (find_edge(bridge.a, bridge.b, edges)) return;
  const float* a = &pts[3 * static_cast<size_t>(bridge.a)];
  const float* b = &pts[3 * static_cast<size_t>(bridge.b)];
  for (const RingEdge& e : edges) {
    if (e.a == bridge.a || e.b == bridge.a || e.a == bridge.b || e.b == bridge.b) continue;
    if (segments_cross(a, b, &pts[3 * static_cast<size_t>(e.a)], &pts[3 * static_cast<size_t>(e.b)])) return;
  }
  edges.push_back(bridge);
}
}  // namespace

// Mech3.exe 1.2 @ 0x00515f10 (0x00515f00 puts it in the exe's global at
// 0x00610468). The length is the float's bits halved plus 0x1fc00000.
void plane_of(const float* p, int32_t n, float plane[4]) {
  float nx = 0.0f, ny = 0.0f, nz = 0.0f, sx = 0.0f, sy = 0.0f, sz = 0.0f;
  for (int32_t i = 0; i < n; ++i) {
    const float* c = p + 3 * i;
    const float* q = p + 3 * ((i + 1) % n);
    nx = (c[1] - q[1]) * (q[2] + c[2]) + nx;
    ny = (c[2] - q[2]) * (q[0] + c[0]) + ny;
    nz = (c[0] - q[0]) * (q[1] + c[1]) + nz;
    sx = sx + c[0], sy = sy + c[1], sz = sz + c[2];
  }
  float len = 0.0f;
  if (!zeroish(nx) || !zeroish(ny) || !zeroish(nz)) {
    const float sq = nx * nx + ny * ny + nz * nz;
    int32_t bits;
    std::memcpy(&bits, &sq, 4);
    bits = (bits >> 1) + 0x1fc00000;
    std::memcpy(&len, &bits, 4);
  }
  const float inv = zeroish(len) ? 0.0f : 1.0f / len;
  plane[0] = inv * nx, plane[1] = inv * ny, plane[2] = inv * nz;
  const float dot = sx * nx + sy * ny + sz * nz;
  plane[3] = -(dot / (static_cast<float>(n) * len));
}

// Mech3.exe 1.2 @ 0x005160e0
void plane_z(const float plane[4], float* p, int32_t n) {
  for (int32_t i = 0; i < n; ++i, p += 3) p[2] = -((p[0] * plane[0] + p[1] * plane[1] + plane[3]) / plane[2]);
}

// Mech3.exe 1.2 @ 0x00516120
void reverse_points(float* p, int32_t n) {
  float* a = p;
  float* b = p + 3 * n - 3;
  for (int32_t k = n / 2; k != 0; --k) {
    a += 3;
    for (int i = 0; i < 3; ++i) {
      const float t = b[i];
      b[i] = a[i], a[i] = t;
    }
    b -= 3;
  }
}

// Mech3.exe 1.2 @ 0x00516190
int face_up(float* p, int32_t n, int turn) {
  const float ux = p[3] - p[0], uy = p[4] - p[1];
  const float vx = p[6] - p[3], vy = p[7] - p[4];
  if (!(ux * vy - uy * vx > 0.0f)) {  // NaN too, as the exe's test ah, 0x41
    if (!turn) return 0;
    reverse_points(p, n);
  }
  return 1;
}

// Mech3.exe 1.2 @ 0x00515be0. The polygon's and region's points together
// (the region's z on the polygon's plane, written back to the caller's
// buffer); the two outlines' edges, then every polygon-to-region bridge
// that crosses nothing; then at each point every pair of its usable edges
// whose far ends are joined makes a triangle, each edge used up as it
// goes. The exe's triangles live in a global (0x0060d460, the count at
// 0x00610460).
RingTriangles ring_triangles(int32_t np, const float* poly, int32_t nr, float* region) {
  const int32_t n = np + nr;
  std::vector<float> pts(3 * static_cast<size_t>(n));
  std::memcpy(pts.data(), poly, 12 * static_cast<size_t>(np));
  std::memcpy(pts.data() + 3 * np, region, 12 * static_cast<size_t>(nr));
  float plane[4];
  plane_of(poly, np, plane);
  plane_z(plane, pts.data() + 3 * np, nr);
  std::memcpy(region, pts.data() + 3 * np, 12 * static_cast<size_t>(nr));
  std::vector<RingEdge> edges;
  edges.reserve(static_cast<size_t>(n) * static_cast<size_t>(n));
  edges.push_back({0, np - 1, 1});
  for (int32_t i = 1; i < np; ++i) edges.push_back({i - 1, i, 1});
  edges.push_back({np, n - 1, 1});
  for (int32_t i = np + 1; i < n; ++i) edges.push_back({i - 1, i, 1});
  for (int32_t i = 0; i < np; ++i)
    for (int32_t j = 0; j < nr; ++j) add_bridge({i, np + j, 2}, edges, pts);
  // FIX (bug): the exe lists a point's edges in 32 ints on the stack
  // (0x00515aa0 writes past them for a point with more).
  std::vector<int32_t> at;
  std::vector<int32_t> tris;  // the exe's holds 1,024 triangles; a ring makes np + nr, far fewer
  for (int32_t k = 0; k < n; ++k) {
    at.clear();
    for (size_t e = 0; e < edges.size(); ++e)  // 0x00515aa0
      if (edges[e].uses && (edges[e].a == k || edges[e].b == k)) at.push_back(static_cast<int32_t>(e));
    for (size_t i = 0; i < at.size(); ++i) {
      for (size_t j = 0; j < at.size(); ++j) {
        if (i == j) continue;
        // Mech3.exe 1.2 @ 0x00515b30. The triangle of points 0, 1, 2 is
        // never made (its sum is tested against 3).
        RingEdge& e1 = edges[static_cast<size_t>(at[i])];
        RingEdge& e2 = edges[static_cast<size_t>(at[j])];
        if (!e1.uses || !e2.uses) continue;
        const int32_t o1 = e1.a == k ? e1.b : e1.a;
        const int32_t o2 = e2.a == k ? e2.b : e2.a;
        if (o2 + o1 + k == 3) continue;
        RingEdge* third = find_edge(o1, o2, edges);
        if (!third) continue;
        tris.push_back(o1), tris.push_back(o2), tris.push_back(k);
        --e1.uses, --e2.uses, --third->uses;
      }
    }
  }
  RingTriangles out;
  out.count = static_cast<int32_t>(tris.size() / 3);
  out.points.reserve(3 * tris.size());
  for (int32_t v : tris) out.points.insert(out.points.end(), &pts[3 * static_cast<size_t>(v)], &pts[3 * static_cast<size_t>(v)] + 3);
  return out;
}

// Mech3.exe 1.2 @ 0x00516a50. x is f[v[0]]; the leftmost search and the
// y range take y as f[v[1]], the inside tests as f[v[0] + 1].
int split_polygon(const int32_t* v, int32_t n, int32_t s, const float* f, std::vector<int32_t>& out) {
  auto V = [&](int32_t k) { return v + static_cast<ptrdiff_t>(k) * s; };
  auto X = [&](int32_t k) { return f[V(k)[0]]; };
  auto Y = [&](int32_t k) { return f[V(k)[1]]; };
  auto Y1 = [&](int32_t k) { return f[V(k)[0] + 1]; };
  int32_t best = 0;
  float bx = X(0);
  for (int32_t k = 1; k < n; ++k) {
    if (X(k) < bx || (X(k) == bx && Y(k) < Y(best))) bx = X(k), best = k;
  }
  const int32_t prev = best == 0 ? n - 1 : best - 1;
  const int32_t next = best == n - 1 ? 0 : best + 1;
  float lo, hi;
  if (Y(prev) <= Y(next)) lo = Y(prev), hi = Y(next);
  else lo = Y(next), hi = Y(prev);
  if (hi < Y(best)) hi = Y(best);
  else if (lo > Y(best)) lo = Y(best);
  const int32_t bound = X(prev) > X(next) ? prev : next;
  bool found = false;
  float nearest = 1e7f;
  int32_t inside = 0;
  for (int32_t k = 0; k < n; ++k) {
    if (k == prev || k == best || k == next) continue;
    if (!(Y(k) <= hi) || !(lo <= Y(k)) || !(X(k) < X(bound))) continue;
    const float qx = X(k), qy = Y1(k);
    const float a = X(best) - qx, b = X(next) - qx, c = Y1(next) - qy, d = Y1(best) - qy;
    const float e = X(prev) - qx, g = Y1(prev) - qy;
    if (!(b * d < c * a) || !(e * c <= g * b) || !(g * a < e * d)) continue;
    const float y = Y(best) - Y(k);
    const float dist = a * a + y * y;
    if (dist < nearest) found = true, nearest = dist, inside = k;
  }
  int32_t from = best, to = inside;
  if (!found) to = bound, from = bound == prev ? next : prev;
  const int32_t lo_k = from < to ? from : to, hi_k = from < to ? to : from;
  const int32_t n1 = hi_k - lo_k + 1;
  out.assign(2, 0);
  out[0] = n1, out[1] = n - n1 + 2;
  out.insert(out.end(), V(lo_k), V(hi_k + 1));
  out.insert(out.end(), V(hi_k), V(n));
  out.insert(out.end(), V(0), V(lo_k + 1));
  return 1;
}

// Mech3.exe 1.2 @ 0x005166c0. A triangle as given comes back with the
// first indices 0..3 * stride - 1, whatever idx says (only the top call
// can have 3 vertices, and convexify never sends one).
std::vector<int32_t> triangulate(const float* f, const int32_t* idx, int32_t n, int32_t flag) {
  if (n < 3) {
    std::fprintf(stderr, "Error in TRIANGULATE: only %d verts received\n", n);
    return {};
  }
  const int32_t s = (flag != 1) + 2;
  if (n == 3) {
    std::vector<int32_t> r(1 + 3 * static_cast<size_t>(s));
    r[0] = 1;
    for (int32_t i = 0; i < 3 * s; ++i) r[1 + static_cast<size_t>(i)] = i;
    return r;
  }
  std::vector<int32_t> verts(static_cast<size_t>(s) * static_cast<size_t>(n));
  for (size_t i = 0; i < verts.size(); ++i) verts[i] = idx ? idx[i] : static_cast<int32_t>(i);
  std::vector<int32_t> halves;
  if (!split_polygon(verts.data(), n, s, f, halves)) return {};
  const int32_t n1 = halves[0], n2 = halves[1];
  const int32_t* p1 = halves.data() + 2;
  const int32_t* p2 = p1 + static_cast<ptrdiff_t>(n1) * s;
  std::vector<int32_t> r(1);
  r[0] = n - 2;
  auto put = [&](const int32_t* p, size_t count) { r.insert(r.end(), p, p + count); };
  auto sub = [&](const int32_t* p, int32_t m, const char* error) {
    std::vector<int32_t> t = triangulate(f, p, m, flag);
    if (t.empty()) std::fprintf(stderr, "%s", error);
    else put(t.data() + 1, static_cast<size_t>(t[0]) * 3 * static_cast<size_t>(s));
    return !t.empty();
  };
  const size_t tri = 3 * static_cast<size_t>(s);
  if (n1 == 3 && n2 == 3) {
    put(p1, tri), put(p2, tri);
  } else if (n1 == 3) {
    put(p1, tri);
    if (!sub(p2, n2, "Error in recursive triangulate 1\n")) return {};
  } else if (n2 == 3) {
    put(p2, tri);
    if (!sub(p1, n1, "Error in recursive triangulate 2\n")) return {};
  } else {
    if (!sub(p1, n1, "Error in recursive triangulate 3\n")) return {};
    if (!sub(p2, n2, "Error in recursive triangulate 4\n")) return {};
  }
  return r;
}

// Mech3.exe 1.2 @ 0x005162d0
int convexify(const ResultContour* list, int32_t count, int32_t total, const float* points, Convex& out) {
  out.polys.clear(), out.points.clear();
  if (total < 1 || !points) {
    std::fprintf(stderr, "convexify(): One or more inputs are null\n");
    return 0;
  }
  auto add = [&](const float* p, int32_t m) {
    out.polys.push_back(ResultContour{m, static_cast<int32_t>(out.points.size())});
    out.points.insert(out.points.end(), p, p + 3 * m);
  };
  for (int32_t i = 0; i < count; ++i) {
    const ResultContour& c = list[i];
    const float* p = points + c.start;
    if (c.count < 3) continue;
    if (c.count == 3) {
      add(p, 3);
    } else if (c.count == 4) {
      int32_t reflex = -1;
      for (int32_t k = 0; k < 4; ++k) {
        const float* a = p + 3 * k;
        const float* b = p + 3 * ((k + 1) % 4);
        const float* d = p + 3 * ((k + 2) % 4);
        if ((d[0] - a[0]) * (b[1] - a[1]) - (b[0] - a[0]) * (d[1] - a[1]) > 0.0f) {
          reflex = (k + 1) % 4;
          break;
        }
      }
      if (reflex < 0) {
        add(p, 4);
      } else if (reflex & 1) {  // corner 1 or 3: cut 1-3
        float t[9];
        std::memcpy(t, p, 24), std::memcpy(t + 6, p + 9, 12);
        add(t, 3), add(p + 3, 3);
      } else {  // corner 0 or 2: cut 0-2
        float t[9];
        add(p, 3);
        std::memcpy(t, p, 12), std::memcpy(t + 3, p + 6, 24);
        add(t, 3);
      }
    } else {
      const std::vector<int32_t> tris = triangulate(p, nullptr, c.count, 0);
      for (int32_t k = 0; k < tris[0]; ++k) {
        float t[9];
        for (int i = 0; i < 9; ++i) t[i] = p[tris[1 + 9 * static_cast<size_t>(k) + static_cast<size_t>(i)]];
        add(t, 3);
      }
    }
  }
  return 1;
}

}  // namespace zgeo
