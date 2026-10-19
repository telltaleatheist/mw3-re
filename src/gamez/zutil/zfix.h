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
};

inline Options& options() {
  static Options o;
  return o;
}

}  // namespace zfix
