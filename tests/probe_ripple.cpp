// zRender's ripple (0x00558dd0) for test_ripple.py: the port on the given
// screen and scratch, both returned.
#include <cstdint>
#include <cstring>

#include "gamez/zrender/zrndr_ripple.h"
#include "gamez/zrender/zrndr_tint.h"

extern "C" {
// screen: h rows of pitch_px pixels; scratch: w * h pixels (the exe's rows
// are the target's width). clip: 4 ints or null. precision: 24 or 53.
void probe_ripple(uint16_t* screen, int32_t w, int32_t h, int32_t pitch_px, uint16_t* scratch, int32_t x, int32_t y,
                  int32_t inner, int32_t outer, int32_t amplitude, float wavelength, float phase, const int32_t* clip,
                  int32_t precision) {
  zrndr::set_finish_target(screen, w, h, pitch_px * 2);
  auto& s = zrndr::ripple_state().scratch;
  s.assign(scratch, scratch + static_cast<size_t>(w) * h);
  zrndr::ripple(x, y, inner, outer, amplitude, wavelength, phase, clip,
                precision == 24 ? zrndr::X87::k24 : zrndr::X87::k53);
  std::memcpy(scratch, s.data(), sizeof(uint16_t) * static_cast<size_t>(w) * h);
}
}
