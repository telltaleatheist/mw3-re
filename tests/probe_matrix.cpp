// Probes for test_matrix.py (docs/drafts/matrix.md). Self-contained: no shared helpers.
#include <cstdint>
#include <cstring>

#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmath/zmth_vector.h"
#include "gamez/zrender/zrndr_fog.h"

namespace {
zmth::Mat34 g_below, g_top, g_pushed;

zmth::Mat34 mat(const float* m) {
  zmth::Mat34 r;
  std::memcpy(r.m, m, sizeof r.m);
  return r;
}

// The stack as the test sets it: slot 0 below, slot 1 the top.
void setup(const float* below, int32_t below_flag, const float* top, int32_t flag) {
  zmth::MatrixStack& s = zmth::matrix_stack();
  g_below = mat(below);
  g_top = mat(top);
  s.slot[0] = &g_below;
  s.slot[1] = &g_top;
  s.identity[0] = below_flag;
  s.identity[1] = flag;
  s.top = 1;
}
}  // namespace

extern "C" {

// One stack operation on (below, top). op: 0 multiply(a, mode), 1 euler(a[0..2]
// rotation, a[3..5] translation, a[6..8] scale), 2 scale, 3 translate (a[0..2]),
// 4/5/6 rotate x/y/z (a[0]), 7 load_model_view, 8 load(a), 9 load_rotation(a),
// 10 load_identity, 11 load_view, 12 load_camera, 13 push_copy then report the
// pushed buffer as the top, 14 push (contents untouched, so the top reported is
// the pushed buffer as given in a). top and flag come back; returns the stack index.
int32_t probe_matrix_stack(int op, const float* below, int32_t below_flag, float* top, int32_t* flag,
                           const float* a, int mode) {
  setup(below, below_flag, top, *flag);
  zmth::MatrixStack& s = zmth::matrix_stack();
  switch (op) {
    case 0: zmth::multiply(mat(a), mode); break;
    case 1: zmth::euler(a, a + 3, a + 6); break;
    case 2: zmth::scale(a[0], a[1], a[2]); break;
    case 3: zmth::translate(a[0], a[1], a[2]); break;
    case 4: zmth::rotate_x(a[0]); break;
    case 5: zmth::rotate_y(a[0]); break;
    case 6: zmth::rotate_z(a[0]); break;
    case 7: zmth::load_model_view(); break;
    case 8: zmth::load(mat(a)); break;
    case 9: zmth::load_rotation(mat(a)); break;
    case 10: zmth::load_identity(); break;
    case 11: zmth::load_view(); break;
    case 12: zmth::load_camera(); break;
    case 13: zmth::push_copy(&g_pushed); break;
    case 14: g_pushed = mat(a); zmth::push(&g_pushed); break;
    default: return -1;
  }
  std::memcpy(top, s.matrix().m, sizeof g_top.m);
  *flag = s.is_identity();
  const int32_t index = s.top;
  if (op >= 13) zmth::pop();
  return index;
}

// set_view 0x520b00: out = view (12) then camera (12).
void probe_matrix_set_view(const float* world, float* out) {
  zmth::set_view(mat(world));
  std::memcpy(out, zmth::view().view.m, 48);
  std::memcpy(out + 12, zmth::view().camera.m, 48);
}

// Sets view and camera directly (for load_view, load_camera, load_model_view).
void probe_matrix_put_view(const float* v, const float* c) {
  zmth::view().view = mat(v);
  zmth::view().camera = mat(c);
}

void probe_matrix_to_euler(const float* m, float* out) { zmth::matrix_to_euler(mat(m), out); }

// An impact's turn to its surface (0x005828a3..): 0x00525a50 from (0, 1, 0)
// to the normal, then 0x00525f80.
void probe_surface_angles(const float* n, float* out) {
  const float up[3] = {0.0f, 1.0f, 0.0f};
  float q[4];
  zmth::quat_between(up, n, q);
  zmth::quat_angles(q, out);
}

int32_t probe_matrix_invert(const float* m, float* out, int translation) {
  zmth::Mat34 o;
  std::memcpy(o.m, out, sizeof o.m);
  const bool r = zmth::invert(mat(m), o, translation != 0);
  std::memcpy(out, o.m, sizeof o.m);
  return r;
}

// set_projection 0x521c60 with a[0..7], the Direct3D flag hw, then set_fov
// a[8], a[9]; out = the Projection fields in declaration order (21 floats),
// then focal() (2).
void probe_matrix_projection(const float* a, int hw, float* out) {
  zrndr::device_fog().active = hw;
  zmth::set_projection(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
  zmth::set_fov(a[8], a[9]);
  zrndr::device_fog().active = 0;
  const zmth::Projection& p = zmth::projection();
  const float f[] = {p.origin_x, p.origin_y, p.half_w, p.half_h, p.cot_h, p.cot_v, p.inv_cot_h,
                     p.inv_cot_v, p.inv_fx, p.inv_fy, p.fov_h, p.fov_v, p.near_z, p.cx, p.cy,
                     p.inv_near, p.far_z, p.fy_near, p.fx, p.fy, p.fx_near};
  std::memcpy(out, f, sizeof f);
  zmth::focal(out + 21);
}

// Uses the projection the last probe_matrix_projection set. op: 0 project,
// 1 project_d3d, 2 unproject_d3d, 3 unproject, 4 scale_by_cot, 5 scale_by_tan
// (4 and 5 in place on dst, which comes in holding src).
void probe_matrix_project(int op, const float* src, float* dst, int32_t count) {
  const auto* s = reinterpret_cast<const zmth::Vec3*>(src);
  auto* d = reinterpret_cast<zmth::Vec3*>(dst);
  switch (op) {
    case 0: zmth::project(s, d, static_cast<uint32_t>(count)); break;
    case 1: zmth::project_d3d(s, d, static_cast<uint32_t>(count)); break;
    case 2: zmth::unproject_d3d(s, d, count); break;
    case 3: zmth::unproject(s, d, count); break;
    case 4: zmth::scale_by_cot(d, static_cast<uint32_t>(count)); break;
    case 5: zmth::scale_by_tan(d, static_cast<uint32_t>(count)); break;
    default: break;
  }
}

// Point ops through a top (flag given). op: 0 rotate_points, 1
// rotate_points_transposed, 3 transform_points (in place on dst, which comes in
// holding src), 2 rotate_points_to, 4 inverse_transform_points_to, 5
// transform_points_to (src -> dst), 6 morph(dst, src, extra, count, f).
void probe_matrix_points(int op, const float* top, int32_t flag, const float* src, float* dst,
                         int32_t count, const float* extra, float f) {
  const float zero[12] = {};
  setup(zero, 0, top, flag);
  const auto* s = reinterpret_cast<const zmth::Vec3*>(src);
  auto* d = reinterpret_cast<zmth::Vec3*>(dst);
  const auto u = static_cast<uint32_t>(count);
  switch (op) {
    case 0: zmth::rotate_points(d, u); break;
    case 1: zmth::rotate_points_transposed(d, count); break;
    case 2: zmth::rotate_points_to(s, d, u); break;
    case 3: zmth::transform_points(d, u); break;
    case 4: zmth::inverse_transform_points_to(s, d, count); break;
    case 5: zmth::transform_points_to(s, d, u); break;
    case 6: zmth::morph(d, s, reinterpret_cast<const zmth::Vec3*>(extra), u, f); break;
    default: break;
  }
}

}  // extern "C"
