// zModel's world point on the screen (0x0053cf10) and the options' render to
// screen point (0x0040da30) for test_project_point.py (docs/hud.md section 14).
#include <cstdint>
#include <cstring>

#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_draw.h"
#include "gamez/zmodel/gmod_screen.h"
#include "mech3/options.h"

extern "C" {

// view: 12 floats; proj: fx, fy, cx, cy; clip: near, the clip's right
// (+0x1c), then the point rect left, top, right, bottom; world: 3 floats.
// out: the point (3), returns the outcode.
uint32_t probe_project_point(const float* view, const float* proj, const float* clip, const float* world,
                             float* out) {
  std::memcpy(zmth::view().view.m, view, 48);
  zmth::Projection& p = zmth::projection();
  p.fx = proj[0], p.fy = proj[1], p.cx = proj[2], p.cy = proj[3];
  zmod::DrawState& ds = zmod::draw_state();
  ds.clip.near_z = clip[0];
  std::memcpy(&ds.clip.side_1c, &clip[1], 4);
  for (int i = 0; i < 4; ++i) ds.point_rect[i] = clip[2 + i];
  zmth::Mat34 base;
  zmth::push(&base);  // the stack's first slot is empty: the test's base
  zmth::Vec3 o;
  const uint32_t code = zmod::project_point(reinterpret_cast<const zmth::Vec3*>(world), &o);
  zmth::pop();
  out[0] = o.x, out[1] = o.y, out[2] = o.z;
  return code;
}

// rects: render then window, each x, y, x_last, y_last; p in place.
void probe_render_to_screen(const int32_t* rects, int32_t doubled, float* p) {
  mech3::Options& o = mech3::options();
  o.render.x = rects[0], o.render.y = rects[1], o.render.x_last = rects[2], o.render.y_last = rects[3];
  o.window.x = rects[4], o.window.y = rects[5], o.window.x_last = rects[6], o.window.y_last = rects[7];
  o.pixel_double = doubled;
  mech3::render_to_screen(p);
}

}  // extern "C"
