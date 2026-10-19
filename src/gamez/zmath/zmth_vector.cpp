#include "gamez/zmath/zmth_vector.h"

#include <cmath>
#include <cstdint>
#include <cstring>

#include "gamez/zmodel/gmod_draw.h"

#pragma STDC FP_CONTRACT OFF

namespace zmth {

// Mech3.exe 1.2 @ 0x0051dfa0
float distance_sq(const Vec3& a, const Vec3& b) {
  const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
  return (dx * dx + dy * dy) + dz * dz;
}

// Mech3.exe 1.2 @ 0x0051e3d0
float direction_to(const Vec3& from, const Vec3& to, Vec3* out) {
  float v[3] = {to.x - from.x, to.y - from.y, to.z - from.z};
  const float len = zmod::normalize(v);
  *out = {v[0], v[1], v[2]};
  return len;
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

// Mech3.exe 1.2 @ 0x00525840: v rotated by the unit quaternion q,
// v (2w^2 - 1) + 2 (q.v) q + 2w (q x v), summed in the exe's order.
void quat_rotate(const Quat& q, const Vec3& v, Vec3* out) {
  const float d = (q.x * v.x + q.y * v.y) + q.z * v.z;
  const float a = d + d;
  const float b = q.w + q.w;
  const float c = b * q.w - 1.0f;
  out->x = ((v.z * q.y - q.z * v.y) * b + a * q.x) + c * v.x;
  out->y = ((v.x * q.z - v.z * q.x) * b + a * q.y) + c * v.y;
  out->z = ((v.y * q.x - v.x * q.y) * b + c * v.z) + a * q.z;
}

}  // namespace zmth
