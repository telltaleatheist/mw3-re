#pragma once
#include <cmath>

// The AI's sums as the reference runtime runs them (docs/pm-tests.md, MW3's AI
// at 53 bits; pm-map.md, PM's AI at 53 bits): the x87 at 53 bits keeps a value
// in the register (a double here) until the exe stores it to a float. A port
// in float rounds every step, which flips a compare on a last bit now and then.
namespace mech3::ai {

// Two products summed in the register, stored once (a dot or a cross).
inline float wide_sum(float a, float b, float c, float d) { return static_cast<float>(double{a} * b + double{c} * d); }

// Mech3.exe 1.2 @ 0x004ce530 (PM's 0x004dc0f0 and 0x004dbf10, 0x004013d0 with the
// root not stored): (x, z) to unit length, the length returned as stored. The
// squares, their sum and the root stay in the register, the root is stored, and
// 1 over the root (stored, or not) stays unrounded for each word's product.
// Of no length: each word times 0.
inline float unit_flat(float* x, float* z, bool root_stored) {
  const double root = std::sqrt(double{*x} * *x + double{*z} * *z);
  const float len = static_cast<float>(root);
  const double k = len != 0.0f ? 1.0 / (root_stored ? double{len} : root) : 0.0;
  *x = static_cast<float>(*x * k), *z = static_cast<float>(*z * k);
  return len;
}

// Mech3.exe 1.2 @ 0x004020d0 (zmth's normalize as the exe runs it, for the AI;
// the shared zmth::normalize stays as it is for its other callers): the squares
// summed (x and z, then y) and the root in the register, the root stored, 1
// over the unrounded root for each word, stored. Of no length: left as it is.
inline float normalize_x87(float v[3]) {
  const double root = std::sqrt((double{v[0]} * v[0] + double{v[2]} * v[2]) + double{v[1]} * v[1]);
  const float len = static_cast<float>(root);
  if (len == 0.0f) return len;  // the stored root's bits less the sign
  const double k = 1.0 / root;
  v[0] = static_cast<float>(v[0] * k), v[1] = static_cast<float>(v[1] * k), v[2] = static_cast<float>(v[2] * k);
  return len;
}

// One sum or difference in the register, then stored.
inline float stored(double v) { return static_cast<float>(v); }

}  // namespace mech3::ai
