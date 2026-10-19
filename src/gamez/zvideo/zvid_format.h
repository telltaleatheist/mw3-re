#pragma once
#include <cstdint>

// The display's 16-bit pixel format and colour packing. Original file
// unknown: between zvid_init.c and zvid_ddd3d.c.
namespace zvid {

struct PixelFormat {
  int32_t r_bits = 5, g_bits = 6, b_bits = 5;  // 0x00802720, 0x00802724, 0x00802728
  int32_t r_shift = 8, g_shift = 3;            // 0x00802738, 0x0080273c
  int32_t b_shift_right = 3;                   // 0x00802740
  uint32_t r_mask = 0xf8, g_mask = 0xfc;       // 0x00802744, 0x00802748
  uint32_t b_mask = 0xf8;                      // 0x0080274c
};
PixelFormat& pixel_format();

void set_pixel_format(int r_bits, int g_bits, int b_bits);  // 0x005792c0
uint16_t pack_rgb(const float rgb[3]);                      // 0x005793c0

}  // namespace zvid
