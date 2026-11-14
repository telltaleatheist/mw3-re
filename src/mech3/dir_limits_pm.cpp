#include "mech3/dir_limits_pm.h"

#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace mech3::pm_dir {

void (*report)(int level, int line, const char* text) = nullptr;

namespace {
constexpr float kHalfPi = 1.5707964f;    // 0x005ea7b0
constexpr float kTwoPi = 6.2831855f;      // 0x40c90fdb
constexpr double kFlat = 0.9990000128746033;  // [0x005ea7b8]: past it a direction is straight up or down

// Pirate's Moon 1.0 @ 0x004a6d40: a yaw band as its middle's direction and the cosine of its half width.
void band(float* o, float lo, float hi) {
  if (lo > hi) {
    o[3] = FLT_MAX;
    return;
  }
  o[1] = 0.0f;
  const double mid = (static_cast<double>(lo) + hi) * 0.5;
  const float half = (hi - lo) * 0.5f;
  o[0] = static_cast<float>(-std::sin(mid));
  o[2] = static_cast<float>(-std::cos(mid));
  o[3] = half < 3.14159265359 ? static_cast<float>(std::cos(static_cast<double>(half))) : -1.0f;  // [0x005e6e20]
}

// Pirate's Moon 1.0 @ 0x004a6e30
void pitch(float* o, float lo, float hi) {
  if (!(lo < hi)) {
    o[0] = o[1] = FLT_MAX;
    o[2] = o[3] = 0.0f;
    return;
  }
  const double a = kHalfPi;
  o[0] = static_cast<float>(std::cos(a - lo));
  o[1] = static_cast<float>(std::cos(a - hi));
  o[2] = static_cast<float>(std::sin(a - lo));
  o[3] = static_cast<float>(std::sin(a - hi));
}

// Pirate's Moon 1.0 @ 0x004a6dd0: how far inside a band a flat unit direction is (out), and whether it is.
bool in_band(const float* b, const float* flat, float* out) {
  const float s = static_cast<float>(double{b[0]} * flat[0] + double{b[2]} * flat[2]);
  const double d = double{s} - b[3];
  *out = static_cast<float>(d);
  return !(d < 0.0) && !std::isnan(d);
}

// The classifier's record (0x004a7000's second argument, 0x1c bytes).
struct Seen {
  bool in0 = false, in1 = false;  // +0x00, +0x01: inside the band from 0 up, from 0 down
  bool pitch_ok = false;          // +0x02
  bool flat_ok = false;           // +0x03: not straight up or down
  float len = 0;                  // +0x04: the flat part's length
  float flat[3] = {};             // +0x08: the flat part, unit
  float d1 = 0, d0 = 0;           // +0x14, +0x18: how far inside each band
};

// Pirate's Moon 1.0 @ 0x004013d0: a vector made unit, its length (as a float) answered; none when 0.
float unit(float* v) {
  const double l = std::sqrt((double{v[0]} * v[0] + double{v[2]} * v[2]) + double{v[1]} * v[1]);
  const float lf = static_cast<float>(l);
  uint32_t bits;
  std::memcpy(&bits, &lf, 4);
  if (bits & 0x7fffffffu) {
    const double inv = 1.0 / l;
    v[0] = static_cast<float>(inv * v[0]), v[1] = static_cast<float>(inv * v[1]), v[2] = static_cast<float>(inv * v[2]);
  }
  return lf;
}

// Pirate's Moon 1.0 @ 0x004a7000 with 0x004a6e90: the pitch inside its band (unordered fails), then the bands.
bool classify(const float* c, const float d[3], Seen& s) {
  s.pitch_ok = c[14] <= d[1] && d[1] <= c[15];
  s.flat_ok = !(std::fabs(double{d[1]}) >= kFlat);  // unordered counts as flat (C0)
  if (s.flat_ok) {
    s.flat[0] = d[0], s.flat[1] = 0.0f, s.flat[2] = d[2];
    s.len = unit(s.flat);
    s.in0 = in_band(c, s.flat, &s.d0);
    s.in1 = in_band(c + 4, s.flat, &s.d1);
  } else {
    s.in0 = s.in1 = true;
    s.len = 0.0f;
  }
  return s.pitch_ok && (s.in0 || s.in1);
}

// Pirate's Moon 1.0 @ 0x004a7170: the pitch put at the bottom or the top it passed (unordered counts as below
// the bottom), the flat part scaled by that edge's cosine; inside, kept and scaled by `len`.
void pitch_clamp(const float* p, float* o, float y, float len) {
  float k;
  if (!(y >= p[0])) {
    o[1] = p[0], k = p[2];
  } else if (y > p[1]) {
    o[1] = p[1], k = p[3];
  } else {
    o[1] = y, k = len;
  }
  o[0] = o[0] * k;
  o[2] = o[2] * k;
}
}  // namespace

