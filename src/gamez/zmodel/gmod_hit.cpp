#include "gamez/zmodel/gmod_hit.h"

#include <cmath>
#include <cstring>
#include <vector>

#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmath/zmth_vector.h"

#pragma STDC FP_CONTRACT OFF

namespace zmod {

namespace {
// The scratch buffers: the morphed vertices (0x0074a288), the vertices
// through the matrix (0x0074a28c) and one polygon's (0x007473a4). They keep
// what the last model left past its own count, as the exe's do.
struct Scratch {
  std::vector<float> morphed, world;
  float poly[256 * 3];
};
Scratch g_s;

uint32_t hdr(const Model& m, int off) {
  uint32_t v;
  std::memcpy(&v, m.header + off, 4);
  return v;
}

float* sized(std::vector<float>& v, uint32_t n) {
  if (v.size() < n) v.resize(n);
  return v.data();
}

// The model's vertices through the top of the matrix stack (0x0054c655..0x0054c704).
const float* place_vertices(Model& m) {
  const float* src = m.vertices;
  float f;
  std::memcpy(&f, m.header + 0x24, 4);
  if ((hdr(m, 8) & 8) && f != 0.0f && hdr(m, 0x1c) != 0) {
    float* dst = sized(g_s.morphed, 3 * hdr(m, 0x1c));
    zmth::morph(reinterpret_cast<zmth::Vec3*>(dst), reinterpret_cast<const zmth::Vec3*>(m.vertices),
                reinterpret_cast<const zmth::Vec3*>(m.morphs), hdr(m, 0x1c), f);
    src = dst;
  }
  const uint32_t n = hdr(m, 0x14);
  float* out = sized(g_s.world, 3 * n);
  zmth::MatrixStack& s = zmth::matrix_stack();
  if (s.is_identity()) {
    std::memcpy(out, src, 12 * n);
    return out;
  }
  const float* t = s.matrix().m;
  for (uint32_t i = 0; i < n; ++i, src += 3) {
    const float x = src[0], y = src[1], z = src[2];
    out[3 * i] = ((x * t[0] + y * t[3]) + z * t[6]) + t[9];
    out[3 * i + 1] = ((x * t[1] + y * t[4]) + z * t[7]) + t[10];
    out[3 * i + 2] = ((x * t[2] + y * t[5]) + z * t[8]) + t[11];
  }
  return out;
}

void copy_vertex(float* dst, const float* world, uint32_t index) { std::memcpy(dst, world + 3 * index, 12); }

// One polygon of the model: a strip is tested as its triangles (the count
// byte set to 3 while they are), a plain polygon whole. `degenerate` is
// what a strip of two vertices or fewer leaves as the result.
int32_t test_polygon(zcls::Node* node, Polygon& p, const float* world, const float* probes, const int32_t* mask,
                     int32_t count, float top, zcls::GroundHits* out, int32_t degenerate) {
  const uint32_t flags = p.flags;
  const uint32_t n = flags & 0xff;
  if (!(flags & 0x400)) {
    for (uint32_t i = 0; i < n; ++i) copy_vertex(g_s.poly + 3 * i, world, p.vertex_indices[i]);
    return polygon_ground(node, out, probes, mask, count, top, g_s.poly, p);
  }
  p.flags = (flags & ~0xffu) | 3;
  int32_t r = degenerate;
  if (static_cast<int32_t>(n) - 2 > 0) {
    for (uint32_t s = 2; static_cast<int32_t>(s - 2) < static_cast<int32_t>(n) - 2; ++s) {
      copy_vertex(g_s.poly, world, p.vertex_indices[s - 2]);
      copy_vertex(g_s.poly + 3, world, p.vertex_indices[(s - 1) | 1]);
      copy_vertex(g_s.poly + 6, world, p.vertex_indices[s & ~1u]);
      r = polygon_ground(node, out, probes, mask, count, top, g_s.poly, p);
      if (r) break;
    }
  }
  p.flags = (p.flags & ~0xffu) | n;
  return r;
}
}  // namespace

namespace {
UvCapture g_uv;

// The x87 branch tests: `less` is C0 (also set when unordered), `at_most`
// C0 or C3.
bool less(float a, float b) { return !(a >= b); }
bool at_most(float a, float b) { return !(a > b); }

bool same_sign(float a, float b) {
  uint32_t x, y;
  std::memcpy(&x, &a, 4);
  std::memcpy(&y, &b, 4);
  return ((x ^ y) & 0x80000000u) == 0;
}

float dot_from(const float* p, const float* v0, const float* n) {
  const float dx = p[0] - v0[0], dy = p[1] - v0[1], dz = p[2] - v0[2];
  return (dx * n[0] + dy * n[1]) + dz * n[2];
}

// 0x0054e2b0 and 0x0054ea30: the same test; the textured one differs in
// the one-side case and records texture coordinates.
int32_t segments_core(zcls::Node* node, zcls::SegmentHits* out, const float* segs, const int32_t* mask,
                      int32_t count, const float* v, const Polygon& poly, int32_t* all, bool textured,
                      const float* uvs, float* uv_out) {
  *all = 1;
  int32_t in[24];
  for (int32_t k = count - 1; k >= 0; --k) in[k] = mask[k];
  float n[3];
  zmth::poly_normal(v, v + 3, v + 6, n);
  int32_t crossed = 0;
  for (int32_t k = 0; k < count; ++k) {
    const float* from = segs + 6 * k;
    const float* to = from + 3;
    if (!in[k]) {
      *all = 0;
      continue;
    }
    const float e1 = dot_from(to, v, n);
    if (!(poly.flags & 0x100) && !less(e1, 0.0f)) {
      in[k] = 0;
      *all = 0;
      continue;
    }
    const float e0 = dot_from(from, v, n);
    if (same_sign(e0, e1)) {
      in[k] = 0;
      if (textured) *all = 0;
      continue;
    }
    crossed = 1;
    const float t = e0 / (e0 - e1);
    const float dx = t * (to[0] - from[0]), dy = t * (to[1] - from[1]), dz = t * (to[2] - from[2]);
    zcls::SegmentHit& h = out[k].hit[out[k].count];
    h.point[0] = from[0] + dx;
    h.point[1] = from[1] + dy;
    h.point[2] = from[2] + dz;
  }
  if (!crossed) return 0;
  // The polygon is projected on the plane its normal points most along.
  int axis = 0;
  float m = std::fabs(n[0]);
  if (!at_most(std::fabs(n[1]), m)) m = std::fabs(n[1]), axis = 1;
  if (!at_most(std::fabs(n[2]), m)) axis = 2;
  int32_t any = 1;
  int32_t j = 0;
  for (int32_t i = static_cast<int32_t>(poly.flags & 0xff) - 1; i >= 0; j = i, --i) {
    if (!any) return 0;
    const float* c = v + 3 * i;
    const float* a = v + 3 * j;
    float p, q;
    int u0, u1;  // the two coordinates tested
    if (axis == 2) {
      if (!at_most(n[2], 0.0f)) p = c[1] - a[1], q = a[0] - c[0];
      else p = a[1] - c[1], q = c[0] - a[0];
      u0 = 0, u1 = 1;
    } else if (axis == 1) {
      if (!at_most(n[1], 0.0f)) q = c[0] - a[0], p = a[2] - c[2];
      else q = a[0] - c[0], p = c[2] - a[2];
      u0 = 0, u1 = 2;
    } else {
      if (less(n[0], 0.0f)) q = c[1] - a[1], p = a[2] - c[2];
      else q = a[1] - c[1], p = c[2] - a[2];
      u0 = 1, u1 = 2;
    }
    any = 0;
    for (int32_t k = 0; k < count; ++k) {
      if (!in[k]) continue;
      const float* h = out[k].hit[out[k].count].point;
      const float s = (h[u0] - c[u0]) * p + (h[u1] - c[u1]) * q;
      in[k] = s > -0.0001;  // 0x005ce518
      any |= in[k];
      *all &= in[k];
    }
  }
  if (!any || count <= 0) return any;
  const int a0 = axis == 0 ? 1 : 0, a1 = axis == 2 ? 1 : 2;
  float gu[2] = {}, gv[2] = {};
  if (textured && g_uv.on) {
    zmth::plane_gradient(v[a0], v[a1], v[3 + a0], v[3 + a1], v[6 + a0], v[6 + a1], uvs[0], uvs[2], uvs[4], &gu[0],
                         &gu[1]);
    zmth::plane_gradient(v[a0], v[a1], v[3 + a0], v[3 + a1], v[6 + a0], v[6 + a1], uvs[1], uvs[3], uvs[5], &gv[0],
                         &gv[1]);
  }
  for (int32_t k = 0; k < count; ++k) {
    if (!in[k] || out[k].count >= 0x20) continue;
    zcls::SegmentHit& h = out[k].hit[out[k].count];
    if (textured && g_uv.on) {
      uv_out[0] = (h.point[a0] - v[a0]) * gu[0] + (h.point[a1] - v[a1]) * gu[1] + uvs[0];
      uv_out[1] = (h.point[a0] - v[a0]) * gv[0] + (h.point[a1] - v[a1]) * gv[1] + uvs[1];
      g_uv.uv[0] = uv_out[0];
      g_uv.uv[1] = uv_out[1];
    }
    std::memcpy(h.normal, n, 12);
    h.node = node;
    h.material = poly.material;
    ++out[k].count;
  }
  return any;
}
}  // namespace

UvCapture& uv_capture() { return g_uv; }

// Mech3.exe 1.2 @ 0x005426a0
void mark_set_type(int32_t type) { g_uv.type = type; }

// Mech3.exe 1.2 @ 0x005426b0. FIX (bug): a type past 2 is ignored; the exe
// writes over the coordinates (3, 4) and the globals after them.
void mark_set_map(zimg::TextureEntry* image) {
  g_uv.on = 1;
  if (g_uv.type >= 0 && g_uv.type < 3) g_uv.maps[g_uv.type] = image;
}

// Mech3.exe 1.2 @ 0x005426d0
int32_t mark_on() { return g_uv.on; }

// Mech3.exe 1.2 @ 0x005426e0
void mark_set_uv(float u, float v) { g_uv.uv[0] = u, g_uv.uv[1] = v; }

// Mech3.exe 1.2 @ 0x00542700
void mark_enable(int32_t on) { g_uv.on = on; }

// Mech3.exe 1.2 @ 0x00542710. A null entry matches an empty slot.
bool mark_is_map(const zimg::TextureEntry* e) {
  for (const zimg::TextureEntry* m : g_uv.maps)
    if (m == e) return true;
  return false;
}

// Mech3.exe 1.2 @ 0x0054e2b0
int32_t polygon_segments(zcls::Node* node, zcls::SegmentHits* out, const float* segs, const int32_t* mask,
                         int32_t count, const float* verts, const Polygon& poly, int32_t* all) {
  return segments_core(node, out, segs, mask, count, verts, poly, all, false, nullptr, nullptr);
}

// Mech3.exe 1.2 @ 0x0054ea30
void polygon_segments_uv(zcls::Node* node, zcls::SegmentHits* out, const float* segs, const int32_t* mask,
                         int32_t count, const float* verts, const float* uvs, float uv_out[2], const Polygon& poly,
                         int32_t* all) {
  segments_core(node, out, segs, mask, count, verts, poly, all, true, uvs, uv_out);
}

// Mech3.exe 1.2 @ 0x0054fa10. The faces in the exe's order; the quad's
// flags keep only the 4 (one-sided, not a strip) and its material is 0.
int32_t box_segments(zcls::Node* node, zcls::SegmentHits* out, const float* segs, const int32_t* mask,
                     int32_t count, const float c[8][3]) {
  static const int kFaces[6][4] = {{0, 4, 7, 3}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {0, 3, 2, 1}, {4, 5, 6, 7}};
  Polygon quad{};
  quad.flags = 4;
  quad.material = nullptr;
  int32_t any = 0;
  for (const auto& f : kFaces) {
    for (int i = 0; i < 4; ++i) std::memcpy(g_s.poly + 3 * i, c[f[i]], 12);
    int32_t all = 0;
    if (polygon_segments(node, out, segs, mask, count, g_s.poly, quad, &all)) {
      if (all) return 1;
      any = 1;
    }
  }
  return any;
}

// Mech3.exe 1.2 @ 0x0054f450. A strip's triangles carry their own corners'
// texture coordinates. A strip of two vertices or fewer at the cached index
// reads an unset stack slot as its result in the exe; 0 here.
void model_segments(zcls::Node* node, Model* model, const float* segs, const int32_t* mask, int32_t count,
                    zcls::SegmentHits* out) {
  if (!model || hdr(*model, 0x10) == 0) return;
  const float* world = place_vertices(*model);
  float uv_out[2];
  auto test = [&](Polygon& p) {
    int32_t all = 0;
    const uint32_t flags = p.flags;
    const uint32_t n = flags & 0xff;
    const bool textured = p.material && (p.material->flags & 1);
    if (!(flags & 0x400)) {
      for (uint32_t i = 0; i < n; ++i) copy_vertex(g_s.poly + 3 * i, world, p.vertex_indices[i]);
      if (textured) polygon_segments_uv(node, out, segs, mask, count, g_s.poly, p.uvs, uv_out, p, &all);
      else polygon_segments(node, out, segs, mask, count, g_s.poly, p, &all);
      return all;
    }
    p.flags = (flags & ~0xffu) | 3;
    if (static_cast<int32_t>(n) - 2 > 0) {
      for (uint32_t s = 2; static_cast<int32_t>(s - 2) < static_cast<int32_t>(n) - 2; ++s) {
        const uint32_t k[3] = {s - 2, (s - 1) | 1, s & ~1u};
        for (int i = 0; i < 3; ++i) copy_vertex(g_s.poly + 3 * i, world, p.vertex_indices[k[i]]);
        if (textured) {
          float uv[6];
          for (int i = 0; i < 3; ++i) uv[2 * i] = p.uvs[2 * k[i]], uv[2 * i + 1] = p.uvs[2 * k[i] + 1];
          polygon_segments_uv(node, out, segs, mask, count, g_s.poly, uv, uv_out, p, &all);
        } else {
          polygon_segments(node, out, segs, mask, count, g_s.poly, p, &all);
        }
        if (all) break;
      }
    }
    p.flags = (p.flags & ~0xffu) | n;
    return all;
  };
  if (test(model->polygons[hdr(*model, 0x58)])) return;
  int32_t r = 0;
  for (uint32_t i = 0; i < hdr(*model, 0x10) && !r; ++i) {
    if (i == hdr(*model, 0x58)) continue;
    r = test(model->polygons[i]);
    if (r) std::memcpy(model->header + 0x58, &i, 4);
  }
}

// Mech3.exe 1.2 @ 0x0054c330. Inside means on the inner side of every edge,
// walked backwards, within 0.0001; the height is the polygon's plane at the
// probe. A record's next slot gets the normal and height even when the
// probe is above top (and so not counted).
int32_t polygon_ground(zcls::Node* node, zcls::GroundHits* out, const float* probes, const int32_t* mask,
                       int32_t count, float top, const float* v, const Polygon& poly) {
  int32_t all_in = 1, any = 1;
  float n[3];
  zmth::poly_normal(v, v + 3, v + 6, n);
  if (!(n[1] > 0.0f)) return 0;
  int32_t in[24];
  for (int32_t k = count - 1; k >= 0; --k) in[k] = mask[k];
  int32_t i = static_cast<int32_t>(poly.flags & 0xff) - 1;
  int32_t j = 0;
  for (; i >= 0 && any; j = i, --i) {
    const float* c = v + 3 * i;
    const float* a = v + 3 * j;
    const float ax = c[0] - a[0];
    const float bz = a[2] - c[2];
    any = 0;
    for (int32_t k = 0; k < count; ++k) {
      if (!in[k]) {
        all_in = 0;
        continue;
      }
      const float* p = probes + 3 * k;
      const float s = (p[0] - c[0]) * bz + (p[2] - c[2]) * ax;
      in[k] = s > -0.0001;  // 0x005ce518
      if (in[k]) any = 1;
      else all_in = 0;
    }
  }
  if (!any || count <= 0) return all_in;
  bool first = true;
  float fx = 0, fz = 0;
  for (int32_t k = 0; k < count; ++k) {
    if (!in[k]) {
      all_in = 0;
      continue;
    }
    zcls::GroundHits& rec = out[k];
    if (rec.count >= 0x20) continue;
    zcls::GroundHit& h = rec.hit[rec.count];
    std::memcpy(h.normal, n, 12);
    if (first) {
      first = false;
      const float inv = 1.0f / n[1];
      fx = -(n[0] * inv);
      fz = -(n[2] * inv);
    }
    const float* p = probes + 3 * k;
    h.height = ((p[2] - v[2]) * fz + (p[0] - v[0]) * fx) + v[1];
    if (h.height > top) {
      all_in = 0;
      continue;
    }
    h.node = node;
    h.zone[0] = poly.zone_set;
    h.material = poly.material;
    ++rec.count;
  }
  return all_in;
}

// Mech3.exe 1.2 @ 0x0054c5f0
void model_ground(zcls::Node* node, Model* model, const float* probes, const int32_t* mask, int32_t count,
                  float top, zcls::GroundHits* out) {
  if (!model || hdr(*model, 0x10) == 0) return;
  const float* world = place_vertices(*model);
  int32_t top_bits;
  std::memcpy(&top_bits, &top, 4);
  const uint32_t cached = hdr(*model, 0x58);
  if (test_polygon(node, model->polygons[cached], world, probes, mask, count, top, out, top_bits)) return;
  int32_t r = 0;
  for (uint32_t i = 0; i < hdr(*model, 0x10) && !r; ++i) {
    if (i == hdr(*model, 0x58)) continue;
    r = test_polygon(node, model->polygons[i], world, probes, mask, count, top, out, 0);
    if (r) std::memcpy(model->header + 0x58, &i, 4);
  }
}

// Mech3.exe 1.2 @ 0x0054d640. Inside means on the inner side of every
// edge, walked backwards from the closing one, within 0.0001.
int32_t polygon_ground_point(zcls::GroundHit* slot, const float* v, float x, float z, int32_t n) {
  for (int32_t i = n - 1; i >= 0; --i) {
    const float* c = v + 3 * i;
    const float* a = i == n - 1 ? v : v + 3 * (i + 1);
    const float e = (x - c[0]) * (a[2] - c[2]) + (z - c[2]) * (c[0] - a[0]);
    if (!(e > -0.0001)) return 0;  // 0x005ce518
  }
  zmth::poly_normal(v, v + 3, v + 6, slot->normal);
  if (slot->normal[1] == 0.0f) {
    slot->height = v[1];
    return 1;
  }
  const float inv = 1.0f / slot->normal[1];
  const float fx = -(slot->normal[0] * inv), fz = -(slot->normal[2] * inv);
  slot->height = ((z - v[2]) * fz + (x - v[0]) * fx) + v[1];
  return 1;
}

namespace {
// One polygon of 0x0054be40: a strip as its triangles, the count byte left
// alone. A strip of fewer than two vertices would read past its list in the
// exe; none ships, and the port skips it.
int32_t point_polygon(const Polygon& p, const float* world, zcls::GroundHit* slot, const float* pt) {
  const uint32_t n = p.flags & 0xff;
  if (!(p.flags & 0x400)) {
    for (uint32_t i = 0; i < n; ++i) copy_vertex(g_s.poly + 3 * i, world, p.vertex_indices[i]);
    return polygon_ground_point(slot, g_s.poly, pt[0], pt[2], static_cast<int32_t>(n)) && !(slot->height > pt[1]);
  }
  if (n < 3) return 0;
  for (uint32_t s = 2; s < n; ++s) {
    copy_vertex(g_s.poly, world, p.vertex_indices[s - 2]);
    copy_vertex(g_s.poly + 3, world, p.vertex_indices[(s - 1) | 1]);
    copy_vertex(g_s.poly + 6, world, p.vertex_indices[s & ~1u]);
    if (polygon_ground_point(slot, g_s.poly, pt[0], pt[2], 3) && !(slot->height > pt[1])) return 1;
  }
  return 0;
}
}  // namespace

// Mech3.exe 1.2 @ 0x0054be40
int32_t model_ground_point(Model* model, zcls::GroundHit* slot, const float point[3]) {
  if (hdr(*model, 0x10) == 0) return 0;
  const float* world = place_vertices(*model);
  uint32_t hit = hdr(*model, 0x58);
  bool found = point_polygon(model->polygons[hit], world, slot, point);
  for (uint32_t i = 0; !found && i < hdr(*model, 0x10); ++i) {
    if (i == hdr(*model, 0x58)) continue;
    if (point_polygon(model->polygons[i], world, slot, point)) hit = i, found = true;
  }
  if (!found) return 0;
  std::memcpy(model->header + 0x58, &hit, 4);
  const Polygon& p = model->polygons[hit];
  slot->zone[0] = p.zone_set;
  slot->material = p.material;
  return 1;
}

namespace {
uint32_t abs_bits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u & 0x7fffffffu;
}

// The inside test of 0x0054d760 on the plane (u, v): c a vertex, a the next.
float edge(const float* p, const float* c, const float* a, int u, int v) {
  return (p[v] - c[v]) * (a[u] - c[u]) + (p[u] - c[u]) * (c[v] - a[v]);
}
}  // namespace

