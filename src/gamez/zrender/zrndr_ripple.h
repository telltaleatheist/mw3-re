#pragma once
#include <cstdint>
#include <vector>

#include "gamez/zrender/zrndr_span.h"

// File unknown: between zreader.cpp and zrndr_draw.c (0x005581c0..0x00559ec0),
// named after zRender. The main menu's ripple around a button
// (docs/mainmenu.md, docs/render-ripple.md).
namespace zrndr {

// The ripple's state: the scratch the ring is built in, the centre and clip
// of the last call.
struct RippleState {
  // 0x0072c1e0: malloc(0x180000) by 0x005581c0, rows as wide as the finish
  // target. FIX (B): sized to the target (the exe's 1.5 MB ends at 1024x768).
  std::vector<uint16_t> scratch;
  int32_t cx = 0, cy = 0;          // 0x0072c1b0, 0x0072c1b4  the clipped path's centre
  int32_t clip[4] = {};            // 0x0072c1c0..0x0072c1cc  left, top, right, bottom
};
RippleState& ripple_state();

// Mech3.exe 1.2 @ 0x00558dd0 (ECX x, EDX y; inner, outer, amplitude,
// wavelength, phase, clip): the finish target's pixels from `inner` to
// `outer` around (x, y) shifted along the radius by amplitude x
// sin(r / wavelength + phase), through the scratch, and the square around it
// copied back. `clip` is left, top, right, bottom (null: the target).
void ripple(int32_t x, int32_t y, int32_t inner, int32_t outer, int32_t amplitude, float wavelength, float phase,
            const int32_t* clip, X87 precision);

}  // namespace zrndr
