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

}  // namespace zrndr