// Mech3.exe 1.2 @ 0x0054d760. The normal is the cross product of the first
// three vertices' edges, normalised only on a hit. The inside test runs on
// the plane the normal points along most (picked by the magnitudes' bit
// patterns), the edges walked back from the closing one, within 0.0001; on
// the -z plane an unordered edge also fails, on the others only the first.
int32_t polygon_segment(zcls::LineHit* slot, const float a[3], const float b[3], const float* v, int32_t n,
                        int32_t two_sided) {
  const float e1x = v[3] - v[0], e1y = v[4] - v[1], e1z = v[5] - v[2];
  const float e2x = v[6] - v[0], e2y = v[7] - v[1], e2z = v[8] - v[2];
  float* nn = slot->normal;
  nn[0] = e2z * e1y - e2y * e1z;
  nn[1] = e2x * e1z - e2z * e1x;
  nn[2] = e2y * e1x - e2x * e1y;
  const float d_end = ((b[1] - v[1]) * nn[1] + (b[2] - v[2]) * nn[2]) + (b[0] - v[0]) * nn[0];
  if (!two_sided && d_end >= 0.0f) return 0;  // 0x005ce510
  const float d_start = ((a[1] - v[1]) * nn[1] + (a[2] - v[2]) * nn[2]) + (a[0] - v[0]) * nn[0];
  if (same_sign(d_end, d_start)) return 0;
  const float t = d_start / (d_start - d_end);
  float* p = slot->point;
  p[0] = (b[0] - a[0]) * t + a[0];
  p[1] = (b[1] - a[1]) * t + a[1];
  p[2] = (b[2] - a[2]) * t + a[2];
  const uint32_t ax = abs_bits(nn[0]), ay = abs_bits(nn[1]);
  int k = ax < ay ? 1 : 0;
  if ((ax < ay ? ay : ax) < abs_bits(nn[2])) k = 2;
  uint32_t sign;
  std::memcpy(&sign, &nn[k], 4);
  if (sign & 0x80000000u) k += 3;
  static const int kAxes[6][2] = {{1, 2}, {2, 0}, {0, 1}, {2, 1}, {0, 2}, {1, 0}};
  const int u = kAxes[k][0], w = kAxes[k][1];
  if (!(edge(p, v + 3 * (n - 1), v, u, w) > -0.0001)) return 0;  // 0x005ce518
  for (int32_t i = n - 2; i >= 0; --i) {
    const float e = edge(p, v + 3 * i, v + 3 * (i + 1), u, w);
    if (k == 5 ? !(e > -0.0001) : e <= -0.0001) return 0;
  }
  zmth::Vec3 un{nn[0], nn[1], nn[2]};
  zmth::normalize(un);
  nn[0] = un.x, nn[1] = un.y, nn[2] = un.z;
  return 1;
}

