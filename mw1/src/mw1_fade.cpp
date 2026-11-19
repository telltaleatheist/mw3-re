#include "mw1_fade.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace mw1::fade {

namespace {
constexpr float kHalf = kFadeSeconds / 2.0f;
}

bool Fade::start(std::function<void()> change) {
  if (busy()) return false;
  phase_ = Phase::kOut, t_ = 0.0f, change_ = std::move(change);
  return true;
}

void Fade::step(float dt) {
  if (!busy()) return;
  t_ += std::max(dt, 0.0f);
  if (phase_ == Phase::kOut && t_ >= kHalf) {
    // Black: the change under it, then in from black.
    std::function<void()> c = std::move(change_);
    change_ = nullptr;
    phase_ = Phase::kIn, t_ = 0.0f;
    if (c) c();
    return;
  }
  if (phase_ == Phase::kIn && t_ >= kHalf) phase_ = Phase::kNone, t_ = 0.0f;
}

float Fade::alpha() const {
  switch (phase_) {
    case Phase::kOut: return std::clamp(t_ / kHalf, 0.0f, 1.0f);
    case Phase::kIn: return std::clamp(1.0f - t_ / kHalf, 0.0f, 1.0f);
    default: return 0.0f;
  }
}

void Fade::cut(std::function<void()> change, bool in) {
  reset();
  if (change) change();
  if (in) fade_in();
}

void Fade::fade_in() { phase_ = Phase::kIn, t_ = 0.0f, change_ = nullptr; }

Fade& current() {
  static Fade f;
  return f;
}

namespace {
bool& enabled_flag() {
  static bool on = [] {
    const char* v = std::getenv("MW1_FADE");
    return !(v && std::strcmp(v, "0") == 0);
  }();
  return on;
}
}  // namespace

bool enabled() { return enabled_flag(); }

void set_enabled(bool on) {
  enabled_flag() = on;
  current().reset();
}

}  // namespace mw1::fade
