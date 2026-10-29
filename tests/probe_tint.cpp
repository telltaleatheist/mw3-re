// Probes for test_screen_tint.py (docs/anim-run.md, The screen tint): the
// animations' event 36 0x005074b0 through the weather manager's tint
// (0x005a6730 -> 0x005a6620, show_for 0x0059ff10), then the tint's draw
// 0x005a5360 -> 0x005a53c0 with 0x00558540's Direct3D quad recorded.
#include <cstdint>
#include <cstring>
#include <vector>

#include "gamez/zeffect/zeff_anim_run.h"
#include "gamez/zrender/zrndr_fog.h"
#include "gamez/zrender/zrndr_tint.h"
#include "gamez/ztwod/twod_weather.h"
#include "gamez/zvideo/zvid_dd.h"
#include "gamez/zvideo/zvid_format.h"

namespace {
uint32_t fb(float f) {
  uint32_t i;
  std::memcpy(&i, &f, 4);
  return i;
}
float bf(uint32_t i) {
  float f;
  std::memcpy(&f, &i, 4);
  return f;
}
ztwod::Rect g_rects[2];
std::vector<uint32_t> g_quads;  // per draw: colour, has rect, l t r b, the alpha's two dwords
}  // namespace

extern "C" {

// One event: its 0x40 bytes, the sequence's time and the time left (bits),
// the tint's flags and timer before, which manager rects are set (bit 0, 1)
// and their 8 ints. out: the result, the time left after, the colour, the
// alpha's dwords, the flags and timer after, then the draw's quads (count
// first) and which rect +0x34 holds after (0 none, 1, 2).
int32_t probe_tint_run(const uint8_t* ev, uint32_t now, uint32_t left, uint32_t flags, uint32_t timer,
                       int32_t rects, const int32_t* r8, uint32_t* out) {
  zvid::set_pixel_format(5, 6, 5);
  zvid::text_target().pixels = nullptr;
  zrndr::device_fog().active = 1;
  zrndr::tint_hooks().quad = [](uint16_t colour, const int32_t* rect, double alpha) {
    uint64_t q;
    std::memcpy(&q, &alpha, sizeof q);
    g_quads.push_back(colour);
    g_quads.push_back(rect ? 1 : 0);
    for (int i = 0; i < 4; ++i) g_quads.push_back(rect ? static_cast<uint32_t>(rect[i]) : 0);
    g_quads.push_back(static_cast<uint32_t>(q));
    g_quads.push_back(static_cast<uint32_t>(q >> 32));
  };
  zeff::anim_run_hooks().screen_tint = [](void*, uint16_t colour, double alpha) { ztwod::weather_tint(colour, alpha); };
  ztwod::WeatherLayer& l = ztwod::weather_layer();
  for (int i = 0; i < 2; ++i) {
    g_rects[i] = {r8[4 * i], r8[4 * i + 1], r8[4 * i + 2], r8[4 * i + 3]};
    l.rects[i] = rects & (1 << i) ? &g_rects[i] : nullptr;
  }
  ztwod::ScreenTint& t = l.tint;
  t.flags = flags, t.timer = bf(timer), t.rect = nullptr;
  std::vector<uint8_t> e(ev, ev + 0x40);
  zeff::SeqDef seq{};
  *seq.floats(0x28) = bf(now);
  zeff::anim().time_left() = bf(left);
  out[0] = static_cast<uint32_t>(zeff::anim_fbfx_color(seq, e.data()));
  out[1] = fb(zeff::anim().time_left());
  uint64_t q;
  std::memcpy(&q, &t.alpha, sizeof q);
  out[2] = t.colour, out[3] = static_cast<uint32_t>(q), out[4] = static_cast<uint32_t>(q >> 32);
  out[5] = t.flags, out[6] = fb(t.timer);
  g_quads.clear();
  t.draw();
  out[7] = static_cast<uint32_t>(g_quads.size() / 8);
  for (size_t i = 0; i < g_quads.size(); ++i) out[8 + i] = g_quads[i];
  out[8 + g_quads.size()] = t.rect == nullptr ? 0 : t.rect == &g_rects[0] ? 1 : 2;
  zrndr::tint_hooks().quad = nullptr;
  zrndr::device_fog().active = 0;
  return 0;
}

}  // extern "C"
