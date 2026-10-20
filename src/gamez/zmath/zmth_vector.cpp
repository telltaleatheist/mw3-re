#include "gamez/zmath/zmth_vector.h"

#include <cmath>
#include <cstdint>
#include <cstring>

#pragma STDC FP_CONTRACT OFF

namespace zmth {

// Mech3.exe 1.2 @ 0x004020d0
float normalize(Vec3& v) {
  const float len = std::sqrt((v.x * v.x + v.z * v.z) + v.y * v.y);
  uint32_t bits;
  std::memcpy(&bits, &len, 4);
  if (bits & 0x7fffffff) {
    const float inv = 1.0f / len;
    v.x = inv * v.x;
    v.y = inv * v.y;
    v.z = inv * v.z;
  }
  return len;
}

// Mech3.exe 1.2 @ 0x0051e210. 0 stays 0; out may be v.
void flat_normalize(const Vec3& v, Vec3& out) {
  const float len = std::sqrt((v.x * v.x + 0.0f * 0.0f) + v.z * v.z);
  const float inv = len == 0.0f ? len : 1.0f / len;
  out.x = inv * v.x;
  out.z = v.z * inv;
}

// Mech3.exe 1.2 @ 0x0051e3d0
float direction(const Vec3& from, const Vec3& to, Vec3& out) {
  out = {to.x - from.x, to.y - from.y, to.z - from.z};
  return normalize(out);
}

// Mech3.exe 1.2 @ 0x00525840: 2w (q x v) + 2 (q.v) q + (2w^2 - 1) v, summed
// in the original's order (z adds its terms the other way round).
void quat_rotate(const Quat& q, const Vec3& v, Vec3& out) {
  const float d = (q.x * v.x + q.y * v.y) + q.z * v.z;
  const float d2 = d + d, w2 = q.w + q.w;
  const float c = w2 * q.w - 1.0f;
  out.x = ((v.z * q.y - q.z * v.y) * w2 + d2 * q.x) + c * v.x;
  out.y = ((v.x * q.z - v.z * q.x) * w2 + d2 * q.y) + c * v.y;
  out.z = ((v.y * q.x - v.x * q.y) * w2 + c * v.z) + d2 * q.z;
}

// Mech3.exe 1.2 @ 0x0051dfa0
float distance_sq(const Vec3& a, const Vec3& b) {
  const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
  return (dx * dx + dy * dy) + dz * dz;
}

// Mech3.exe 1.2 @ 0x0051e740: a unit vector at right angles to v in the xy
// plane; the length comes from the bit-trick square root.
void perpendicular(const Vec3& v, Vec3* out) {
  out->z = 0.0f;
  if (v.x == 0.0f) {
    out->x = 1.0f;
    out->y = 0.0f;
    return;
  }
  const float sq = v.y * v.y + v.x * v.x;
  int32_t bits;
  std::memcpy(&bits, &sq, 4);
  bits = (bits >> 1) + 0x1fc00000;
  float len;
  std::memcpy(&len, &bits, 4);
  const float inv = 1.0f / len;
  out->x = inv * v.y;
  out->y = -(inv * v.x);
}

// Mech3.exe 1.2 @ 0x0051e0e0
void add_scaled(const Vec3& base, const Vec3& dir, float s, Vec3* out) {
  *out = {s * dir.x + base.x, dir.y * s + base.y, dir.z * s + base.z};
}

// Mech3.exe 1.2 @ 0x0051e000
float distance(const Vec3& a, const Vec3& b) {
  const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
  return std::sqrt((dx * dx + dy * dy) + dz * dz);
}

// Mech3.exe 1.2 @ 0x0051e280. out may be v.
void reflect(const Vec3& n, const Vec3& v, Vec3* out) {
  const float d = (n.x * v.x + n.y * v.y) + n.z * v.z;
  if (d == 0.0f) {
    *out = {v.x * -1.0f, v.y * -1.0f, v.z * -1.0f};
    return;
  }
  const float k = -d;
  const Vec3 t{k * n.x, n.y * k, n.z * k};
  const Vec3 m{v.x + t.x, v.y + t.y, v.z + t.z};
  *out = {t.x + m.x, t.y + m.y, t.z + m.z};
}

// Mech3.exe 1.2 @ 0x0051e460. out may be a. The sines and the arc are the
// x87's at 24 bits in the original; here the CRT's, rounded once.
void slerp(const Vec3& a, const Vec3& b, float t, Vec3* out) {
  if (!(t > 0.0f)) {
    *out = a;
    return;
  }
  if (!(t < 1.0f)) {
    *out = b;
    return;
  }
  const float c = (a.x * b.x + a.y * b.y) + a.z * b.z;
  if (!(c >= -0.96f)) {
    Vec3 p;
    perpendicular(a, &p);
    const float ang = t * 3.14159274f;
    const float s = static_cast<float>(std::sin(static_cast<double>(ang)));
    const float co = static_cast<float>(std::cos(static_cast<double>(ang)));
    p = {p.x * s, p.y * s, p.z * s};
    const Vec3 o{co * a.x, a.y * co, a.z * co};
    *out = {p.x + o.x, p.y + o.y, p.z + o.z};
    return;
  }
  if (c > 0.96f) {
    const float u = 1.0f - t;
    const Vec3 o{u * a.x, a.y * u, a.z * u};
    const Vec3 q{t * b.x, b.y * t, b.z * t};
    *out = {o.x + q.x, o.y + q.y, o.z + q.z};
    return;
  }
  const float s = std::sqrt(1.0f - c * c);
  const float th = static_cast<float>(std::atan2(static_cast<double>(s), static_cast<double>(c)));
  const float k1 = static_cast<float>(std::sin(static_cast<double>((1.0f - t) * th)));
  const Vec3 o{k1 * a.x, a.y * k1, a.z * k1};
  const float k2 = static_cast<float>(std::sin(static_cast<double>(th * t)));
  const Vec3 q{k2 * b.x, b.y * k2, b.z * k2};
  const Vec3 r{o.x + q.x, o.y + q.y, o.z + q.z};
  const float inv = 1.0f / s;
  *out = {inv * r.x, inv * r.y, inv * r.z};
}

// Mech3.exe 1.2 @ 0x00524247: solves |at + vel t - from| = speed t. With d =
// from - at the answer is vel - d k for the k of the nearer root; the square
// root is the bit trick's.
int32_t intercept(const Vec3& from, const Vec3& at, float speed, const Vec3& vel, Vec3* out) {
  Vec3 d{from.x - at.x, from.y - at.y, from.z - at.z};
  const float dd = (d.x * d.x + d.y * d.y) + d.z * d.z;
  if (dd == 0.0f) return 0;
  const float b = (vel.x * d.x + vel.y * d.y) + vel.z * d.z;
  const float vv = (vel.x * vel.x + vel.y * vel.y) + vel.z * vel.z;
  float a = vv - speed * speed;
  float k;
  if (a == 0.0f) {
    if (!(b > 0.0f)) return 0;
    k = (b + b) / dd;
  } else {
    const float disc = b * b - dd * a;
    if (!(disc >= 0.0f)) return 0;
    int32_t bits;
    std::memcpy(&bits, &disc, 4);
    bits = (bits >> 1) + 0x1fc00000;
    float r;
    std::memcpy(&r, &bits, 4);
    float bb = b;
    if (a < 0.0f) {
      a = -a;
      bb = -b;
    }
    if (bb > r) {
      k = a / (bb - r);
    } else {
      const float s = bb + r;
      if (!(s > 0.0f)) return 0;
      k = a / s;
    }
  }
  d = {k * d.x, k * d.y, k * d.z};
  *out = {vel.x - d.x, vel.y - d.y, vel.z - d.z};
  normalize(*out);
  return 1;
}

// Mech3.exe 1.2 @ 0x00525490. Angles beyond 9.22e18 take the other branch
// (fsin and fcos apart); nothing passes such angles.
void quat_axis_angle(const Vec3& axis, float angle, Quat* q) {
  const float h = angle * 0.5f;
  q->w = static_cast<float>(std::cos(static_cast<double>(h)));
  const float s = static_cast<float>(std::sin(static_cast<double>(h)));
  q->x = s * axis.x;
  q->y = axis.y * s;
  q->z = axis.z * s;
}

}  // namespace zmth
