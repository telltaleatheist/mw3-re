#include "check.h"
#include "gamez/zutil/ztimer.h"
#include "platform/platform.h"

namespace {
struct FakeClock : platform::Clock {
  uint32_t now = 0;
  uint32_t ticks_ms() override { return now; }
  ~FakeClock() override { ztimer::set_clock(nullptr); }
};
void reset() { ztimer::state() = ztimer::State{}; }
}  // namespace

TEST(ztimer_step_follows_the_clock) {
  reset();
  FakeClock clock;
  ztimer::set_clock(&clock);
  clock.now = 100;
  ztimer::update();
  clock.now = 133;
  ztimer::update();
  EXPECT(ztimer::state().raw_dt > 0.0329f && ztimer::state().raw_dt < 0.0331f);
  EXPECT(ztimer::state().dt == ztimer::state().raw_dt);
}

TEST(ztimer_clamps_long_frames_to_an_eighth_of_a_second) {
  reset();
  FakeClock clock;
  ztimer::set_clock(&clock);
  clock.now = 0;
  ztimer::update();
  clock.now = 2000;  // a two-second hitch
  ztimer::update();
  EXPECT(ztimer::state().dt == 0.125f);
  EXPECT(ztimer::state().raw_dt == 2.0f);
}

TEST(ztimer_scale_applies_to_one_frame_only) {
  reset();
  FakeClock clock;
  ztimer::set_clock(&clock);
  clock.now = 0;
  ztimer::update();
  ztimer::state().scale = 0.5f;
  clock.now = 100;
  ztimer::update();
  EXPECT(ztimer::state().dt == 0.05f);
  clock.now = 200;
  ztimer::update();
  EXPECT(ztimer::state().dt > 0.0999f && ztimer::state().dt < 0.1001f);
}

TEST(ztimer_max_rate_round_trips) {
  reset();
  EXPECT(ztimer::set_max_rate(0.0f) == 8.0f);
  EXPECT(ztimer::set_max_rate(20.0f) == 20.0f);
  EXPECT(ztimer::state().max_dt == 0.05f);
}

// The bug the rebuild fixes: with the original's ticks-since-boot, ten days of
// uptime leaves float seconds unable to resolve a 16 ms frame.
TEST(ztimer_float_seconds_lose_frames_at_ten_days_of_uptime) {
  const uint32_t ten_days_ms = 10u * 24u * 3600u * 1000u;
  const float a = static_cast<float>(ten_days_ms) * 0.001f;
  const float b = static_cast<float>(ten_days_ms + 16u) * 0.001f;
  EXPECT(b - a == 0.0f);
}
