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

}  // namespace zmth
