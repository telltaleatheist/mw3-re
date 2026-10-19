#pragma once

// The rebuild's switches for fixes that change play against the original
// (docs/fixes.md). Not original code. The probes turn them off, so the
// differential tests check the original path.
namespace zfix {

// The frame the original's per-frame steps were tuned at (Owen, 2026-10-20):
// a step of x a frame becomes x * 30 a second.
constexpr float kReferenceFps = 30.0f;

struct Options {
  bool per_second = true;  // FIX (B): per-frame steps scaled by dt (fixes.md, Frame rate)
  bool deep_water = true;  // FIX (bug): the gait's wading slowdown as a continuous curve (movement.md)
};

inline Options& options() {
  static Options o;
  return o;
}

// A per-frame step's share of this frame: 1 in the exe, dt * 30 with the fix
// (exactly 1 at dt = 1/30).
inline float frame_steps(float dt) { return options().per_second ? dt * kReferenceFps : 1.0f; }

}  // namespace zfix
