#pragma once

#include "gamez/zmath/zmth_matrix.h"

// Vector and quaternion helpers the movement code uses (docs/movement.md).
// The direction and the quaternion rotation sit in the unnamed maths module
// of zmth_matrix (0x51b170..0x526920); the normalise at 0x004020d0 is a copy
// in the first, unnamed game module (before mech3\briefing.cpp).
namespace zmth {

// Quaternions are (w, x, y, z), Hamilton products.
struct Quat {
  float w, x, y, z;
};

float normalize(Vec3& v);                                // 0x004020d0, returns the length; 0 stays 0
float direction(const Vec3& from, const Vec3& to, Vec3& out);  // 0x0051e3d0, normalize(to - from)
void flat_normalize(const Vec3& v, Vec3& out);           // 0x0051e210: x and z to unit length; out.y untouched
void quat_rotate(const Quat& q, const Vec3& v, Vec3& out);     // 0x00525840, q v q*
void quat_roll(float angle, Quat& out);                  // 0x00525210: about z
void quat_normalize(const Quat& q, Quat& out);           // 0x00525500: bit-trick square root
void quat_mul(const Quat& a, const Quat& b, Quat& out);  // 0x005255d0: a b
void quat_exp(const Vec3& v, Quat& out);                 // 0x00525900: (cos |v|, sin |v| v / |v|)

}  // namespace zmth
