#pragma once

#include "gamez/zmath/zmth_matrix.h"

// Vector and quaternion helpers the movement and weapon code use
// (docs/movement.md, docs/weapons.md).
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
float distance_sq(const Vec3& a, const Vec3& b);         // 0x0051dfa0 (keeps a - b in 0x0063d328)
void perpendicular(const Vec3& v, Vec3* out);            // 0x0051e740: in the xy plane, by the fast square root
void quat_axis_angle(const Vec3& axis, float angle, Quat* q);  // 0x00525490

// The same two with pointer outputs, as the weapon code calls them.
inline float direction_to(const Vec3& from, const Vec3& to, Vec3* out) { return direction(from, to, *out); }
inline void quat_rotate(const Quat& q, const Vec3& v, Vec3* out) { quat_rotate(q, v, *out); }

}  // namespace zmth
