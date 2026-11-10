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
  // The fixes that are always on (docs/upgrades-sort.md, (a)): the game never
  // turns this off, not even --original; only the probes and tests do, to run
  // the exe's own path against the original.
  bool fixed = true;
  // The Upgrades page's switches (Owen, 2026-11-10; upgrades-sort.md (b), (c)):
  // gameplay changes a player can turn off to play the game as it shipped.
  bool water = true;        // deep water slows a mech smoothly
  bool mfb_regroup = true;  // the field bases come back after a repair
  bool improved_ai = true;  // the lancemates' pathing
  bool artemis = true;      // Artemis FCS tracking
  bool small_fixes = true;  // the list in upgrades/upgrades_list.cpp
  // Not a fix: the 1.5 * 2^52 rounding trick gives what a 53-bit FPU gives, as
  // on Windows 11 in play (Owen, 2026-10-20: mechs cast shadows); false gives
  // the 24-bit result Direct3D 6's FPU setting would (CLAUDE.md, FPU precision).
  bool fpu53 = true;
  bool any_resolution = true;  // FIX (B): any screen size and 16:9 (resolution.md)
  bool nested_lock = true;     // FIX (A): a 3D lock inside a lock leaves the outer one (render-frame.md)
  // Ours: the menus on the mission's canvas when it is wider than 640x480
  // (16:9), on by default (Owen, 2026-10-31); a display choice, not a bug
  // fix: mw3.cfg's widemenus=0, --no-wide-menus (resolution.md section 12).
  bool wide_menus = true;
  // Ours (W56, Owen 2026-11-03), with `fixed`: a stick's throttle lever covers
  // reverse too (mech3::lever_throttle); mw3.cfg's throttle_reverse=0 keeps
  // the original's stop at the bottom.
  bool throttle_reverse = true;
  // Ours (W58, with improved_ai: docs/lancemate-pathing.md): AI pilots walk round
  // walls by a walkability grid when their straight way is blocked; the
  // player's side on, the enemies off until Owen decides (the game harder).
  bool nav_allies = true;
  bool nav_enemies = false;
};

inline Options& options() {
  static Options o;
  return o;
}

// The five switches together (--original, bugfixes= of old, the lobby's word).
inline void set_upgrades(bool on) {
  Options& o = options();
  o.water = o.mfb_regroup = o.improved_ai = o.artemis = o.small_fixes = on;
}
// Every fix, the always-on ones too: the probes' and tests' original path.
inline void set_all(bool on) {
  options().fixed = on;
  set_upgrades(on);
}
inline bool all() {
  const Options& o = options();
  return o.fixed && o.water && o.mfb_regroup && o.improved_ai && o.artemis && o.small_fixes;
}

// A per-frame step's share of this frame: 1 in the exe, dt * 30 with the fix
// (exactly 1 at dt = 1/30).
inline float frame_steps(float dt) { return options().per_second ? dt * kReferenceFps : 1.0f; }

}  // namespace zfix
