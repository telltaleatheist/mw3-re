#include "gamez/zmodel/gmod_hit.h"

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

}  // namespace zmod
