#pragma once
#include <cstdint>

// The 16-bit software rasterizer's span routines (zRender; the original file has
// no assertion strings, it sits just before zrndr_draw.c). See docs/blit-trick.md.
namespace zrndr {

// The globals every span routine reads, at their original addresses.
struct SpanState {
  uint8_t* dest = nullptr;              // 0x0072c278  start of the span's pixels
  const uint16_t* texture = nullptr;    // 0x0072c268
  uint32_t du = 0;                      // 0x0072c270
  uint32_t dv = 0;                      // 0x0072c274
  uint32_t v_mask = 0;                  // 0x005efb98
};

SpanState& span_state();

// Mech3.exe 1.2 @ 0x00564080: fill `count` pixels with `color`.
void span_fill16(uint32_t color, uint32_t count);

// Mech3.exe 1.2 @ 0x0056cb40: `count` texels along one span, u and v in fixed point.
void span_textured16(uint32_t u, uint32_t v, int32_t count);

}  // namespace zrndr