// Mech3.exe 1.2 @ 0x0054dd30. As one segment of 0x0054ea30 (the unit normal,
// the projected inside test), but one-sided unless two_sided, and the
// coordinates always captured.
int32_t polygon_segment_uv(zcls::LineHit* slot, const float a[3], const float b[3], const float* v,
                           const float* uvs, float uv_out[2], int32_t n, int32_t two_sided) {
  float* nn = slot->normal;
  zmth::poly_normal(v, v + 3, v + 6, nn);
  const float d_end = dot_from(b, v, nn);
  if (!two_sided && !(d_end < 0.0f)) return 0;
  const float d_start = dot_from(a, v, nn);
  if (same_sign(d_start, d_end)) return 0;
  const float t = d_start / (d_start - d_end);
  float* p = slot->point;
  for (int i = 0; i < 3; ++i) p[i] = a[i] + t * (b[i] - a[i]);
  const float mx = std::fabs(nn[0]), my = std::fabs(nn[1]);
  int axis = mx < my ? 1 : 0;
  if ((mx < my ? my : mx) < std::fabs(nn[2])) axis = 2;
  bool in = true;
  int32_t j = 0;
  for (int32_t i = n - 1; i >= 0; j = i, --i) {
    if (!in) return 0;
    const float* c = v + 3 * i;
    const float* q = v + 3 * j;
    float s;
    if (axis == 0) {
      float y, z;
      if (0.0f <= nn[0]) y = q[1] - c[1], z = c[2] - q[2];
      else y = c[1] - q[1], z = q[2] - c[2];
      s = (p[1] - c[1]) * z + (p[2] - c[2]) * y;
    } else if (axis == 1) {
      float x, z;
      if (nn[1] <= 0.0f) x = q[0] - c[0], z = c[2] - q[2];
      else x = c[0] - q[0], z = q[2] - c[2];
      s = (p[2] - c[2]) * x + (p[0] - c[0]) * z;
    } else {
      float y, x;
      if (nn[2] <= 0.0f) y = q[1] - c[1], x = c[0] - q[0];
      else y = c[1] - q[1], x = q[0] - c[0];
      s = (p[1] - c[1]) * x + (p[0] - c[0]) * y;
    }
    in = s > -0.0001;
  }
  if (!in) return 0;
  const int a0 = axis == 0 ? 1 : 0, a1 = axis == 2 ? 1 : 2;
  float gu[2], gv[2];
  zmth::plane_gradient(v[a0], v[a1], v[3 + a0], v[3 + a1], v[6 + a0], v[6 + a1], uvs[0], uvs[2], uvs[4], &gu[0],
                       &gu[1]);
  zmth::plane_gradient(v[a0], v[a1], v[3 + a0], v[3 + a1], v[6 + a0], v[6 + a1], uvs[1], uvs[3], uvs[5], &gv[0],
                       &gv[1]);
  uv_out[0] = (p[a0] - v[a0]) * gu[0] + (p[a1] - v[a1]) * gu[1] + uvs[0];
  uv_out[1] = (p[a1] - v[a1]) * gv[1] + (p[a0] - v[a0]) * gv[0] + uvs[1];
  g_uv.uv[0] = uv_out[0];
  g_uv.uv[1] = uv_out[1];
  return 1;
}

