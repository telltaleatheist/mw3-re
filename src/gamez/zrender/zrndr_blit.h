#pragma once
#include <cstdint>

namespace zimg { struct Texture; }

// The software 2D blits zTwoD draws with (docs/twod.md, Drawing). Original
// file unknown: between zreader.cpp and zrndr_draw.c (0x0055b1f0..0x0055bbe0),
// next to the 2D target they draw into.
namespace zrndr {

// The locked 16-bit buffer (0x008007c8..0x008007d4), set by the video lock.
struct Target2D {
  uint8_t* base = nullptr;         // 0x008007c8
  uint32_t width = 0;              // 0x008007cc  pixels
  uint32_t height = 0;             // 0x008007d0
  uint32_t pitch = 0;              // 0x008007d4  bytes
  int32_t format = 6;              // 0x0074a738  6: RGB565, else RGB555 (the alpha blend)
};
Target2D& target2d();

// A source rectangle, as a Win32 RECT.
struct BlitRect {
  int32_t left, top, right, bottom;
};

// Mech3.exe 1.2 @ 0x0055b3c0, the 16-bit target's blit ([0x00800824]):
// `image` at (x, y), all of it or `src`'s part, clipped to the target. Pixels
// are 16-bit or palette indices; with an alpha plane each pixel blends by its
// alpha (none at 0..3, or 0..7 in 555; all from 0xfc), else with flag 0x02 the
// colour (or index) `key` is transparent.
void blit16(const zimg::Texture* image, int32_t x, int32_t y, uint16_t key, const BlitRect* src);

}  // namespace zrndr
