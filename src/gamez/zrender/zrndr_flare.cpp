#include "gamez/zrender/zrndr_flare.h"

#include <cstring>

#include "gamez/zmath/zmth_matrix.h"
#include "gamez/zmodel/gmod_draw.h"
#include "gamez/zrender/zrndr_draw.h"
#include "gamez/zrender/zrndr_fog.h"

#pragma STDC FP_CONTRACT OFF

namespace zrndr {

namespace {
Flares g_flares;
float as_float(uint32_t bits) {
  float f;
  std::memcpy(&f, &bits, 4);
  return f;
}
float field(const uint8_t* p, int off) {
  float f;
  std::memcpy(&f, p + off, 4);
  return f;
}
// [0x0072f348]: a Direct3D device (the draw functions' own test).
bool hardware() { return device_fog().active != 0; }
}  // namespace

Flares& flares() { return g_flares; }
FlareHooks& flare_hooks() {
  static FlareHooks h;
  return h;
}

// Mech3.exe 1.2 @ 0x00567420
void flare_add(const float world[3], const float screen[3], uint32_t color, const uint8_t* light) {
  Flares& f = g_flares;
  if (f.count >= Flares::kMax) return;
  Flare& r = f.list[f.count];
  r.world[0] = world[0], r.world[1] = world[1], r.world[2] = world[2];
  r.screen[0] = screen[0], r.screen[1] = screen[1], r.screen[2] = screen[2];
  r.color = color;
  r.light = light;
  ++f.count;
}

// Mech3.exe 1.2 @ 0x005674d0 / 0x00567570
int32_t flare_count() { return g_flares.count; }
void flares_clear() { g_flares.count = 0; }

// Mech3.exe 1.2 @ 0x00567650
int32_t flares_pick(int32_t from) {
  Flares& f = g_flares;
  f.picked_count = 0;
  for (int32_t i = from; i < f.count; ++i) {
    const Flare& r = f.list[i];
    int32_t on = 0;
    if (r.light) std::memcpy(&on, r.light + 0xc, 4);
    if (r.light && on != 0 && i < Flares::kPickable && r.screen[2] != 0.0f) f.picked[f.picked_count++] = &f.list[i];
  }
  return f.picked_count;
}

// Mech3.exe 1.2 @ 0x005676c0
Flare* flare_picked(int32_t i) { return g_flares.picked[i]; }

// Mech3.exe 1.2 @ 0x005676d0
int32_t flare_texture(int32_t index, const zimg::TextureEntry* texture) {
  Flares& f = g_flares;
  if (index >= 0 && index < 4) f.textures[index] = texture;
  f.on = f.textures[0] && f.textures[1] && f.textures[2] && f.textures[3] ? 1 : 0;
  return 0;
}

// Mech3.exe 1.2 @ 0x00567b90. The screen's centre is the clip rect's left and
// top with the point rect's right and bottom (0x007402fc, 0x00740300,
// 0x00740324, 0x00740328); depth 0.99 (10 without a device), alpha 0.5.
void flare_quads(const float screen[3], float scale) {
  const zmod::DrawState& s = zmod::draw_state();
  const float left = as_float(s.clip.side_04), top = as_float(s.clip.side_08);
  const float right = s.point_rect[2], bottom = s.point_rect[3];
  const float cx = (left + right) * 0.5f, cy = (top + bottom) * 0.5f;  // 0x005ce640
  const float dx = screen[0] - cx, dy = screen[1] - cy;
  const float z = hardware() ? 0.99f : 10.0f;
  const float size = (right - left) * scale * 0.0625f;  // 0x005ce64c
  const zimg::TextureEntry* const* t = g_flares.textures;
  auto draw_sprite = [](const float* p, const zimg::TextureEntry* tex, float sz, float a, int32_t c) {
    if (flare_hooks().sprite) flare_hooks().sprite(p, tex, sz, a, c);
    else zrndr::draw_sprite(p, tex, sz, a, c);
  };
  const float p0[3] = {screen[0], screen[1], z};
  draw_sprite(p0, t[0], size, 0.5f, 0);
  const float p1[3] = {cx - dx * -0.5f, cy - dy * -0.5f, z};  // 0x005ce650
  draw_sprite(p1, t[1], size, 0.5f, 0);
  const float p2[3] = {cx - dx * -0.1f, cy - dy * -0.1f, z};  // 0x005ce654
  draw_sprite(p2, t[2], size * 0.5f, 0.5f, 0);
  // The last mirrors x through the centre but keeps y on the light's side.
  const float p3[3] = {cx - dx, dy + cy, z};
  draw_sprite(p3, t[3], size * 1.5f, 0.5f, 0);  // 0x005ce658
}

// Mech3.exe 1.2 @ 0x00567d30. The x87's "below" is also true for NaN.
void flare_draw(int32_t i) {
  const Flares& f = g_flares;
  const Flare& r = *f.picked[i];
  if (!f.on) return;
  const float q = hardware() ? zmth::projection().near_z : 1.0f;  // [0x0073fb80], else 0x005ce638
  const float d = q / r.screen[2];
  const float full = field(r.light, 0x14), gone = field(r.light, 0x18);
  if (d >= gone) return;
  const float scale = !(d >= full) ? 1.0f : (gone - d) / (gone - full);
  flare_quads(r.screen, scale);
}

}  // namespace zrndr
