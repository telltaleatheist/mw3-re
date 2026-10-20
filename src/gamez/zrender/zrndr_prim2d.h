#pragma once
#include <cstdint>

// zRender's 2D primitives on the locked 16-bit target (zrndr::target2d()),
// the ones zTwoD's HUD objects draw with (docs/twod.md, Drawing; hud.md).
// Original file unknown: 0x00562940..0x00564300, after the 2D line entry,
// named after what it holds. A clip rect is left, top, right, bottom, all
// inclusive. FIX (bug): where the original writes a pixel without a
// clip rect, a pixel off the target is skipped here (it wrote past the
// surface; docs/fixes.md). The jittered ones draw from MSVCRT's rand()
// (crt::msvc_rand), as the original.
namespace zrndr {

// 0x00562c80 ([0x008008c4] = 0x00563c50): a dashed line, on and off every
// (length + 1) / dash pixels along the longer axis.
void line16_dashed(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t pixel, int32_t dash);
// 0x00563db0 ([0x008008c8]): the line clipped to `clip` (both ends' outcodes,
// the clipped end moved along the float slope), or nothing.
void line16_clipped(const int32_t clip[4], int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t pixel);
// 0x00562cb0: `count` segments joining points[0..count] (x, y pairs), each clipped.
void line16_list(const int32_t* points, int32_t count, const int32_t clip[4], uint16_t pixel);
// 0x005630a0: one pixel.
void pixel16(int32_t x, int32_t y, uint16_t pixel);
// 0x005630c0: a midpoint circle of radius r around (cx, cy), only inside
// `clip` when given.
void circle16(int32_t cx, int32_t cy, int32_t r, uint16_t pixel, const int32_t* clip);
// 0x00563440: the circle with every point thrown up to r * noise * 0.1
// either way on each axis.
void circle16_noisy(int32_t cx, int32_t cy, int32_t r, uint16_t pixel, float noise, const int32_t* clip);
// 0x00562970: a line whose pixels are thrown about by its length * noise * 0.1
// (the length by the exe's bit-trick square root), only inside `clip` when given.
void line16_noisy(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t pixel, float noise, const int32_t* clip);

}  // namespace zrndr