// Pirate's Moon 1.0 @ 0x004a6ec0 (with 0x004a6d30 twice and 0x004a6e20).
void unset(float* c) {
  for (int i = 0; i < kSize; ++i) c[i] = 0.0f;
  c[3] = c[7] = FLT_MAX;
  c[14] = FLT_MAX;
}

// Pirate's Moon 1.0 @ 0x004a6ee0: a yaw range that crosses 0 is split there, one band either side; a pitch
// outside -pi/2..pi/2 is reported and taken back (a top past -pi/2 to pi/2: the exe compares the top with -pi/2,
// [0x005ea7b4], where it means pi/2, so any top becomes pi/2 there).
void set(float* c, float pitch_lo, float pitch_hi, float yaw_lo, float yaw_hi) {
  if (yaw_hi < 0.0f) {
    band(c + 4, yaw_lo, yaw_hi);
    band(c, 0.0f, yaw_hi);
  } else if (yaw_lo > 0.0f) {
    band(c + 4, yaw_lo, 0.0f);
    band(c, yaw_lo, yaw_hi);
  } else {
    band(c + 4, yaw_lo, 0.0f);
    band(c, 0.0f, yaw_hi);
  }
  if (!(-kHalfPi <= pitch_lo && pitch_hi <= kHalfPi)) {
    if (report) report(0x100, 0x72, "Requires version of CZValidateDir that handles angles outside the range");
    if (pitch_lo < -kHalfPi) pitch_lo = -kHalfPi;
    if (pitch_hi > -kHalfPi) pitch_hi = kHalfPi;
  }
  pitch(c + 14, pitch_lo, pitch_hi);
}

// Pirate's Moon 1.0 @ 0x004a7390: each angle held to its range (the yaw a turn either way), then the yaw turned
// round: its range negated and swapped.
void set_radians(float* c, float pitch_lo, float pitch_hi, float yaw_lo, float yaw_hi) {
  if (yaw_hi > kTwoPi) yaw_hi = kTwoPi;       // [0x005e14d0]
  if (yaw_lo < -kTwoPi) yaw_lo = -kTwoPi;     // [0x005ea790]
  if (pitch_hi > kHalfPi) pitch_hi = kHalfPi;
  if (pitch_lo < -kHalfPi) pitch_lo = -kHalfPi;
  set(c, pitch_lo, pitch_hi, -yaw_hi, -yaw_lo);
}

// Pirate's Moon 1.0 @ 0x004a7420
void set_turn(float* c, float pitch_v, float yaw) {
  if (yaw > kTwoPi) yaw = kTwoPi;
  if (pitch_v > kHalfPi) pitch_v = kHalfPi;
  set(c, -pitch_v, pitch_v, -yaw, yaw);
}

// Pirate's Moon 1.0 @ 0x004a70c0
bool within(const float* c, const float d[3]) {
  Seen s;
  return classify(c, d, s);
}

// Pirate's Moon 1.0 @ 0x004a71f0: within, the direction itself (true); else (false) straight ahead at the
// pitch it passed when it is all but vertical, the nearer band's edge outside both bands, else its own heading,
// each put at the pitch (0x004a7170).
bool clamp(const float* c, const float d[3], float out[3]) {
  Seen s;
  if (classify(c, d, s)) {
    out[0] = d[0], out[1] = d[1], out[2] = d[2];
    return true;
  }
  if (!s.flat_ok) {
    out[0] = 0.0f, out[1] = 0.0f, out[2] = -1.0f;
  } else if (!s.in0 && !s.in1) {
    const float* e = s.d0 <= s.d1 ? c + 11 : c + 8;  // +0x2c, else +0x20
    out[0] = e[0], out[1] = e[1], out[2] = e[2];
  } else {
    out[0] = s.flat[0], out[1] = s.flat[1], out[2] = s.flat[2];
  }
  pitch_clamp(c + 14, out, d[1], s.len);
  return false;
}

}  // namespace mech3::pm_dir
