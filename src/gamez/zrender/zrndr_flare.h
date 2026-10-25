#pragma once
// Lens flares (0x00567420..0x00567dc0; docs/render.md, Lens flares): the model
// lights flagged +0x3c go on a list as the scene is walked, and the Direct3D
// frame draws the ones in sight as four screen squares (zclass's
// camera_draw_flares, 0x004f7350). Original file unknown: beside the puffers'
// sprite (zrndr_draw.cpp), most likely zrndr_draw.c.
#include <cstdint>

namespace zimg { struct TextureEntry; }

namespace zrndr {

struct Flare {              // 0x20 bytes (0x007fb304 + 0x20 i)
  float world[3];           // +0x00  the light in world space (camera space by 0x0073fc00)
  float screen[3];          // +0x0c  x, y and the priority-biased depth of its point
  uint32_t color;           // +0x18  the light's colour (low word)
  const uint8_t* light;     // +0x1c  the light's +0x30: +0x0c on, +0x14 full, +0x18 gone
};

struct Flares {
  static constexpr int32_t kMax = 650;     // 0x28a
  static constexpr int32_t kPickable = 64;  // only the first 64 can be picked (0x007fbb20)
  int32_t count = 0;                        // 0x007fb300
  Flare list[kMax] = {};                    // 0x007fb304
  int32_t picked_count = 0;                 // 0x00800444
  Flare* picked[kPickable] = {};            // 0x00800448
  const zimg::TextureEntry* textures[4] = {};  // 0x0072c258  LensFlareTexture 0..3
  int32_t on = 0;                           // 0x0072c250  all four set
};
Flares& flares();
// The square each of the four goes through; unset, draw_sprite (0x00567720).
struct FlareHooks {
  void (*sprite)(const float screen[3], const zimg::TextureEntry* texture, float half_size, float alpha,
                 int32_t colored) = nullptr;
};
FlareHooks& flare_hooks();

// Mech3.exe 1.2 @ 0x00567420 (ECX world point, EDX screen point, color,
// light +0x30): one more flare, while there is room. The software path's
// depth rescale ([0x0074a3c4], [0x0074a3c0]) is not ported.
void flare_add(const float world[3], const float screen[3], uint32_t color, const uint8_t* light);
int32_t flare_count();  // 0x005674d0
void flares_clear();    // 0x00567570
// Mech3.exe 1.2 @ 0x00567650 (ECX the first): from it on, the flares whose
// light is on, within the first 64, with a depth; the count picked.
int32_t flares_pick(int32_t from);
Flare* flare_picked(int32_t i);  // 0x005676c0
// Mech3.exe 1.2 @ 0x005676d0 (ECX index, EDX texture): the four textures
// (LensFlareTexture); flares are drawn only with all four.
int32_t flare_texture(int32_t index, const zimg::TextureEntry* texture);
// Mech3.exe 1.2 @ 0x00567b90 (ECX the screen point, the scale): the four
// squares, one on the light and three on the line through the screen's
// centre (the last mirrored in x only, as written).
void flare_quads(const float screen[3], float scale);
// Mech3.exe 1.2 @ 0x00567d30 (ECX the picked index): its size from its depth,
// full nearer than the light's +0x44, gone past +0x48.
void flare_draw(int32_t i);

}  // namespace zrndr
