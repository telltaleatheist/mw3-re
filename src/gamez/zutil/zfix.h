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
  bool bugs = true;        // FIX (bug): the play-changing bug fixes decided with Owen (CHANGES.md)
  // Not a fix: the 1.5 * 2^52 rounding trick gives what a 53-bit FPU gives, as
  // on Windows 11 in play (Owen, 2026-10-20: mechs cast shadows); false gives
  // the 24-bit result Direct3D 6's FPU setting would (CLAUDE.md, FPU precision).
  bool fpu53 = true;
};

inline Options& options() {
  static Options o;
  return o;
}

}  // namespace zfix
