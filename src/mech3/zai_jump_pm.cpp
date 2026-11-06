#include "mech3/zai_jump_pm.h"

#include <cfloat>
#include <cmath>

namespace mech3::ai {

namespace {
const TurnPmParams& turn_params_pm() {  // 0x004daf00, 0x60bb74 degrees
  static const TurnPmParams p{std::cos(1.0f * 0.017453292519943295f), std::sin(1.0f * 0.017453292519943295f), 1.2f, 0.1f, 0.0f};
  return p;
}
// [0x0062fb28..0x0062fb30], set at 0x004daec0: none within 0.1, else half of it, against the error's sign.
float jet_forward(double error) {
  const double a = std::fabs(error);
  if (a <= 0.1f) return 0.0f;
  const float k = static_cast<float>(double{0.5f + 0.0f} * a);
  return 0.0 < error ? -k : k;
}
// 0x0041e280: its argument pushed as a float.
float thrust(double a, float climb) { return static_cast<float>(double{static_cast<float>(a)} / climb); }
float clamp01(float v) {
  if (v > 1.0f) return 1.0f;
  return v >= 0.0f ? v : 0.0f;
}
}  // namespace

// Pirate's Moon 1.0 @ 0x004dac40 with 0x004dad70 (MW3's 0x004cb120, 0x004cb250)
JumpPlanPm jump_plan_pm(const float from[3], const float to[3], float up, float down, float margin, float speed) {
  JumpPlanPm p;
  const float dy = to[1] - from[1];
  const float dx = to[0] - from[0], dz = to[2] - from[2];
  const double hyp = std::sqrt(double{dx} * dx + double{dz} * dz);  // the CRT's _hypot
  p.dist = static_cast<float>(hyp);
  p.rise = dy;
  if (0.0f <= dy) {
    p.fall = margin;
    p.climb = dy + margin;
  } else {
    p.rise = -dy;
    p.climb = margin;
    p.fall = -dy + margin;
  }
  p.t[2] = 0.0f;
  p.peak = static_cast<float>(double{from[1]} + p.climb);
  const double ratio = double{up} / down;  // these three stay in the register
  const double k = 2.0 / ((double{down} - up) * ratio);
  p.ratio = static_cast<float>(ratio);
  p.k = static_cast<float>(k);
  p.t[0] = static_cast<float>(std::sqrt(k * p.climb));
  p.t[1] = static_cast<float>(-(p.t[0] * ratio));
  p.t[4] = static_cast<float>(std::sqrt(k * p.fall));
  const double back = -(ratio * p.t[4]);
  p.t[3] = static_cast<float>(back);
  const double total = back + p.t[4] + p.t[1] + p.t[0];
  p.total = static_cast<float>(total);
  p.need = total <= 0.0 ? FLT_MAX : static_cast<float>(hyp / total);
  if (p.need < speed) {
    p.speed = p.need;
  } else if (speed <= 0.0f) {
    p.speed = speed;
  } else {
    p.t[2] = static_cast<float>(double{p.dist} / speed - p.total);
    p.speed = speed;
  }
  return p;
}

// Pirate's Moon 1.0 @ 0x004dade0
bool JumpPm::can(JumpPmHooks& h, zgame::Handle v) {
  if (!v) return false;
  const zgame::Handle m = h.mech(v);
  return m && h.has_jets(m);
}

// Pirate's Moon 1.0 @ 0x004daf40
JumpPm::JumpPm(JumpPmHooks& hooks, SteerPm& s, zgame::Handle v) : h(hooks), steer(s), vehicle(v) {
  const float top = static_cast<float>(h.speed_at(v, h.max_throttle()));
  steer.set(h, h.way_from(), h.way_point(), top, v);
  const float g = h.gravity();
  float from[3];
  h.position(v, from);
  const float up = static_cast<float>(double{h.jet_climb()} * 1.0f - g);
  const float down = static_cast<float>(double{h.jet_climb()} * 0.01f - g);  // 0x0060bb6c
  const JumpPlanPm p = jump_plan_pm(from, h.way_point(), up, down, h.margin(), top);
  peak = p.peak;
  for (int i = 0; i < 5; ++i) t[i] = p.t[i];
  speed = p.speed;
  far = static_cast<float>((double{t[3]} + t[4]) * top);
}

// Pirate's Moon 1.0 @ 0x004db0e0. Each phase's time starts as it comes; the last phases also end
// when the mech is down.
void JumpPm::run(float dt) {
  const zgame::Handle m = h.mech(vehicle);
  h.set_x588(m);  // [0x0060bb70], 1 in the exe's data, nothing writes it
  float pos[3], fwd[2], fw3[3] = {}, dir[3] = {}, dist = 0;
  auto way = [&] {
    h.position(vehicle, pos);
    dist = flat_dir_pm(pos, h.way_point(), dir);
  };
  auto steer_on = [&](int32_t now_phase) {
    way();
    h.forward(vehicle, &fwd[0], &fwd[1]);
    fw3[0] = fwd[0], fw3[2] = fwd[1];
    float dot, cross;
    h.set_turn(turn_pm(fw3, dir, &dot, &cross, h.turn_goal(), turn_params_pm()));
    phase = now_phase;
  };
  const float jump_low = 0.01f;  // 0x0060bb6c
  switch (phase) {
    case 0:
    case 1: {
      way();
      h.forward(vehicle, &fwd[0], &fwd[1]);
      fw3[0] = fwd[0], fw3[2] = fwd[1];
      float dot, cross;
      const float turn = turn_pm(fw3, dir, &dot, &cross, h.turn_goal(), turn_params_pm());
      if (turn != 0.0f) {
        h.set_throttle(0.0f);
        h.set_turn(turn);
        phase = 1;
        return;
      }
      h.set_throttle(h.max_throttle());
      t[0] = h.now() + t[0];
      [[fallthrough]];
    }
    case 2:
      if (h.now() < t[0]) {
        h.set_jump(1.0f);
        h.set_jet_forward(jet_forward(double{-h.along_speed(m)} - speed));
        steer_on(2);
        return;
      }
      t[1] = h.now() + t[1];
      [[fallthrough]];
    case 3:
      if (h.now() < t[1] && h.in_air(m)) {
        h.set_jump(jump_low);
        h.set_jet_forward(jet_forward(double{-h.along_speed(m)} - speed));
        steer_on(3);
        return;
      }
      way();
      t[2] = static_cast<float>(double{h.now()} + double{dist} / h.speed_at(vehicle, h.max_throttle()) + 5.0f);
      [[fallthrough]];
    case 4:
      if (h.now() < t[2] && h.in_air(m)) {
        way();
        if (far < dist) {
          const float above = pos[1] - peak;
          double a;
          if (above < -0.1f || 0.1f < above) {
            a = -above - double{h.climb_speed(m)} * dt * 5.0f;
            a = (a + a) / (double{dt} * dt * 25.0f);
          } else {
            a = -(double{h.climb_speed(m)} / dt);
          }
          h.set_jump(clamp01(thrust(h.gravity() + a, h.jet_climb())));
          h.set_jet_forward(jet_forward(double{-h.along_speed(m)} - speed));
          steer_on(4);
          return;
        }
      }
      t[3] = h.now() + t[3];
      [[fallthrough]];
    case 5:
      if (h.now() < t[3] && h.in_air(m)) {
        h.set_jump(jump_low);
        way();
        h.set_jet_forward(jet_forward(-h.along_speed(m) - dist / ((double{t[4]} + t[3]) - h.now())));
        steer_on(5);
        return;
      }
      t[4] = h.now() + t[4];
      [[fallthrough]];
    case 6:
      if (h.now() < t[4] && h.climb_speed(m) < 0.0f && h.in_air(m)) {
        h.set_jump(clamp01(thrust(-(double{h.climb_speed(m)} / dt) + h.gravity(), h.jet_climb())));
        way();
        h.set_jet_forward(jet_forward(-h.along_speed(m) - dist / (double{t[4]} - h.now())));
        steer_on(6);
        return;
      }
      h.set_throttle(0.0f);
      h.set_jump(0.0f);
      h.set_turn(0.0f);
      [[fallthrough]];
    case 7:
      if (h.in_air(m)) {
        phase = 7;
        return;
      }
      h.position(vehicle, h.way_from());
      h.go_next();
      return;
    default:
      return;
  }
}

}  // namespace mech3::ai
