#pragma once
#include <cstdint>

// A screen-facing textured square (0x00567720), the puffers' sprite. Original
// file unknown: between zrndr_draw.c and zsnd_parm.cpp, most likely
// zrndr_draw.c (docs/puffers.md, drawing).
namespace zimg { struct TextureEntry; }

namespace zrndr {

// ECX screen (x, y, 1/z), EDX texture entry, stack half size in pixels,
// alpha, coloured. Under 1 pixel nothing is drawn; the square is cut to the
// model renderer's screen rect with its texture coordinates, then drawn
// through [0x72f3c0] with the work records' colours (coloured) or
// [0x72f3bc] white. On Direct3D only: the software path (0x00566620) is not
// ported (render.md section 9).
void draw_sprite(const float screen[3], const zimg::TextureEntry* texture, float half_size, float alpha,
                 int32_t colored);  // 0x00567720

// zRender's GfxFlags ([0x008008d8] -> the settings entry "GfxFlags_HW" /
// "_SW", else [0x008008d4]): read by the software renderer's set-up
// (0x00557b30: bits 2, 8, 0x10) and its tint finish (0x00558610: bit 4, MMX
// rows; the port's finish_machine). The program draws through Direct3D.
struct GfxFlags {
  uint32_t base = 0;               // 0x008008d4
  const uint32_t* flags = &base;   // 0x008008d8
};
GfxFlags& gfx_flags();
void gfx_flags_start(bool hardware);                    // 0x005578f0's part (zRender's start)
void use_gfx_flags(const char* name, uint32_t flags);  // 0x00557ae0 (ECX name, EDX flags)

}  // namespace zrndr