// Mech3.exe 1.2 @ 0x0054d2f0
int32_t box_segment(const float c[8][3], zcls::LineHit* slot, const float a[3], const float b[3]) {
  static const int kFaces[6][4] = {{0, 4, 7, 3}, {0, 1, 5, 4}, {5, 1, 2, 6}, {7, 6, 2, 3}, {0, 3, 2, 1}, {4, 5, 6, 7}};
  slot->material = nullptr;
  for (const auto& f : kFaces) {
    for (int i = 0; i < 4; ++i) std::memcpy(g_s.poly + 3 * i, c[f[i]], 12);
    if (polygon_segment(slot, a, b, g_s.poly, 4, 0)) return 1;
  }
  return 0;
}

namespace {
// A model's vertices in its own space, morphed when the node set a morph.
const float* model_vertices(Model& m) {
  float f;
  std::memcpy(&f, m.header + 0x24, 4);
  if ((hdr(m, 8) & 8) && f != 0.0f && hdr(m, 0x1c) != 0) {
    float* dst = sized(g_s.morphed, 3 * hdr(m, 0x1c));
    zmth::morph(reinterpret_cast<zmth::Vec3*>(dst), reinterpret_cast<const zmth::Vec3*>(m.vertices),
                reinterpret_cast<const zmth::Vec3*>(m.morphs), hdr(m, 0x1c), f);
    return dst;
  }
  return m.vertices;
}

// One polygon of 0x0054cad0: -1 for a strip of two vertices (the exe reads
// its unset result as a hit at the cached index, nothing elsewhere).
int32_t segment_polygon(const Polygon& p, const float* verts, zcls::LineHit* slot, const float* a, const float* b) {
  const uint32_t flags = p.flags;
  const uint32_t n = flags & 0xff;
  const int32_t two = (flags >> 8) & 1;
  const bool textured = p.material && (p.material->flags & 2);
  float uv_out[2];
  if (!(flags & 0x400)) {
    for (uint32_t i = 0; i < n; ++i) copy_vertex(g_s.poly + 3 * i, verts, p.vertex_indices[i]);
    return textured ? polygon_segment_uv(slot, a, b, g_s.poly, p.uvs, uv_out, static_cast<int32_t>(n), two)
                    : polygon_segment(slot, a, b, g_s.poly, static_cast<int32_t>(n), two);
  }
  if (n == 2) return -1;
  if (n < 2) return 0;  // the exe walks past its list; none ships
  for (uint32_t s = 2; s < n; ++s) {
    const uint32_t i0 = s - 2, i1 = (s - 1) | 1, i2 = s & ~1u;
    copy_vertex(g_s.poly, verts, p.vertex_indices[i0]);
    copy_vertex(g_s.poly + 3, verts, p.vertex_indices[i1]);
    copy_vertex(g_s.poly + 6, verts, p.vertex_indices[i2]);
    int32_t r;
    if (textured) {
      const float uv[6] = {p.uvs[2 * i0], p.uvs[2 * i0 + 1], p.uvs[2 * i1], p.uvs[2 * i1 + 1], p.uvs[2 * i2],
                           p.uvs[2 * i2 + 1]};
      r = polygon_segment_uv(slot, a, b, g_s.poly, uv, uv_out, 3, two);
    } else {
      r = polygon_segment(slot, a, b, g_s.poly, 3, two);
    }
    if (r) return r;
  }
  return 0;
}

// The hit back in world space: the point through the top, the normal through
// its 3x3, normalised.
void hit_to_world(zcls::LineHit* slot) {
  const float* m = zmth::matrix_stack().matrix().m;
  const float* q = slot->point;
  const float px = ((q[0] * m[0] + q[1] * m[3]) + q[2] * m[6]) + m[9];
  const float py = ((q[0] * m[1] + q[1] * m[4]) + q[2] * m[7]) + m[10];
  const float pz = ((q[0] * m[2] + q[1] * m[5]) + q[2] * m[8]) + m[11];
  slot->point[0] = px, slot->point[1] = py, slot->point[2] = pz;
  const float* nn = slot->normal;
  zmth::Vec3 w{(nn[0] * m[0] + nn[1] * m[3]) + nn[2] * m[6], (nn[0] * m[1] + nn[1] * m[4]) + nn[2] * m[7],
               (nn[0] * m[2] + nn[1] * m[5]) + nn[2] * m[8]};
  zmth::normalize(w);
  slot->normal[0] = w.x, slot->normal[1] = w.y, slot->normal[2] = w.z;
}
}  // namespace

