#include "gamez/zutil/ztimer.h"

#include "platform/platform.h"

namespace ztimer {

namespace {
State g_state;
platform::Clock* g_clock = nullptr;
}  // namespace

State& state() { return g_state; }
void set_clock(platform::Clock* clock) { g_clock = clock; }

// Mech3.exe 1.2 @ 0x00575a00
//
// FIX (B): the original reads GetTickCount(), milliseconds since the machine
// booted, and converts it to float seconds. A float holds 24 bits of precision,
// so once uptime passes about 4.7 hours the clock moves in steps of 1 ms or more,
// and after 10 days in steps of 64 ms: frame times quantise and the game
// stutters. Machines that sleep instead of shutting down get there easily.
// platform::Clock counts from game start instead; the arithmetic is unchanged.
//
// x87: the product ticks * 0.001f stays on the FPU stack at 80 bits while it is
// stored to now_s as a float and subtracted from prev_s, so raw_dt is rounded
// once, from the unrounded product. Double reproduces that exactly while ticks
// stay under 2^29 (six days of play).
void update() {
  State& s = g_state;
  const uint32_t ticks = g_clock ? g_clock->ticks_ms() : 0;
  const float scale = s.scale;
  const double now = static_cast<double>(ticks) * static_cast<double>(0.001f);  // 0x005ce748
  s.now_s = static_cast<float>(now);
  s.scale = 1.0f;
  s.raw_dt = static_cast<float>(now - static_cast<double>(s.prev_s));
  s.dt = scale * s.raw_dt;
  s.total_raw = s.total_raw + s.raw_dt;
  if (s.clamp_enabled != 0 && s.max_dt < s.dt) s.dt = s.max_dt;
  s.prev_s = s.now_s;
  s.total_scaled = s.total_scaled + s.dt;
}

// Mech3.exe 1.2 @ 0x00575ab0: sets the lowest frame rate the clamp allows and
// returns it; a non-positive argument only reads it back.
float set_max_rate(float fps) {
  if (0.0f < fps) {
    g_state.max_dt = 1.0f / fps;
    return fps;
  }
  return 1.0f / g_state.max_dt;
}

// Mech3.exe 1.2 @ 0x00575af0
void set_clamp_enabled(int on) { g_state.clamp_enabled = on; }

}  // namespace ztimer
