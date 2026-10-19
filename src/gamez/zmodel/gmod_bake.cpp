#include "gamez/zmodel/gmod_bake.h"

#include <cmath>
#include <cstring>

#include "gamez/zclass/cls_data.h"
#include "gamez/zmodel/gmod_clip3d.h"
#include "gamez/zmodel/gmod_light.h"

#pragma STDC FP_CONTRACT OFF

namespace zmod {

namespace {
BakeState g_bake;
float g_normals[Clip3dBuffers::kVerts][3];  // 0x00747ca8, the polygon's normals (the draw's buffer)

constexpr double kVisible = 0.003921569;  // 0x005ce4a0, 1/255 as a double

enum : int {
  kCamPos = 0x78, kCamDir = 0x90, kDiffuse = 0x9c, kAmbient = 0xa0, kFlags = 0xb0,
  kNear = 0xb4, kFar = 0xb8, kNearSq = 0xbc, kFarSq = 0xc0, kInvRange = 0xc4,
};
enum : uint32_t { kSpot = 0x4, kDirected = 0x8, kStatic = 0x100 };

// x87 "below" (C0): true for a < b and for NaN.
bool below(float a, float b) { return !(a >= b); }
bool below(float a, double b) { return !(double{a} >= b); }

// The x87 dot product order: (x*x' + y*y') + z*z'.
float dot(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

// The exe's square root by halving the float's exponent bits (gmod_light has the same).
float approx_sqrt(float x) {
  int32_t bits;
  std::memcpy(&bits, &x, 4);
  bits = (bits >> 1) + 0x1fc00000;
  float r;
  std::memcpy(&r, &bits, 4);
  return r;
}

// Mech3.exe 1.2 @ 0x0051e110. v /= len in place (a copy when len is 0).
void scale_inverse(float v[3], float len) {
  if (len == 0.0f) return;
  const float inv = 1.0f / len;
  v[0] = inv * v[0], v[1] = v[1] * inv, v[2] = v[2] * inv;
}

// Mech3.exe 1.2 @ 0x00524020 (zMath): the unit normal (b - a) x (c - a), ECX a,
// EDX b, stack c. A zero-length normal is stored as (y, z, x), as the x87
// stack leaves it (all zero anyway).
void poly_normal(const float a[3], const float b[3], const float c[3], float out[3]) {
  const float px = c[0] - a[0], py = c[1] - a[1], pz = c[2] - a[2];
  const float qx = b[0] - a[0], qy = b[1] - a[1], qz = b[2] - a[2];
  const float x = qy * pz - py * qz;
  const float y = qz * px - pz * qx;
  const float z = qx * py - qy * px;
  const float len = std::sqrt(x * x + y * y + z * z);
  if (len == 0.0f) {
    out[0] = y, out[1] = z, out[2] = x;
    return;
  }
  const float inv = 1.0f / len;
  out[0] = x * inv, out[1] = y * inv, out[2] = z * inv;
}

// The sun's term for one normal: min(1, diffuse * N.L + ambient), or the
// ambient alone (unclamped) when N.L is not positive.
float directed_term(float d, float diffuse, float ambient) {
  if (!(d > 0.0f)) return ambient;
  const float lit = diffuse * d + ambient;
  return lit > 1.0f ? 1.0f : lit;
}

// v = light - p, and its squared length.
float to_light(const float* light, const float p[3], float v[3]) {
  v[0] = light[0] - p[0], v[1] = light[1] - p[1], v[2] = light[2] - p[2];
  return v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
}

// The range falloff times lit (dist is the squared distance inside near^2:
// the quirk gmod_light documents).
float falloff(const zcls::Light* d, float dist, float lit) {
  const float far = d->get<float>(kFar);
  if (!below(dist, far)) return 0.0f * lit;
  if (dist > d->get<float>(kNear)) return (far - dist) * d->get<float>(kInvRange) * lit;
  return 1.0f * lit;
}
}  // namespace

BakeState& bake_state() { return g_bake; }

// Mech3.exe 1.2 @ 0x00544ed0. The light starts at 1 per vertex and each
// static light adds its term; with any directed static light the 1 is taken
// off again, so without one the bake holds 1 + the point lights.
void bake_static_light(Model& model, const float (*cam)[3], const float (*normals)[3]) {
  float (*poly_cam)[3] = clip3d_buffers().cam;  // 0x007476a4, file order here (the draw reverses it)
  const uint32_t no_sun = (model.count(0x08) >> 5) & 1;
  const int32_t polys = static_cast<int32_t>(model.count(0x10));
  const LightList& L = light_list();
  float* stale = g_bake.stale;
  for (int32_t p = 0; p < polys; ++p) {
    Polygon& poly = model.polygons[p];
    if (poly.flags & 0x100) continue;  // two-sided: the draw passes no baked light for these
    const int32_t n = static_cast<int32_t>(poly.flags & 0xff);
    // The copies are do-while loops: a polygon with no vertices would run 2^32 times.
    for (int32_t i = 0; i < n; ++i) std::memcpy(poly_cam[i], cam[poly.vertex_indices[i]], 12);
    const bool vn = !no_sun && (poly.flags & 0x200);
    if (vn)
      for (int32_t i = 0; i < n; ++i) std::memcpy(g_normals[i], normals[poly.normal_indices[i]], 12);
    lighting().vertex_normals = vn;  // 0x00747ca4
    float* out = reinterpret_cast<float*>(poly.scratch);
    const int32_t last = n - 1;
    for (int32_t i = last; i >= 0; --i) out[i] = 1.0f;
    bool have_normal = false, directed = false;
    float normal[3];
    float lit = 0.0f;  // [ebp-4]
    for (int32_t li = 0; li < L.count; ++li) {
      const LightEntry& e = L.list[li];
      zcls::Light* d = e.data;
      const uint32_t fl = d->get<uint32_t>(kFlags);
      if (!(fl & kStatic)) continue;
      const float* dir = d->floats(kCamDir);
      const float diffuse = d->get<float>(kDiffuse), ambient = d->get<float>(kAmbient);
      if (fl & kDirected) {
        // Directed lights are not range-tested here (no reach check).
        if (no_sun) {
          lit = ambient > 1.0f ? 1.0f : ambient;
        } else if (!vn) {
          if (!have_normal) poly_normal(poly_cam[1], poly_cam[2], poly_cam[0], normal), have_normal = true;
          lit = directed_term(dot(normal, dir), diffuse, ambient);
        }
        directed = true;
        for (int32_t i = last; i >= 0; --i) {
          if (vn) lit = directed_term(dot(g_normals[i], dir), diffuse, ambient);
          out[i] = lit + out[i];
        }
        continue;
      }
      if (!e.reach) continue;
      const float* pos = d->floats(kCamPos);
      float spot, factor;  // [ebp-0x44] the polygon's N.L, the first vertex's spot factor
      if (fl & kSpot) {
        if (!have_normal) poly_normal(poly_cam[1], poly_cam[2], poly_cam[0], normal), have_normal = true;
        spot = dot(normal, dir);
        if (!(double{spot} > kVisible)) {
          spot = 0.0f, factor = 0.0f;
        } else {
          // Quirk: the vector is still the previous light's or polygon's.
          const float s = dot(stale, dir);
          factor = below(s, kVisible) ? 0.0f : s;
        }
      } else {
        spot = 1.0f, factor = 1.0f;
      }
      if (!(spot > 0.0f) && !(ambient > 0.0f)) continue;
      lit = directed_term(factor, diffuse, ambient);
      // The first vertex, then the others last to first.
      bool in = true;
      float dist = 0.0f;  // [ebp-8]
      if (!e.full) {
        const float d2 = to_light(pos, poly_cam[0], stale);
        dist = d2;
        in = below(d2, d->get<float>(kFarSq));
        if (in) {
          if (fl & kSpot) dist = approx_sqrt(d2), scale_inverse(stale, dist);
          else if (!below(d2, d->get<float>(kNearSq))) dist = approx_sqrt(d2);
        }
      }
      if (in) out[0] = (e.full ? lit : falloff(d, dist, lit)) + out[0];
      for (int32_t i = last; i >= 1; --i) {
        if (!e.full) {
          const float d2 = to_light(pos, poly_cam[i], stale);
          dist = d2;
          if (!below(d2, d->get<float>(kFarSq))) continue;
          if (fl & kSpot) dist = approx_sqrt(d2), scale_inverse(stale, dist);
          else if (!below(d2, d->get<float>(kNearSq))) dist = approx_sqrt(d2);
        }
        float f = 0.0f;
        if (spot > 0.0f) {
          f = spot;
          if (fl & kSpot) {
            if (e.full) to_light(pos, poly_cam[i], stale);  // unnormalised
            const float s = dot(stale, dir);
            f = below(s, kVisible) ? 0.0f : s;
          }
        }
        if (!(double{f} > kVisible) && !(ambient > 0.0f)) continue;
        if (fl & kSpot) lit = directed_term(f, diffuse, ambient);
        out[i] = (e.full ? lit : falloff(d, dist, lit)) + out[i];
      }
    }
    if (directed)
      for (int32_t i = last; i >= 0; --i) out[i] = out[i] - 1.0f;
  }
}

}  // namespace zmod
