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
              float* throttle, float inv_slow, float slack_along, float slack_side, const float* fwd_in) {
  float pos[3], to[3], fx, fz;
  h.position(v, pos);
  if (fwd_in) fx = fwd_in[0], fz = fwd_in[2];
  else h.forward(v, &fx, &fz);
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

namespace {
// 0x004d52e1..0x004d5415: 0x004d7060's turn in line, its NaNs as the compares fall.
float manoeuvre_turn(const float fw[3], const float dir[3], float keep) {
  const TurnPmParams& p = turn_walk_params();
  const float d = static_cast<float>(double{fw[2]} * dir[2] + double{fw[0]} * dir[0]);
  if (!(p.behind <= d)) return keep == 0.0f || std::isnan(keep) ? 1.0f : keep;
  if (p.cos_dead <= d) return 0.0f;
  const float c = static_cast<float>(double{dir[0]} * fw[2] - double{dir[2]} * fw[0]);
  if (!(d >= 0.0f)) return !(c >= 0.0f) ? -1.0f : 1.0f;
  if (-p.sin_dead > c) {
    const double k = (double{p.sin_dead} + c) * p.gain;
    if (!(k >= -1.0)) return -1.0f;
    const float nl = -p.least;
    return k > nl ? nl : static_cast<float>(k);
  }
  if (!(c > p.sin_dead)) return 0.0f;
  const double k = (double{c} - p.sin_dead) * p.gain;
  if (k > 1.0) return 1.0f;
  return k < p.least ? p.least : static_cast<float>(k);
}
}  // namespace

// Pirate's Moon 1.0 @ 0x004d5040 (MW3's 0x004ca600). Unlike MW3's, over the ceiling it still gives the jets 0.01.
void ManoeuvrePm::run(WalkPmHooks& h, zgame::Handle v) {
  float pos[3];
  h.position(v, pos);
  float steer = 0.0f;
  if (!no_jets) {
    const double fuel = h.jump_fuel(v);
    if (!(fuel >= 0.1)) jump = 0.0f;
    else if (0.6000000238418579 < fuel) jump = 1.0f;
  }
  if (until <= h.now()) {
    h.order_pop();
    return;
  }
  const double s = h.airborne(v) ? -double{h.forward_speed(v)} : h.move_speed(v);
  float thr = 0.0f;
  constexpr double kShare = 0.20000000298023224;  // 0x005ee180
  if (!(-1.0 <= s)) {
    double k = (s + 1.0) * kShare;
    if (!(k >= -1.0)) k = -1.0;
    thr = static_cast<float>(-k);
  } else if (s > 1.0) {
    double k = (s - 1.0) * kShare;
    if (k > 1.0) k = 1.0;
    thr = static_cast<float>(-k);
  }
  bool ready = true;
  if (phase == 0) {
    if (double{thr} < -0.0001 || 0.0001 < double{thr}) ready = false;
    else phase = 1;
  }
  if (ready && phase == 1) {
    if (!target) target = h.contact_pick(v, 1);
    if (!target && h.hit_lately(v)) target = h.attacker(v);
    if (!target) target = h.contact_pick(v, 3);
    if (target) {
      h.set_fought(true);
      steer = turn;
      phase = 2;
    }
  }
  if (ready && phase == 2) {
    float at[3], dir[3], fw[3] = {};
    const float* to = at;
    if (!h.lead_point(v, target, at)) to = h.entity_place(target);
    flat_dir_pm(pos, to, dir);
    h.forward(v, &fw[0], &fw[2]);
    turn = manoeuvre_turn(fw, dir, turn);
    steer = turn;
    if ((h.params_ec() & 0x40) && !h.fought()) until = 0.0f;
  }
  h.set_jump(h.ceiling() <= pos[1] ? 0.01f : jump);  // 0x3c23d70a
  if (h.airborne(v)) h.set_jet_forward(thr);
  else h.set_throttle(thr);
  h.set_turn(steer);
}

// Pirate's Moon 1.0 @ 0x004d4fa0
void ManoeuvrePm::leave(WalkPmHooks& h) {
  h.stop_controls();
  h.manoeuvre_over();
}

// Pirate's Moon 1.0 @ 0x004dcbe0: the way from where it last was to the node, at the speed the brain's throttle gives.
void MoveStatePm::start(WalkPmHooks& h, SteerPm& s, zgame::Handle v, const float* to) {
  step = 0, turn = 0.0f, throttle = 0.0f;
  wake = 3.4028235e38f;
  const float speed = static_cast<float>(h.speed_at(v, h.max_throttle()));
  s.set(h, h.way_from(), to, speed, v);
  const double stop = h.stop_distance(v, speed);
  dist = static_cast<float>(stop);
  slow = static_cast<float>(1.0 / stop);  // from the register
}

// Pirate's Moon 1.0 @ 0x004dcfe0. At the way's end the next behaviour takes over; with none it stands for 5 s and
// tries again.
void MoveStatePm::run(WalkPmHooks& h, SteerPm& s, zgame::Handle v, const float* place) {
  if (step != 0 && step != 1 && step != 2) return;
  float away[2], sum = 0.0f;
  bool waiting = step == 2;
  if (!waiting) {
    away_pm(s, away, &sum);
    if (!(s.at_end() && !(sum > dist))) {  // fcomp, test ah 0x41: unordered counts as there
      drive_pm(h, s, v, away, sum, dist * 0.5f, &turn, &throttle, slow, 0.04f, 0.04f);
      h.set_turn(turn);
      h.set_throttle(throttle);
      step = 1;
      return;
    }
  }
  for (;;) {
    if (!waiting) {
      const float dir[3] = {-s.dir[0], 0.0f, -s.dir[1]};
      if (const zgame::Handle next = h.next_along(dir)) {
        float* from = h.way_from();
        from[0] = place[0], from[1] = place[1], from[2] = place[2];
        h.set_behaviour(next);
        return;
      }
      turn = 0.0f;
      throttle = 0.0f;
      h.set_turn(0.0f);
      h.set_throttle(throttle);
      wake = h.now() + 5.0f;  // [0x005df000]
    }
    waiting = false;
    if (!(wake <= h.now())) break;
  }
  step = 2;
}

// Pirate's Moon 1.0 @ 0x004ddab0. FIX (bug): the exe's aim in steps 0 and 2 is a stack vector it never sets there
// (MW3's 0x004cfad0 the same); the port aims back along the way, as its end does (docs/fixes.md).
void MoveStatePm::run_backup(WalkPmHooks& h, SteerPm& s, zgame::Handle v, const float* place) {
  float fx, fz;
  h.forward(v, &fx, &fz);
  const float fwd[3] = {fx, 0.0f, fz};
  const float aim[3] = {-s.dir[0], 0.0f, -s.dir[1]};
  const float* walk_fwd = fwd;
  const float back[3] = {-fx, -0.0f, -fz};
  float dot, cross;
  bool waiting = false;
  switch (step) {
    case 0:
      turn = turn_pm(back, aim, &dot, &cross, h.turn_goal(), turn_walk_params());
      if (!(dot >= 0.9f)) {  // [0x005dfcd4]
        h.set_turn(turn);
        step = 0;
        return;
      }
      walk_fwd = back;  // the drive turns the context's forward round again: this frame walks with the front
      [[fallthrough]];
    case 1: {
      float away[2], sum;
      away_pm(s, away, &sum);
      if (!s.at_end() || dist < sum) {
        const float round[3] = {-walk_fwd[0], -walk_fwd[1], -walk_fwd[2]};
        drive_pm(h, s, v, away, sum, dist * 0.5f, &turn, &throttle, slow, 0.04f, 0.04f, round);
        h.set_turn(turn);
        h.set_throttle(-throttle);
        step = 1;
        return;
      }
      throttle = 0.0f;
      h.set_throttle(0.0f);
      h.set_turn(turn);
      step = 2;
      return;
    }
    case 2:
      turn = turn_pm(fwd, aim, &dot, &cross, h.turn_goal(), turn_walk_params());
      if (!(dot >= 0.9f)) {
        h.set_turn(turn);
        step = 2;
        return;
      }
      break;
    case 3: waiting = true; break;
    default: return;
  }
  for (;;) {
    if (!waiting) {
      const float dir[3] = {-s.dir[0], 0.0f, -s.dir[1]};
      if (const zgame::Handle next = h.next_along(dir)) {
        float* from = h.way_from();
        from[0] = place[0], from[1] = place[1], from[2] = place[2];
        h.set_behaviour(next);
        return;
      }
      turn = 0.0f;
      throttle = 0.0f;
      h.set_turn(0.0f);
      h.set_throttle(throttle);
      wake = h.now() + 5.0f;  // [0x005df000]
    }
    waiting = false;
    if (!(wake <= h.now())) break;
  }
  step = 3;
}

// Pirate's Moon 1.0 @ 0x004e1fd0. Steps 0 and 1 only: at the end it stays as it is, the item it queued ending it.
void MoveStatePm::run_terminus(WalkPmHooks& h, SteerPm& s, zgame::Handle v, const float* place) {
  if (step != 0 && step != 1) return;
  float away[2], sum;
  away_pm(s, away, &sum);
  if (s.at_end() && !(sum > dist)) {
    h.set_again();
    h.order_ready();
    float* from = h.way_from();
    from[0] = place[0], from[1] = place[1], from[2] = place[2];
    h.return_back();
    return;
  }
  drive_pm(h, s, v, away, sum, dist * 0.5f, &turn, &throttle, slow, 0.04f, 0.04f);
  h.set_turn(turn);
  h.set_throttle(throttle);
  step = 1;
}

namespace {
// [0x0062fbb8], made by 0x004de0e0's first run: a 1 degree dead zone ([0x005eede8] degrees), the gain 1.2 and the
// least turn 0.1.
const TurnPmParams& deploy_turn_params() {
  static const TurnPmParams p = [] {
    const double a = double{1.0f} * 0.01745329251994;  // [0x005deee8]
    return TurnPmParams{static_cast<float>(std::cos(a)), static_cast<float>(std::sin(a)), 1.2f, 0.1f, 0.0f};
  }();
  return p;
}
// 0x00537ae0: the flat distance between two points.
float flat_distance_pm(const float a[3], const float b[3]) {
  const float dx = a[0] - b[0], dz = a[2] - b[2];
  return std::sqrt(dz * dz + dx * dx);
}
// The yaw facing from `from` toward `to`, the vehicle's place angle: fpatan(-dx, -dz).
float yaw_toward_pm(const float from[3], const float to[3]) {
  const float vx = to[0] - from[0], vz = to[2] - from[2];
  return static_cast<float>(std::atan2(-static_cast<double>(vx), -static_cast<double>(vz)));
}
}  // namespace

// Pirate's Moon 1.0 @ 0x004de0e0. FIX (bug): a node without a valve named has none, which the exe reads (a crash);
// it reads 0 here, as MW3's 0x004d00a0's port (docs/fixes.md).
void DeployStatePm::run(WalkPmHooks& h, SteerPm& s, zgame::Handle v, const DeployNodePm& n, float dt) {
  const TurnPmParams& fine = deploy_turn_params();
  auto value = [&](zgame::Handle valve) { return valve ? h.valve_value(valve) : 0; };
  float pos[3], fx, fz;
  h.position(v, pos);
  h.forward(v, &fx, &fz);
  const float fwd[3] = {fx, 0.0f, fz};
  float away[2], sum, dot, cross, k = 0.5f;
  bool on = false, open = false, waiting = false;
  switch (step) {
    case 0:
    case 1:
      away_pm(s, away, &sum);
      if (!s.at_end() || dist < sum) {
        drive_pm(h, s, v, away, sum, dist * 0.5f, &turn, &throttle, slow, 0.04f, 0.04f);
        h.set_turn(turn);
        h.set_throttle(throttle);
        step = 1;
        return;
      }
      h.set_hold(0);
      dist = 1.0f;
      [[fallthrough]];
    case 2:
      away_pm(s, away, &sum);
      if (dist < sum) {
        drive_pm(h, s, v, away, sum, dist * 0.5f, &turn, &throttle, slow, 0.04f, 0.04f);
        h.set_turn(turn);
        h.set_throttle(throttle);
        step = 2;
        return;
      }
      throttle = 0.0f, turn = 0.0f;
      [[fallthrough]];
    case 3:
      if (std::fabs(h.vertical_speed(v)) > 0.1f) {  // [0x0060bbdc]
        h.set_throttle(throttle);
        h.set_turn(turn);
        step = 3;
        return;
      }
      s.set(h, n.place, n.target, static_cast<float>(h.speed_at(v, h.max_throttle())), v);
      face[0] = s.dir[0], face[1] = 0.0f, face[2] = s.dir[1];
      turn = turn_pm(fwd, face, &dot, &cross, h.turn_goal(), fine);
      goto turned;
    case 4:
      turn = turn_pm(fwd, face, &dot, &cross, h.turn_goal(), turn_walk_params());
    turned:
      if (turn != 0.0f && !std::isnan(turn)) {  // test ah 0x40: unordered counts as turned
        h.set_throttle(throttle);
        h.set_turn(turn);
        step = 4;
        return;
      }
      dist = 0.5f;
      goto slide;
    case 0x29:
      k = dist;
    slide: {
      const float len = dt * k;
      const float d = flat_distance_pm(n.place, pos);
      if (len < d) {
        const float f = len / d;
        float p[3];  // each kept in the register, stored once
        for (int i = 0; i < 3; ++i) p[i] = static_cast<float>((double{n.place[i]} - pos[i]) * f + pos[i]);
        h.place_vehicle(v, p, yaw_toward_pm(n.target, p));
        step = 0x29;
        return;
      }
      h.place_vehicle(v, n.place, yaw_toward_pm(n.target, n.place));
    }
      [[fallthrough]];
    case 0x2a:
      on = h.deploy_on(), open = value(n.valve_b) != n.b;
      if (on && open) {
        if (value(n.valve_a) == n.a) {
          turn = turn_pm(fwd, face, &dot, &cross, h.turn_goal(), fine);
          h.set_turn(turn);
          step = 0x2a;
          return;
        }
        if (n.valve_a) h.valve_set(n.valve_a, n.a);
        goto held;
      }
      break;
    case 5:
      on = h.deploy_on(), open = value(n.valve_b) != n.b;
      if (on && open) {
      held:
        turn = turn_pm(fwd, face, &dot, &cross, h.turn_goal(), fine);
        h.set_turn(turn);
        step = 5;
        return;
      }
      break;
    case 6: waiting = true; break;
    default: return;
  }
  if (!waiting) h.set_hold(1);
  for (;;) {
    if (!waiting) {
      const float dir[3] = {-s.dir[0], 0.0f, -s.dir[1]};
      if (const zgame::Handle next = h.next_along(dir)) {
        float* from = h.way_from();
        from[0] = n.place[0], from[1] = n.place[1], from[2] = n.place[2];
        h.set_behaviour(next);
        return;
      }
      turn = 0.0f;
      throttle = 0.0f;
      h.set_turn(0.0f);
      h.set_throttle(throttle);
      wake = h.now() + 5.0f;  // [0x005df000]
    }
    waiting = false;
    if (!(wake <= h.now()) && !h.finding_way()) break;  // the way finder's path ends the wait at once
  }
  step = 6;
}

}  // namespace mech3::ai
