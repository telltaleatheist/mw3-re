#include "mech3/zai_drive_pm.h"

#include <cmath>

namespace mech3::ai {

namespace {
constexpr float kBackCos = -0.70710677f;  // 0x005eeb30
constexpr float kAlongShare = 4.0f;       // 0x0060bb80
}  // namespace

// Pirate's Moon 1.0 @ 0x004d2d40, with [0x0060bb2c] degrees
const TurnPmParams& turn_walk_params() {
  constexpr double kRad = 0.017453292519943295;  // 0x005deee8
  constexpr double kA = 5.0;
  static const TurnPmParams p{static_cast<float>(std::cos(kA * kRad)), static_cast<float>(std::sin(kA * kRad)), 1.2f, 0.1f,
                              static_cast<float>(std::cos((180.0 - kA) * kRad))};
  return p;
}

// Pirate's Moon 1.0 @ 0x004d7060 (MW3's 0x004cdd40)
float turn_pm(const float fwd[3], const float to[3], float* dot, float* cross, float keep, const TurnPmParams& p) {
  const float d = wide_sum(fwd[2], to[2], fwd[0], to[0]);
  *dot = d;
  *cross = 0.0f;
  if (!(p.behind <= d)) return keep == 0.0f ? 1.0f : keep;
  if (p.cos_dead <= d) return 0.0f;
  const float c = wide_sum(fwd[2], to[0], -to[2], fwd[0]);
  *cross = c;
  if (d < 0.0f) return c < 0.0f ? -1.0f : 1.0f;
  if (-p.sin_dead <= c) {
    if (!(p.sin_dead < c)) return 0.0f;
    const double v = (double{c} - p.sin_dead) * p.gain;
    if (1.0 < v) return 1.0f;
    return v < p.least ? p.least : static_cast<float>(v);
  }
  const double v = (double{c} + p.sin_dead) * p.gain;
  if (v < -1.0) return -1.0f;
  return -p.least < v ? -p.least : static_cast<float>(v);
}

// Pirate's Moon 1.0 @ 0x004d71e0 (MW3's 0x004cdee0)
float turn_or_back_pm(const float fwd[3], const float to[3], float* dot, float* cross, float keep, bool* back,
                      const TurnPmParams& p) {
  const float d = wide_sum(fwd[0], to[0], fwd[2], to[2]);
  *back = d < kBackCos;
  if (!*back) return turn_pm(fwd, to, dot, cross, keep, p);
  const float rear[3] = {-fwd[0], 0.0f, -fwd[2]};
  return turn_pm(rear, to, dot, cross, keep, p);
}

// Pirate's Moon 1.0 @ 0x004dccf0
void away_pm(const SteerPm& s, float away[2], float* sum) {
  away[0] = std::fabs(s.along);
  away[1] = std::fabs(s.side);
  *sum = away[1] + away[0];
}

// Pirate's Moon 1.0 @ 0x004dcd20 (MW3's 0x004cee80). [0x0062fbb0] (no backing up) is never set.
void drive_pm(WalkPmHooks& h, SteerPm& s, zgame::Handle v, const float away[2], float dist, float near, float* turn,
              float* throttle, float inv_slow, float slack_along, float slack_side) {
  float pos[3], to[3], fx, fz;
  h.position(v, pos);
  h.forward(v, &fx, &fz);
  const float fwd[3] = {fx, 0.0f, fz};
  s.carrot(to);
  bool back = s.along < 0.0f;
  if (!s.near(h) && h.pull(v, pos, to, to)) {
    flat_dir_pm(pos, to, to);
    back = false;
  } else {
    bool along_way = !(dist >= near);
    if (!along_way) along_way = flat_dir_pm(pos, to, to) < 1.0f;
    if (along_way) {  // there: face along the way
      const bool behind = s.along < 0.0f;
      to[0] = behind ? s.dir[0] : -s.dir[0];
      to[1] = 0.0f;
      to[2] = behind ? s.dir[1] : -s.dir[1];
    }
  }
  const float keep = h.turn_goal();
  float dot, cross;
  *turn = back ? turn_or_back_pm(fwd, to, &dot, &cross, keep, &back, turn_walk_params())
               : turn_pm(fwd, to, &dot, &cross, keep, turn_walk_params());
  float a = away[0] - slack_along;
  double b = double{away[1]} - slack_side;
  if (!(dot > s.x40) || (!(a > 0.0f) && !(b > 0.0))) {
    *throttle = 0.0f;
    return;
  }
  if (a < 0.0f) a = 0.0f;
  if (b < 0.0) b = 0.0;
  const double t = (double{kAlongShare} * a + b) * inv_slow;
  *throttle = static_cast<float>(back ? -t : t);
  const float most = h.max_throttle();
  if (most < *throttle) *throttle = most;
  else if (-most > *throttle) *throttle = -most;
}

}  // namespace mech3::ai
