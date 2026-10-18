// C entry points over ported functions, loaded by tests/differential/*.py to compare
// them with the original x86 code running in an emulator.
#include <cstdint>

#include "gamez/zrender/zrndr_span.h"

extern "C" {

void probe_span_fill16(uint16_t* dest, uint32_t color, uint32_t count) {
  zrndr::span_state().dest = reinterpret_cast<uint8_t*>(dest);
  zrndr::span_fill16(color, count);
}

void probe_span_textured16(uint16_t* dest, const uint16_t* texture, uint32_t du, uint32_t dv,
                           uint32_t v_mask, uint32_t u, uint32_t v, int32_t count) {
  zrndr::SpanState& s = zrndr::span_state();
  s.dest = reinterpret_cast<uint8_t*>(dest);
  s.texture = texture;
  s.du = du;
  s.dv = dv;
  s.v_mask = v_mask;
  zrndr::span_textured16(u, v, count);
}

}  // extern "C"

#include "gamez/zutil/ztimer.h"
#include "platform/platform.h"

namespace {
struct ProbeClock : platform::Clock {
  uint32_t now = 0;
  uint32_t ticks_ms() override { return now; }
};
ProbeClock g_probe_clock;
}  // namespace

extern "C" {

// Runs ztimer::update() for each tick value; writes dt and raw_dt per frame.
void probe_ztimer(const uint32_t* ticks, int frames, float scale_on_frame, int scale_frame,
                  float* dt_out, float* raw_out) {
  ztimer::state() = ztimer::State{};
  ztimer::set_clock(&g_probe_clock);
  for (int i = 0; i < frames; ++i) {
    if (i == scale_frame) ztimer::state().scale = scale_on_frame;
    g_probe_clock.now = ticks[i];
    ztimer::update();
    dt_out[i] = ztimer::state().dt;
    raw_out[i] = ztimer::state().raw_dt;
  }
  ztimer::set_clock(nullptr);
}

}  // extern "C"
