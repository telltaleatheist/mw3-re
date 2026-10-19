#pragma once
#include "gamez/zmath/zmth_matrix.h"

// Vector and quaternion helpers of the engine's maths library (the unnamed
// module 0x51b170..0x526920, see zmth_matrix.h) that the weapons use.
namespace zmth {

struct Quat {
  float w, x, y, z;  // +0 w, then the axis part
};

float distance_sq(const Vec3& a, const Vec3& b);         // 0x0051dfa0 (keeps a - b in 0x0063d328)
float direction_to(const Vec3& from, const Vec3& to, Vec3* out);  // 0x0051e3d0: out = |to - from|, returns the length
void perpendicular(const Vec3& v, Vec3* out);            // 0x0051e740: in the xz plane, by the fast square root
void quat_axis_angle(const Vec3& axis, float angle, Quat* q);  // 0x00525490
void quat_rotate(const Quat& q, const Vec3& v, Vec3* out);     // 0x00525840

}  // namespace zmth
