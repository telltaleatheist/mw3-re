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

// The software target the polygon fillers draw into (0x00557f90 sets it).
struct SoftTarget {
  uint8_t* base = nullptr;         // 0x008007c8  pixel (0, 0)
  int32_t width = 0, height = 0;   // 0x008007cc / 0x008007d0  rect right - left, bottom - top
  int32_t pitch = 0;               // 0x008007d4  bytes per row
  int32_t bytes = 2;               // 0x008007d8  bytes per pixel
  int32_t rect[4] = {};            // 0x008007e4..0x008007f0  left, top, right, bottom
  int32_t screen_w = 0;            // 0x0074a75c  (set by the renderer's init)
  int32_t screen_row = 0;          // 0x0074a76c  screen_w * bytes
  // [0x00800828]: the solid span routine, 0x00564080 for a 16-bit display (0x00557bf2).
  void (*fill)(uint32_t color, uint32_t count) = span_fill16;
  // 0x0074a3c8: 1 when the polygons wind so that walking up the vertex list
  // is the left edge (the shadow images set it; the 3D software path per polygon).
  int32_t winding = 0;
};
SoftTarget& soft_target();

// Mech3.exe 1.2 @ 0x00557f90 (ECX pixels, EDX rect or null, stack depth in
// bits or 0, pitch): the target of the software fillers.
void set_target(uint8_t* base, const int32_t* rect, int32_t depth, int32_t pitch);

// The x87 precision 0x0055d010 runs at. Its 16.16 conversions use the
// 1.5 * 2^52 rounding trick, which needs double precision; at 24 bits (a
// Direct3D 6 that truncates the FPU; not so in play, Owen 2026-10-20) every edge and row comes
// out 0 and nothing is filled (docs/render-shadows.md).
enum class X87 { k24, k53 };

// Mech3.exe 1.2 @ 0x0055d010 (ECX vertices, x and y every 12 bytes, EDX count,
// stack colour): a flat polygon into soft_target(), rows by pixel centres, no
// clipping. FIX (bug): see fill_polygon in the .cpp (a level edge's entry).
void fill_polygon(const float* vertices, int32_t count, uint32_t color, X87 precision);

}  // namespace zrndr
