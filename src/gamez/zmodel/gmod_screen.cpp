#include "gamez/zmodel/gmod_screen.h"

#include <cstring>

#include "gamez/zmodel/gmod_clip3d.h"
#include "gamez/zmodel/gmod_draw.h"

#pragma STDC FP_CONTRACT OFF

namespace zmod {

namespace {
float as_float(uint32_t v) {
  float f;
  std::memcpy(&f, &v, 4);
  return f;
}
}  // namespace

uint32_t project_point(const zmth::Vec3* world, zmth::Vec3* out) {
  zmth::Mat34 scratch;
  zmth::push(&scratch);
  zmth::load_view();
  zmth::MatrixStack& st = zmth::matrix_stack();
  if (st.is_identity()) {
    *out = *world;
  } else {
    const float* m = st.matrix().m;
    const float x = world->x, y = world->y, z = world->z;
    out->x = x * m[0] + y * m[3] + z * m[6] + m[9];
    out->z = x * m[2] + y * m[5] + z * m[8] + m[11];
    out->y = x * m[1] + y * m[4] + z * m[7] + m[10];
  }
  zmth::pop();
  const DrawState& ds = draw_state();
  const float near_z = ds.clip.near_z;            // 0x00740304
  const float left = ds.point_rect[0], top = ds.point_rect[1];
  const float right = ds.point_rect[2], bottom = ds.point_rect[3];
  if (out->z > near_z) {
    zmth::project(out, out, 1);
    uint32_t code = 0;
    if (out->x < left) {
      code = 1;
      out->x = left;
    } else if (out->x > right) {
      code = 2;
      out->x = right;
    }
    if (out->y < top) {
      out->y = top;
      return code | 4;
    }
    if (!(out->y < bottom)) {
      out->y = bottom - 1.0f;  // [0x005ce1d8]
      return code | 8;
    }
    return code;
  }
  out->z = -near_z <= out->z ? near_z : out->z * -1.0f;  // [0x005ce1dc]
  zmth::project(out, out, 1);
  out->y = bottom;
  if (out->x < -5000.0f) out->x = -5000.0f;  // [0x005ce1e0]
  else if (!(out->x < 5000.0f)) out->x = 5000.0f;
  const float clip_right = as_float(ds.clip.side_1c);  // 0x00740314
  out->x = (left + out->x - -5000.0f) / (10000.0f / (clip_right - left));  // [0x005ce1e8]
  return 8;
}

}  // namespace zmod