// Mech3.exe 1.2 @ 0x0054cad0
int32_t model_segment(Model* model, zcls::LineHit* slot, const float a[3], const float b[3]) {
  if (hdr(*model, 0x10) == 0) return 0;
  const float* verts = model_vertices(*model);
  zmth::MatrixStack& st = zmth::matrix_stack();
  const bool identity = st.is_identity() != 0;
  float sa[3], sb[3];
  if (!identity) {
    zmth::Mat34 inv;
    if (!zmth::invert_single(st.matrix(), inv, true)) return 0;
    const float* m = inv.m;
    for (int k = 0; k < 2; ++k) {
      const float* q = k ? b : a;
      float* o = k ? sb : sa;
      o[0] = ((q[0] * m[0] + q[1] * m[3]) + q[2] * m[6]) + m[9];
      o[2] = ((q[0] * m[2] + q[1] * m[5]) + q[2] * m[8]) + m[11];
      o[1] = ((q[0] * m[1] + q[1] * m[4]) + q[2] * m[7]) + m[10];
    }
  } else {
    std::memcpy(sa, a, 12);
    std::memcpy(sb, b, 12);
  }
  const uint32_t cached = hdr(*model, 0x58);
  const Polygon& c = model->polygons[cached];
  if (segment_polygon(c, verts, slot, sa, sb)) {
    slot->material = c.material;
    if (!identity) hit_to_world(slot);
    return 1;
  }
  for (uint32_t i = 0; i < hdr(*model, 0x10); ++i) {
    if (i == hdr(*model, 0x58)) continue;
    const Polygon& p = model->polygons[i];
    if (segment_polygon(p, verts, slot, sa, sb) > 0) {
      slot->material = p.material;
      if (!identity) hit_to_world(slot);
      std::memcpy(model->header + 0x58, &i, 4);
      return 1;
    }
  }
  return 0;
}

}  // namespace zmod
