#include "gamez/zvideo/zvid_format.h"

#include "crt/msvc_math.h"

namespace zvid {

namespace {
PixelFormat g_format;
}

PixelFormat& pixel_format() { return g_format; }

// Mech3.exe 1.2 @ 0x005792c0. The original also keeps three more arguments at
// 0x0080272c..0x00802734.
void set_pixel_format(int r_bits, int g_bits, int b_bits) {
  PixelFormat& f = g_format;
  f.r_bits = r_bits, f.g_bits = g_bits, f.b_bits = b_bits;
  f.r_shift = r_bits + g_bits + b_bits - 8;
  f.g_shift = g_bits + b_bits - 8;
  f.b_shift_right = 8 - b_bits;
  f.r_mask = ((1u << r_bits) - 1) << (8 - r_bits);
  f.g_mask = ((1u << g_bits) - 1) << (8 - g_bits);
  f.b_mask = ((1u << b_bits) - 1) << (8 - b_bits);
}

// Mech3.exe 1.2 @ 0x005793c0. Each channel is rounded (x - -0.5, then _ftol).
uint16_t pack_rgb(const float rgb[3]) {
  const PixelFormat& f = g_format;
  const uint32_t r = static_cast<uint16_t>(crt::msvc_ftol(rgb[0] + 0.5f));
  const uint32_t g = static_cast<uint32_t>(crt::msvc_ftol(rgb[1] + 0.5f));
  const uint16_t b = static_cast<uint16_t>(crt::msvc_ftol(rgb[2] + 0.5f));
  uint32_t v = (r & f.r_mask) << f.r_shift;
  v |= (g & f.g_mask) << f.g_shift;
  v |= static_cast<uint16_t>(b >> f.b_shift_right);
  return static_cast<uint16_t>(v);
}

}  // namespace zvid
