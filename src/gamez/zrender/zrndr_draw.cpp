#include "gamez/zrender/zrndr_draw.h"

#include <cstring>

#include "gamez/zimage/zimg_texture.h"
#include "gamez/zmodel/gmod_clip2d.h"
#include "gamez/zmodel/gmod_draw.h"
#include "gamez/zrender/zrndr_fog.h"

#pragma STDC FP_CONTRACT OFF

namespace zrndr {

namespace {
float as_float(uint32_t bits) {
  float f;
  std::memcpy(&f, &bits, 4);
  return f;
}
}  // namespace

// Mech3.exe 1.2 @ 0x00567720. Vertices: left-bottom, left-top, right-top,
// right-bottom, v running from 0 at the bottom to 1 at the top; rhw and the
// colours are the draw function's.
void draw_sprite(const float screen[3], const zimg::TextureEntry* texture, float half_size, float alpha,
                 int32_t colored) {
  // The x87 compares: "below" (C0) is also true for NaN, so `!(a >= b)`.
  if (!(half_size >= 1.0f)) return;
  if (!device_fog().active) return;  // the software path, 0x00566620: not ported
  const zmod::ClipPlanes& c = zmod::draw_state().clip;
  const float clip_left = as_float(c.side_04), clip_top = as_float(c.side_08);
  const float clip_right = as_float(c.side_1c), clip_bottom = as_float(c.side_20);
  float left = screen[0] - half_size;
  float top = screen[1] - half_size;
  float right = screen[0] + half_size;
  float bottom = half_size + screen[1];
  if (left >= clip_right || !(right >= clip_left) || top >= clip_bottom || !(bottom >= clip_top)) return;
  const float k = 0.5f / half_size;
  float u_left = 0.0f, v_top = 1.0f, u_right = 1.0f, v_bottom = 0.0f;
  if (!(left >= clip_left)) u_left = (clip_left - left) * k, left = clip_left;
  if (!(top >= clip_top)) v_top = 1.0f - (clip_top - top) * k, top = clip_top;
  if (right > clip_right) u_right = 1.0f - (right - clip_right) * k, right = clip_right;
  if (bottom > clip_bottom) v_bottom = (bottom - clip_bottom) * k, bottom = clip_bottom;
  zmod::TLVertex* v = zmod::clip2d().tl;
  const float x[4] = {left, left, right, right}, y[4] = {bottom, top, top, bottom};
  const float u[4] = {u_left, u_left, u_right, u_right}, w[4] = {v_bottom, v_top, v_top, v_bottom};
  for (int i = 0; i < 4; ++i) v[i].sx = x[i], v[i].sy = y[i], v[i].sz = screen[2], v[i].tu = u[i], v[i].tv = w[i];
  void* record = texture ? texture->surface : nullptr;
  // FIX (A): the draw functions read the record unchecked; on Direct3D every entry has
  // one (the default's from 0x005184e0 at startup, not ported: set it there).
  if (!record) return;
  const zmod::DrawTable& t = zmod::draw_table();
  if (colored) {
    if (t.textured) t.textured(t.ctx, 4, record, alpha, 0);
  } else if (t.textured_white) {
    t.textured_white(t.ctx, 4, record, 0, alpha, 0);
  }
}

}  // namespace zrndr
