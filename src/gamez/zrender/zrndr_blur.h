#pragma once
#include <cstdint>

// File unknown: between zreader.cpp and zrndr_draw.c (0x00559ec0..0x0055a540),
// named after zRender. The screen blur of zFMV's BLUR actions and the pause
// screen (docs/fmv.md, The blur): each pixel averaged with its neighbours,
// 1/4 + 1/2 + 1/4, per channel under the display's masks, on the finish
// target (0x00558290) through the ripple's scratch.
namespace zrndr {

// The rect is left, top, right, bottom (inclusive), null for the whole target;
// each pass clamps it to the target its own way.
void blur_h(const int32_t* rect);     // 0x0055a3b0: along the rows
void blur_v(const int32_t* rect);     // 0x0055a1b0: down the columns
void blur_both(const int32_t* rect);  // 0x00559ec0: down, then along

// Mech3.exe 1.2 @ 0x0055a540 (ECX rect, EDX kind): 1 along, 2 down, else both.
void blur(const int32_t* rect, int32_t kind);

}  // namespace zrndr
