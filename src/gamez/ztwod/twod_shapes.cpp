// zTwoD's HUD primitives (0x005a3170..0x005a3bc0, 0x005a67e0..0x005a6d10;
// file name unknown, named after what it holds): built and set up only, their
// drawing not ported yet (docs/twod.md).
#include "crt/msvc_math.h"
#include "gamez/ztwod/twod.h"

#pragma STDC FP_CONTRACT OFF

namespace ztwod {

// Mech3.exe 1.2 @ 0x005a39f0
Frame::Frame() : Object2D(0, 0) {}

// Mech3.exe 1.2 @ 0x005a3a60
Frame::Frame(int32_t px, int32_t py, int32_t r, uint32_t c, const Rect* cl)
    : Object2D(px, py), radius(r), radius_sq(r * r), color(c), clipped(cl != nullptr) {
  if (cl) clip = *cl;
}

// Mech3.exe 1.2 @ 0x005a3b60
FilledBox::FilledBox(int32_t px, int32_t py, int32_t r, uint32_t c, const Rect* cl) : Frame(px, py, r, c, cl) {}

// Mech3.exe 1.2 @ 0x005a67e0
Line2D::Line2D(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t c) : Object2D(0, 0) {
  set(x0, y0, x1, y1);
  x40 = 0;
  color = c;
  x44 = 0;
  x48 = 0;
}

// Mech3.exe 1.2 @ 0x005a6880
void Line2D::set(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
  set_pos(x0, y0);
  end_x = x1;
  end_y = y1;
}

// Mech3.exe 1.2 @ 0x005a6ae0
ClipLine2D::ClipLine2D(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t c) : Line2D(x0, y0, x1, y1, c) {}

// Mech3.exe 1.2 @ 0x005a3170
Shape::Shape() {
  Shape::set_pos(0, 0);
  set_rotation(0);
}

// Mech3.exe 1.2 @ 0x005a34c0
void Shape::set_pos(int32_t px, int32_t py) {
  const float t[3] = {static_cast<float>(px), static_cast<float>(py), 0.0f};
  set_translation(t);
}

// Mech3.exe 1.2 @ 0x005a34f0
void Shape::set_translation(const float* t) {
  translation[0] = t[0];
  translation[1] = t[1];
  translation[2] = t[2];
  xform |= 1;
  dirty();
}

// Mech3.exe 1.2 @ 0x005a3530
void Shape::set_rotation(int32_t a) {
  rotation = a;
  xform |= 2;
  dirty();
}

// Mech3.exe 1.2 @ 0x005a6bd0
Polygon::Polygon() : Object2D(0, 0) { dirty(); }

// Mech3.exe 1.2 @ 0x005a6c30
void Polygon::set_point(int32_t i, float px, float py) {
  if (i < 0x15 && i > -1) {
    pts[i][0] = px;
    pts[i][1] = py;
    if (count < i + 1) count = i + 1;
    if (i == 0) {
      const int32_t ty = crt::msvc_ftol(py);
      set_pos(crt::msvc_ftol(px), ty);
    }
  }
  dirty();
}

}  // namespace ztwod
