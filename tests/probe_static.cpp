// Probes for test_static.py (docs/hud.md section 21): zRender's noise table
// 0x005581c0 and the static fill 0x00558c10 over a buffer, rand from the trace.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zrender/zrndr_tint.h"
#include "probe_app.h"

namespace {
int32_t trace_rand() { return static_cast<int32_t>(probe_app::call("rand", {})); }
std::vector<uint16_t> g_pixels;
}  // namespace

extern "C" {

void probe_static_init() { zrndr::finish_init(trace_rand); }

// The table's 0x6400 bytes.
void probe_static_noise(uint8_t* out) { std::memcpy(out, zrndr::noise_table().data(), 0x6400); }

// A target of w x h pixels (pitch in pixels) holding `pixels`, then the fill
// on the rect (has_rect, left, top, right, bottom), the level's bits and the
// three colours; `pixels` gets the result.
void probe_static_fill(uint16_t* pixels, int32_t w, int32_t h, int32_t pitch_px, const int32_t* rect,
                       int32_t has_rect, uint32_t level_lo, uint32_t level_hi, uint32_t lo, uint32_t mid,
                       uint32_t hi) {
  g_pixels.assign(pixels, pixels + static_cast<size_t>(pitch_px) * h);
  zrndr::set_finish_target(g_pixels.data(), w, h, pitch_px * 2);
  const uint64_t b = static_cast<uint64_t>(level_hi) << 32 | level_lo;
  double level;
  std::memcpy(&level, &b, 8);
  zrndr::static_fill(has_rect ? rect : nullptr, level, static_cast<uint16_t>(lo), static_cast<uint16_t>(mid),
                     static_cast<uint16_t>(hi), trace_rand);
  std::memcpy(pixels, g_pixels.data(), g_pixels.size() * 2);
}

}  // extern "C"
