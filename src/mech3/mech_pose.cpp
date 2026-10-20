// The pose on a mech's parts: each part's set_pose, the leg IK (docs/movement-pose.md).
// Original file: mech3\mech_parts.cpp (the part methods, 0x00460280..0x00463ee0).
#include <cmath>

#include "crt/msvc_math.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmath/zmth_vector.h"
#include "mech3/mech.h"
#include "mech3/mech_move.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3 {

namespace {
PoseCalls g_pose;
bool g_leg_roll = true;  // [0x005e4f1c], 1 in the data: the thigh also rolls toward the foot's side
}  // namespace

PoseCalls& pose_calls() { return g_pose; }

// Mech3.exe 1.2 @ 0x00460180
void MechPart::node_changed() {
  if (g_pose.node_set) g_pose.node_set(node, rotation);
}

// Mech3.exe 1.2 @ 0x00460280. A pending pose wins once over the one given
// (whose pointers only say which halves to set).
void MechPart::set_pose(const Vec3* pos, const float* q) {
  if (!node) return;
  if (pending) {
    if (pos) *translation = pending_pos;
    if (q) zmth::quat_matrix(pending_q, rotation);
    MechPart::node_changed();
    pending = false;
    pending_pos = {0, 0, 0};                                  // 0x0063d870
    pending_q[0] = 1, pending_q[1] = pending_q[2] = pending_q[3] = 0;  // 0x005eda50
    return;
  }
  if (pos) *translation = *pos;
  if (q) zmth::quat_matrix(q, rotation);
  node_changed();
}

// Mech3.exe 1.2 @ 0x004629b0. The rotation waits for the foot's update.
void Foot::set_pose(const Vec3* pos, const float* q) {
  *translation = *pos;
  if (q)
    for (int i = 0; i < 4; ++i) pending_q[i] = q[i];
}

// Mech3.exe 1.2 @ 0x004624b0: the offset (the crouch) added; a new rotation
// also keeps its inverse and puts both feet back on the ground (0x00462a10).
void Hip::set_pose(const Vec3* pos, const float* q) {
  const Vec3 p{offset.x + pos->x, offset.y + pos->y, offset.z + pos->z};
  MechPart::set_pose(&p, q);
  if (!q) return;
  for (int i = 0; i < 4; ++i) rot[i] = q[i];
  zmth::Quat c;
  zmth::quat_conjugate(zmth::Quat{q[0], q[1], q[2], q[3]}, c);
  zmth::quat_normalize(c, c);
  inv[0] = c.w, inv[1] = c.x, inv[2] = c.y, inv[3] = c.z;
  if (move_calls().foot_ground) {
    move_calls().foot_ground(*mech->legs[0]->foot);
    move_calls().foot_ground(*mech->legs[1]->foot);
  }
}

// Mech3.exe 1.2 @ 0x00463c90. Two-bone IK in the leg's plane: the knee from
// the law of cosines (an ankle out of reach is pulled in along the line to
// it), the thigh's pitch, and its roll toward the ankle's side. fpatan and
// its products stay on the x87 stack; the 24-bit rounding is kept.
bool Leg::solve(const Vec3& p) {
  const Vec3& t = *translation;
  float y = -(p.y - t.y);
  float z = -(p.z - t.z);
  const float d2 = y * y + z * z;
  const float xr = p.x - hip_x;
  const float s2 = ik_k + d2;
  float c = ik_m * s2;
  if (!(c <= 1.0f) || c < -1.0f) {  // NaN is pulled in too
    const float u = 1.0f / c - ik_k / s2;
    const float k = std::sqrt(u * s2 / d2);
    c = 1.0f;
    y = y * k;
    z = k * z;
  }
  const double roll = std::atan2(double{xr}, double{y});
  float s = std::sqrt(1.0f - c * c);
  if (!knee_back) s = -s;
  knee_angle = static_cast<float>(std::atan2(double{s}, double{c}));
  const double b = std::atan2(double{z}, double{y});
  const float den = calf_len * c + thigh_len;
  const double a = std::atan2(double{calf_len * s}, double{den});
  thigh_angle = crt::x87_add_f24(b, -a);
  zmth::Quat pitch, out;
  zmth::quat_pitch(thigh_angle, pitch);
  if (g_leg_roll) {
    zmth::Quat r;
    zmth::quat_roll(static_cast<float>(roll), r);
    zmth::quat_mul(r, pitch, out);
  } else {
    out = pitch;
  }
  thigh_q[0] = out.w, thigh_q[1] = out.x, thigh_q[2] = out.y, thigh_q[3] = out.z;
  zmth::quat_pitch(knee_angle, out);
  calf_q[0] = out.w, calf_q[1] = out.x, calf_q[2] = out.y, calf_q[3] = out.z;
  return true;
}

// Mech3.exe 1.2 @ 0x00463e40. p is in the hip's frame without its offset;
// the foot gets it and its rotation only when the solve succeeds (always).
bool Leg::reach(const Vec3& p, const float q[4]) {
  const Vec3& o = mech->hip->offset;
  const Vec3 local{p.x - o.x, p.y - o.y, p.z - o.z};
  const bool ok = solve(local);
  set_pose(nullptr, thigh_q);
  calf->set_pose(nullptr, calf_q);
  if (!ok) return false;
  foot->set_pose(&local, q);
  return true;
}

}  // namespace mech3
