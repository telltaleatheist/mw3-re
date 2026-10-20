#pragma once
#include <cstdint>

// The renderer's fog colour (docs/gamez.md, world fog). Original file
// unknown: between zrndr_draw.c and zsnd_parm.cpp.
namespace zrndr {

// The Direct3D device's copy, kept only while a device is up.
struct DeviceFog {
  int32_t active = 0;              // 0x0072f348
  float color[3] = {};             // 0x0072f3f4  0..255
  float sent[3] = {};              // 0x0072f400
  void (*send)() = nullptr;        // 0x0072f3f0  FIX (A): the device's fog call, set by the front end
};
DeviceFog& device_fog();
void device_fog_color(const float rgb[3]);  // the block 0x00526cf0 and 0x00567eb0 share

// The software renderer's pixel layout for the fog ramp.
struct SoftwareFormat {
  uint32_t r_mask = 0;             // 0x0074a740
  uint32_t g_mask = 0;             // 0x0074a744
  int32_t r_shift = 0;             // 0x0074a74c
  int32_t g_shift = 0;             // 0x0074a750
  int32_t b_shift_right = 0;       // 0x0074a754
};
SoftwareFormat& software_format();

struct FogColor {                  // 0x00800548
  float rgb[3] = {};
  uint32_t r = 0, g = 0, b = 0;    // +0x0c, +0x10, +0x14
  uint16_t packed = 0;             // +0x18
  uint32_t doubled = 0;            // +0x1c
  int32_t ramp[32] = {};           // +0x20  ramp[31 - i] = i * step
};
FogColor& fog_color();
FogColor& light_color();           // 0x008005e8, the same layout for the light colour

void set_light_color(float rgb[3]);                        // 0x00568370, clamps rgb in place

void set_fog_color(float rgb[3]);                          // 0x00567eb0, clamps rgb in place
void set_fog_ramp(FogColor& f, uint32_t r, uint32_t g, uint32_t b);  // 0x0056c170

}  // namespace zrndr
