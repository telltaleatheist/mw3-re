#include "gamez/zmodel/gmod_cull.h"

#pragma STDC FP_CONTRACT OFF

#include "gamez/zclass/cls_data.h"
#include "gamez/zmodel/gmod_draw.h"
#include "gamez/zmodel/gmod_init.h"

namespace zmod {

namespace {
Zones g_zones;
ViewPolygon g_poly;
ShadowCasters g_casters;
CullHooks g_hooks;

float dot_from(const float* c, const float* p, const float* n) {
  const float d0 = c[0] - p[0], d1 = c[1] - p[1], d2 = c[2] - p[2];
  return d0 * n[0] + d1 * n[1] + d2 * n[2];
}
}  // namespace

Zones& zones() { return g_zones; }
ViewPolygon& view_polygon() { return g_poly; }
ShadowCasters& shadow_casters() { return g_casters; }
CullHooks& cull_hooks() { return g_hooks; }

// Mech3.exe 1.2 @ 0x0053cdc0. The exe reads as many ids as the count byte
// says, past the four bytes; the lists hold at most three.
int32_t zone_visible(uint8_t zone) {
  if (g_zones.mode == 0 || zone == 0xff) return 1;
  const int n = g_zones.list[0];
  if (n == 0) return 1;
  for (int i = 0; i < n && i < 3; ++i) {
    const uint8_t id = g_zones.list[1 + i];
    if (id == 0xff || id == zone) return 1;
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x0053cc40
void zone_reset(uint8_t list[4]) {
  if (!list) return;
  list[0] = 0, list[1] = list[2] = list[3] = 0xff;
}

// Mech3.exe 1.2 @ 0x00541760. Order: near, the four sides, far. A plane the
// sphere is wholly behind (d <= -r) rejects it; one it straddles (d < r)
// stays in the mask for the children.
uint32_t cull_sphere(const float centre[3], uint32_t* mask, float radius) {
  zcls::Camera* cam = walk_state().camera;
  const uint32_t test = *mask;
  *mask = 0;
  if (test & 0x10) {
    const float d = dot_from(centre, cam->floats(0xb8), cam->floats(0x1b8));
    if (d < radius) {
      if (!(-radius < d)) return 0x10;
      *mask = 0x10;
    }
  }
  const float* pos = cam->floats(0x2c);
  for (int i = 0; i < 4; ++i) {
    const uint32_t bit = 1u << i;
    if (!(test & bit)) continue;
    const float d = dot_from(centre, pos, cam->floats(0x188 + 12 * i));
    if (!(-radius < d)) return bit;
    if (d < radius) *mask |= bit;
  }
  if (test & 0x20) {
    const float d = dot_from(centre, cam->floats(0xc4), cam->floats(0x1c4));
    if (!(-radius < d)) return 0x20;
    if (d < radius) *mask |= 0x20;
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x0054fe80. Edge i runs from point i to point i+1 (the
// last back to 0); its normal (dz, 0, -dx) is normalised.
int32_t set_view_polygon(const float (*points)[3], int32_t count) {
  g_poly.count = 0;
  if (count > 64) return 0;
  for (int32_t i = count - 1; i >= 0; --i) {
    const float* next = points[i == count - 1 ? 0 : i + 1];
    for (int k = 0; k < 3; ++k) g_poly.points[i][k] = points[i][k];
    float* n = g_poly.normals[i];
    n[2] = points[i][0] - next[0];
    n[0] = next[2] - points[i][2];
    n[1] = 0.0f;
    normalize(n);
  }
  g_poly.count = count;
  return 1;
}

// Mech3.exe 1.2 @ 0x0054ff40. Inside (1) unless the point is more than
// -tolerance outside some edge.
int32_t inside_view_polygon(const float point[3], float tolerance) {
  for (int32_t i = 0; i < g_poly.count; ++i) {
    const float dx = point[0] - g_poly.points[i][0];
    const float dz = point[2] - g_poly.points[i][2];
    const float a = dx * g_poly.normals[i][0];
    const float b = dz * g_poly.normals[i][2];
    if (a + b < tolerance) return 0;
  }
  return 1;
}

// Mech3.exe 1.2 @ 0x005438d0
void use_all_casters() {
  g_casters.list = g_casters.all;
  g_casters.count = g_casters.all_count;
}

// Mech3.exe 1.2 @ 0x005438f0
void use_cell_casters() {
  g_casters.list = g_casters.cell;
  g_casters.count = g_casters.cell_count;
}

int32_t hidden_surface(const float centre[3], float radius) {
  return g_hooks.hidden_surface ? g_hooks.hidden_surface(g_hooks.ctx, centre, radius) : 1;
}

}  // namespace zmod
