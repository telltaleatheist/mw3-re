// The fade between screens (FIXES.md item 52, mw1_fade.h): out over a quarter second, the change on the first black
// frame and only once, in over a quarter second; a change asked for while one runs dropped; the same at 60 and 30
// frames a second.
#include <cmath>
#include <cstdio>
#include <vector>

#include "check.h"
#include "mw1_fade.h"

namespace F = mw1::fade;

TEST(mw1_fade_out_change_in) {
  for (const float fps : {60.0f, 30.0f}) {
    F::Fade f;
    int32_t changes = 0, changed_at = -1, frames = 0;
    float alpha_at_change = -1.0f, last = 0.0f;
    bool rising = true, falling = true;
    F::Fade::Phase was = F::Fade::Phase::kOut;
    EXPECT(f.start([&] { ++changes, changed_at = frames, alpha_at_change = 1.0f; }) && f.busy());
    EXPECT(!f.start([&] { changes += 100; }));  // a double click: dropped
    for (; f.busy() && frames < 1000; ++frames) {
      const float a = f.alpha();
      if (f.phase() == F::Fade::Phase::kOut) rising = rising && a >= last;
      else if (was == F::Fade::Phase::kIn) falling = falling && a <= last;
      else falling = falling && a == 1.0f;  // the first frame after the change: black
      was = f.phase();
      last = a;
      f.step(1.0f / fps);
    }
    const float span = frames / fps;
    std::printf("  %2.0f fps: changed at %.3f s, done at %.3f s\n", fps, changed_at / fps, span);
    EXPECT(changes == 1 && alpha_at_change == 1.0f && rising && falling);
    EXPECT(std::fabs((changed_at + 1) / fps - F::kFadeSeconds / 2) <= 1.0f / fps + 1e-4f);
    EXPECT(std::fabs(span - F::kFadeSeconds) <= 2.0f / fps + 1e-4f);
    EXPECT(f.alpha() == 0.0f && f.start([] {}));  // done: the next one starts
  }
}

TEST(mw1_fade_the_black_frame_and_the_ramp) {
  F::Fade f;
  bool done = false;
  f.start([&] { done = true; });
  EXPECT(f.alpha() == 0.0f);
  f.step(F::kFadeSeconds / 4);
  EXPECT(std::fabs(f.alpha() - 0.5f) < 1e-4f && !done);
  f.step(F::kFadeSeconds / 4);
  EXPECT(done && f.phase() == F::Fade::Phase::kIn && f.alpha() == 1.0f);  // the change's frame: black
  f.step(F::kFadeSeconds / 4);
  EXPECT(std::fabs(f.alpha() - 0.5f) < 1e-4f && f.busy());
  f.step(F::kFadeSeconds / 4);
  EXPECT(!f.busy() && f.alpha() == 0.0f);
  // Only in (from black), and a cut.
  f.fade_in();
  EXPECT(f.busy() && f.alpha() == 1.0f);
  int32_t n = 0;
  f.cut([&] { ++n; }, false);
  EXPECT(n == 1 && !f.busy());
}
