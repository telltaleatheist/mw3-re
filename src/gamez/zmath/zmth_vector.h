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

void add_scaled(const Vec3& base, const Vec3& dir, float s, Vec3* out);  // 0x0051e0e0: base + dir s
float distance(const Vec3& a, const Vec3& b);            // 0x0051e000 (keeps a - b in 0x0063d328)
void reflect(const Vec3& n, const Vec3& v, Vec3* out);   // 0x0051e280: v - 2 (n.v) n; -v when n.v is 0
// 0x0051e460: from a toward b by t (0..1) over the arc of two unit vectors:
// linear when they are close (dot > 0.96), round a perpendicular when nearly
// opposite (dot < -0.96).
void slerp(const Vec3& a, const Vec3& b, float t, Vec3* out);
// 0x00524247: the direction to shoot from `from` at `speed` to meet a target
// at `at` moving at `vel`; 0 when it cannot.
int32_t intercept(const Vec3& from, const Vec3& at, float speed, const Vec3& vel, Vec3* out);

// The same two with pointer outputs, as the weapon code calls them.
inline float direction_to(const Vec3& from, const Vec3& to, Vec3* out) { return direction(from, to, *out); }
inline void quat_rotate(const Quat& q, const Vec3& v, Vec3* out) { quat_rotate(q, v, *out); }

}  // namespace zmth
