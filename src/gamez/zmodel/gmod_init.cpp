#include "gamez/zmodel/gmod_init.h"

#include <cstring>

#pragma STDC FP_CONTRACT OFF

#include "gamez/zclass/cls_data.h"
#include "gamez/zerror/zerr.h"
#include "gamez/zclass/window.h"
#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_const.h"
#include "gamez/zmodel/gmod_draw.h"
#include "gamez/zmodel/gmod_fog.h"
#include "gamez/zmodel/gmod_light.h"
#include "gamez/zutil/zutl_reg.h"
#include "gamez/zrender/zrndr_fog.h"

namespace zmod {

namespace {
WalkState g_walk{{}, 0, draw_model};

void put_float(uint32_t& slot, float v) { std::memcpy(&slot, &v, 4); }
}  // namespace

WalkState& walk_state() { return g_walk; }
uint32_t& mask_top() { return g_walk.mask[g_walk.top]; }

// Mech3.exe 1.2 @ 0x005266f0 (zModel's start 0x00526510, from 0x005266f0 on:
// its GfxFlags; the rest of the start is the program's).
void gfx_flags_start(bool hardware) {
  Lighting& l = lighting();
  l.mask_base = 0xffffffff;
  const zreg::Entry* e = zreg::find(hardware ? "GfxFlags_HW" : "GfxFlags_SW");
  l.mask = e ? reinterpret_cast<const uint32_t*>(e->data) : &l.mask_base;
}

// Mech3.exe 1.2 @ 0x00526750. The settings entry holds the value itself (the
// port's entry points at the option that does).
void use_gfx_flags(const char* name, uint32_t flags) {
  Lighting& l = lighting();
  const zreg::Entry* e = name ? zreg::find(name) : nullptr;
  if (e) {
    l.mask = reinterpret_cast<const uint32_t*>(e->data);
    return;
  }
  l.mask = &l.mask_base;
  l.mask_base = flags;
}

// Mech3.exe 1.2 @ 0x00526ae0 (ECX the model, EDX on, stack u, v).
int model_set_scroll(Model* m, int on, float u, float v) {
  if (!m) {
    zerr::report(0x200, "C:\\zipper\\GameZMech3\\zModel\\gmod_init.c", 0x2d5, "ERROR setting model texture scroll data; Null ptr.");
    return 1;
  }
  uint32_t f;
  std::memcpy(&f, m->header + 8, 4);
  f = (static_cast<uint32_t>(on) & 1) << 4 | (f & ~0x10u);
  std::memcpy(m->header + 8, &f, 4);
  std::memcpy(m->header + 0x2c, &v, 4);
  std::memcpy(m->header + 0x28, &u, 4);
  return 0;
}

// Mech3.exe 1.2 @ 0x00526a10. A weight of 1/255 or less turns the tint off.
void set_tint(const float* rgb, float weight) {
  Lighting& l = lighting();
  if (rgb) l.tint[0] = rgb[0], l.tint[1] = rgb[1], l.tint[2] = rgb[2];
  l.tint_on = 1;
  l.tint_weight = weight;
  if (!(weight > 0.003921569)) l.tint_on = 0;  // 0x005ce138, a double
}

// Mech3.exe 1.2 @ 0x00526a60
void set_opacity(float opacity) { draw_state().opacity = opacity; }
// Mech3.exe 1.2 @ 0x00526a70
void set_shadows(int32_t on) { draw_state().shadows = on; }
// Mech3.exe 1.2 @ 0x00526a80
int32_t shadows_on() { return draw_state().shadows; }
// Mech3.exe 1.2 @ 0x00526a90
void set_overlay(int32_t on) { draw_state().overlay = on; }
// Mech3.exe 1.2 @ 0x00526b90
int32_t fog_on() { return fog().on; }
// Mech3.exe 1.2 @ 0x00526bf0
float fog_near() { return fog().near_range; }

// Mech3.exe 1.2 @ 0x00526e10. The rect is the window's (the display's size
// when the window has none); right and bottom are nudged past the last pixel
// (Direct3D +0.001; software adds 0.5 for pixel centres).
void set_camera(zcls::Camera* camera) {
  WalkState& w = g_walk;
  DrawState& ds = draw_state();
  w.camera = camera;
  ds.camera_data = reinterpret_cast<const float*>(camera->raw);
  ClipPlanes& c = ds.clip;
  c.near_z = camera->get<float>(0xb0);
  c.far_z = camera->get<float>(0xb4);
  int32_t x = 0, y = 0, wd = 0, ht = 0;
  if (zcls::window_origin(camera->nodes[1], &x, &y)) x = 0, y = 0;
  if (zcls::window_size(camera->nodes[1], &wd, &ht)) wd = w.screen_w, ht = w.screen_h;
  const float fx = static_cast<float>(x), fy = static_cast<float>(y);
  const float fr = static_cast<float>(wd + x), fb = static_cast<float>(ht + y);
  if (zrndr::device_fog().active) {
    put_float(c.side_04, fx), put_float(c.side_08, fy);
    put_float(c.unknown_10, fr + 0.001f), put_float(c.unknown_14, fb + 0.001f);  // 0x005ce144 = -0.001
    put_float(c.side_1c, fr + 0.001f), put_float(c.side_20, fb + 0.001f);
  } else {
    put_float(c.side_04, (fx + 0.5f) - 0.999f), put_float(c.side_08, (fy + 0.5f) - 0.999f);
    put_float(c.unknown_10, fr + 1.499f), put_float(c.unknown_14, fb + 1.499f);
    put_float(c.side_1c, (fr + 0.5f) - 0.001f), put_float(c.side_20, (fb + 0.5f) - 0.001f);
  }
  ds.point_rect[0] = fx, ds.point_rect[1] = fy;
  ds.point_rect[2] = fr - 0.001f, ds.point_rect[3] = fb - 0.001f;
  zmth::set_projection(fx, fy, static_cast<float>(wd) * 0.5f, static_cast<float>(ht) * 0.5f,
                       camera->get<float>(0x1d4), camera->get<float>(0x1d8), camera->get<float>(0xb0),
                       camera->get<float>(0xb4));
  zmth::set_fov(camera->get<float>(0xe8), camera->get<float>(0xec));
}

// Mech3.exe 1.2 @ 0x005271a0. Sides: +x turned by half the horizontal fov
// about y and its mirror, -y turned by half the vertical fov about x and its
// mirror; then near (0,0,-1) and far (0,0,1). All point into the view.
void camera_local_planes(zcls::Camera* camera) {
  zmth::Mat34 a, b;
  float* p = camera->floats(0x13c);
  zmth::push(&a);
  zmth::load_identity();
  zmth::push_copy(&b);
  p[0] = 1.0f, p[1] = 0.0f, p[2] = 0.0f;
  zmth::rotate_y(camera->get<float>(0xf0));
  zmth::rotate_points(reinterpret_cast<zmth::Vec3*>(p), 1);
  zmth::pop();
  p[3] = -p[0], p[4] = p[1], p[5] = p[2];
  p[6] = 0.0f, p[7] = -1.0f, p[8] = 0.0f;
  zmth::rotate_x(camera->get<float>(0xf4));
  zmth::rotate_points(reinterpret_cast<zmth::Vec3*>(p + 6), 1);
  zmth::pop();
  p[9] = p[6], p[10] = -p[7], p[11] = p[8];
  p[12] = 0.0f, p[13] = 0.0f, p[14] = -1.0f;
  p[15] = 0.0f, p[16] = 0.0f, p[17] = 1.0f;
}

// Mech3.exe 1.2 @ 0x005272b0
void camera_world_planes(zcls::Camera* camera) {
  zmth::push(reinterpret_cast<zmth::Mat34*>(camera->floats(0x44)));
  zmth::rotate_points_to(reinterpret_cast<const zmth::Vec3*>(camera->floats(0x13c)),
                         reinterpret_cast<zmth::Vec3*>(camera->floats(0x188)), 6);
  zmth::pop();
}

}  // namespace zmod
