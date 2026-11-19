#pragma once
// The fade between MW1's screens (FIXES.md item 52, Owen 2026-11-18: "fade to black and fade back in to the new
// screen ... maybe half a second"): out to black over half of kFadeSeconds, the change made on the first black frame,
// then in over the other half. Input is ignored for the whole of it (a double click can't skip a screen). Pure: the
// clock is the caller's (the engine's frame time); the screens draw the tint (mw1_screen.h's Mw1State, a tint quad
// over the frame, item 12's path).
#include <functional>

namespace mw1::fade {

inline constexpr float kFadeSeconds = 0.5f;  // out and in, half each: the one number to tune

class Fade {
 public:
  enum class Phase { kNone, kOut, kIn };
  // A change of screen: out, `change` on the first black frame, in. False (and `change` dropped) while one runs.
  bool start(std::function<void()> change);
  // The clock: `dt` seconds more. Runs the change when the screen has gone black.
  void step(float dt);
  // The black's alpha over the frame (0 clear, 1 black).
  float alpha() const;
  bool busy() const { return phase_ != Phase::kNone; }
  Phase phase() const { return phase_; }
  // A change made at once (no fade: no screen up yet, or fades off): `change` now, the fade-in kept if `fade_in`.
  void cut(std::function<void()> change, bool fade_in);
  // Only the fade-in (a screen up from black: the results after a mission, the loading screen).
  void fade_in();
  void reset() { phase_ = Phase::kNone, t_ = 0.0f, change_ = nullptr; }

 private:
  Phase phase_ = Phase::kNone;
  float t_ = 0.0f;  // seconds into the phase
  std::function<void()> change_;
};

// The one fade MW1's screens share, and whether fades are on (MW1_FADE=0: off). The tests that press through
// screens expect each change at once: their runs set them off, the fade's own test on (set_enabled; reset() too).
Fade& current();
bool enabled();
void set_enabled(bool on);

}  // namespace mw1::fade
