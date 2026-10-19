#include "gamez/zclass/cls_world.h"

#include <cmath>

#pragma STDC FP_CONTRACT OFF

#include "gamez/zclass/cls_data.h"
#include "gamez/zclass/list.h"
#include "gamez/zmodel/gmod_fog.h"
#include "gamez/zrender/zrndr_fog.h"

namespace zcls {

namespace {
AreaUpdate g_area_update = nullptr;

World* world_of(Node* n) { return static_cast<World*>(n->data); }
void mark(World* w, uint32_t bit) { w->put<uint32_t>(0, w->get<uint32_t>(0) | bit); }
}  // namespace

void set_area_update(AreaUpdate fn) { g_area_update = fn; }

// Mech3.exe 1.2 @ 0x004f23e0
int world_set_fog_type(Node* world, uint32_t type) {
  World* w = world_of(world);
  w->put(0x10, type);
  mark(w, kFogType);
  return 0;
}

// Mech3.exe 1.2 @ 0x004f23f0
int world_set_fog_color(Node* world, float r, float g, float b) {
  World* w = world_of(world);
  w->put(0x14, r), w->put(0x18, g), w->put(0x1c, b);
  mark(w, kFogColor);
  return 0;
}

// Mech3.exe 1.2 @ 0x004f2420
int world_set_fog_altitude(Node* world, float at_2c, float at_28) {
  World* w = world_of(world);
  w->put(0x28, at_28), w->put(0x2c, at_2c);
  mark(w, kFogAltitude);
  return 0;
}

// Mech3.exe 1.2 @ 0x004f2440
int world_set_fog_range(Node* world, float near_range, float far_range) {
  World* w = world_of(world);
  w->put(0x20, near_range), w->put(0x24, far_range);
  mark(w, kFogRange);
  return 0;
}

// Mech3.exe 1.2 @ 0x004f2460
int world_set_fog_density(Node* world, float density) {
  World* w = world_of(world);
  w->put(0x30, density);
  mark(w, kFogDensity);
  return 0;
}

// Mech3.exe 1.2 @ 0x004f1df0. With more than one world every setting is
// pushed. Fog type 0 is off, 1 linear, anything else on but not linear.
int world_push_fog(Node* world) {
  World* w = world_of(world);
  if (list_count(kWorlds) > 1) w->put<uint32_t>(0, 0x2f);
  const uint32_t changed = w->get<uint32_t>(0);
  if (changed == 0) return 0;
  bool any = false;
  if (changed & kFogType) {
    any = true;
    const uint32_t type = w->get<uint32_t>(0x10);
    if (type == 0) {
      zmod::fog_set_on(0), zmod::fog_set_linear(0), zmod::fog_set_density(0);
    } else {
      zmod::fog_set_on(1), zmod::fog_set_linear(type == 1);
    }
  }
  if (changed & kFogColor) {
    any = true;
    zrndr::set_fog_color(w->floats(0x14));
    zmod::fog_set_color(w->floats(0x14));
  }
  if (changed & kFogRange) {
    any = true;
    zmod::fog_set_near(w->get<float>(0x20)), zmod::fog_set_far(w->get<float>(0x24));
  }
  if (changed & kFogAltitude) {
    any = true;
    zmod::fog_set_alt_high(w->get<float>(0x28)), zmod::fog_set_alt_low(w->get<float>(0x2c));
  }
  if (changed & kFogDensity) {
    any = true;
    zmod::fog_set_density(w->get<float>(0x30));
  }
  if (any) zmod::fog_apply();  // after 0x005680b0, which does nothing
  if (w->get<int32_t>(4) > 0) {
    uint32_t** e = w->area_pending;
    int32_t n;
    do {
      uint32_t* entry = *e++;
      if (g_area_update) g_area_update(w, entry);
      *entry &= ~1u;
      n = w->get<int32_t>(4) - 1;
      w->put(4, n);
    } while (n > 0);
  }
  w->put<uint32_t>(0, 0);
  return 0;
}

// Mech3.exe 1.2 @ 0x004f1f10. Left of the grid the cell counts back from the
// block's right edge (the block start +0x5c is not added there); right of
// it, forward from the start. Rows the same with +0x58 / +0x8c / +0x60.
int world_wrap_cell(Node* world, int32_t x, int32_t z, int32_t* out_x, int32_t* out_z,
                    int32_t* inside) {
  const World* w = world_of(world);
  *inside = 1;
  if (x < 0) {
    *inside = 0;
    *out_x = w->get<int32_t>(0x54) - (-1 - x) % w->get<int32_t>(0x54) - 1;
  } else if (x >= w->get<int32_t>(0x88)) {
    *inside = 0;
    *out_x = (x - w->get<int32_t>(0x88)) % w->get<int32_t>(0x54) + w->get<int32_t>(0x5c);
  } else {
    *out_x = x;
  }
  if (z < 0) {
    *inside = 0;
    *out_z = w->get<int32_t>(0x58) - (-1 - z) % w->get<int32_t>(0x58) - 1;
  } else if (z >= w->get<int32_t>(0x8c)) {
    *inside = 0;
    *out_z = (z - w->get<int32_t>(0x8c)) % w->get<int32_t>(0x58) + w->get<int32_t>(0x60);
  } else {
    *out_z = z;
  }
  return 0;
}

// Mech3.exe 1.2 @ 0x004f1fe0. Grid origin +0x34 / +0x38, inverse cell size
// +0x74 / +0x78 (z runs negative).
int world_cell_wrapped(Node* world, int32_t* cell_x, float x, float z, int32_t* cell_z,
                       int32_t* out_x, int32_t* out_z, int32_t* inside) {
  const World* w = world_of(world);
  const float dz = z - w->get<float>(0x38);
  const float fx = (x - w->get<float>(0x34)) * w->get<float>(0x74);
  *cell_x = static_cast<int32_t>(std::floor(static_cast<double>(fx)));
  const float fz = w->get<float>(0x78) * dz;
  *cell_z = static_cast<int32_t>(std::floor(static_cast<double>(fz)));
  return world_wrap_cell(world, *cell_x, *cell_z, out_x, out_z, inside);
}

// Mech3.exe 1.2 @ 0x004f2060. x is kept 0.1 inside [+0x34, +0x44], z inside
// [+0x48, +0x38]; z at or below +0x48 + 0.1 takes the edge.
int world_cell_clamped(Node* world, int32_t* cell_x, float x, float z, int32_t* cell_z) {
  const World* w = world_of(world);
  float cx;
  if (!(static_cast<float>(w->get<float>(0x34) - -0.1) <= x)) {  // 0x005cdb18, a double
    cx = w->get<float>(0x34) - -0.1f;
  } else {
    cx = w->get<float>(0x44) - 0.1f;
    if (x < cx) cx = x;
  }
  float cz = w->get<float>(0x38) - 0.1f;
  if (!(z > cz)) {
    cz = w->get<float>(0x48) - -0.1f;
    if (!(z <= cz)) cz = z;
  }
  *cell_x = static_cast<int32_t>((cx - w->get<float>(0x34)) * w->get<float>(0x74));
  *cell_z = static_cast<int32_t>(w->get<float>(0x78) * (cz - w->get<float>(0x38)));
  return 0;
}

}  // namespace zcls
