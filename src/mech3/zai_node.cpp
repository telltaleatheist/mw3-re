#include "mech3/zai_node.h"

#include <cfloat>
#include <cmath>
#include <cstring>
#include <vector>

#include "crt/msvc_math.h"
#include "gamez/zmath/zmth_vector.h"
#include "mech3/zai_path.h"
#include "mech3/zai_steps.h"
#include "mech3/zai_valve.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3::ai {

namespace {
bool unordered_or_less(float a, float b) { return !(a >= b); }  // x87 C0 after fcomp a, b

// 0x004020d0 and 0x0051e210 (zmath) on a float[3].
float normalize3(float v[3]) {
  zmth::Vec3 w{v[0], v[1], v[2]};
  const float len = zmth::normalize(w);
  v[0] = w.x, v[1] = w.y, v[2] = w.z;
  return len;
}
void normalize_flat(float v[3]) {
  zmth::Vec3 w{v[0], v[1], v[2]};
  zmth::flat_normalize(w, w);
  v[0] = w.x, v[2] = w.z;
}

// What the steps' passable tests (vt+0xac) see of the pilot.
Pilot pilot_of(Brain& b) {
  Pilot p;
  brain_hooks().pilot_of(b.controls->vehicle(), &p);
  return p;
}

// The turn after its dot is known (both 0x004cdd40 and 0x004cdee0).
float turn_rest(float f0, float f2, const float to[3], float d, float* cross, float keep, const TurnParams& p) {
  *cross = 0;
  if (-p.cos_dead > d) return (keep == 0 || std::isnan(keep)) ? 1.0f : keep;
  if (d >= p.cos_dead) return 0;
  const float cr = f2 * to[0] - to[2] * f0;
  *cross = cr;
  if (unordered_or_less(d, 0)) return unordered_or_less(cr, 0) ? -1.0f : 1.0f;
  if (-p.sin_dead > cr) {
    const float v = (cr + p.sin_dead) * p.gain;
    if (unordered_or_less(v, -1.0f)) return -1.0f;
    const float m = -p.least;
    return v > m ? m : v;
  }
  if (!(cr > p.sin_dead)) return 0;
  const float v = (cr - p.sin_dead) * p.gain;
  if (v > 1.0f) return 1.0f;
  return unordered_or_less(v, p.least) ? p.least : v;
}
}  // namespace

// Mech3.exe 1.2 @ 0x004c7900 (start-up): 5 degrees from the float at
// 0x005e8b90 and the double 0x005cb0b0, then 1.2 and 0.1.
const TurnParams& turn_params() {
  static const TurnParams p = [] {
    const float a = static_cast<float>(5.0f * 0.017453292519943295);
    return TurnParams{std::cos(a), std::sin(a), 1.2f, 0.1f};
  }();
  return p;
}

// Mech3.exe 1.2 @ 0x004cdd40
float turn_toward(const float fwd[3], const float to[3], float* dot, float* cross, float keep, const TurnParams& p) {
  *dot = fwd[0] * to[0] + fwd[2] * to[2];
  return turn_rest(fwd[0], fwd[2], to, *dot, cross, keep, p);
}

// Mech3.exe 1.2 @ 0x004cdee0: past [0x005cb174] (-cos 45 degrees) the vehicle
// faces away and the rest is measured from its back.
float turn_or_back(const float fwd[3], const float to[3], float* dot, float* cross, float keep, bool* reverse,
                   const TurnParams& p) {
  const float d = fwd[0] * to[0] + fwd[2] * to[2];
  *dot = d;
  *reverse = unordered_or_less(d, -0.70710677f);
  if (!*reverse) return turn_rest(fwd[0], fwd[2], to, d, cross, keep, p);
  *dot = -d;
  return turn_rest(-fwd[0], -fwd[2], to, *dot, cross, keep, p);
}

// Mech3.exe 1.2 @ 0x004c79f0, after 0x004c7960 (from kept, x00 = 0): a link
// nearer than 1 is measured along the vehicle's facing instead, at least
// 0.1 off, and `from` moved back to make it so.
void LinkWay::measure(const float to[3], const float fwd[3]) {
  dir[0] = to[0] - from[0], dir[1] = to[1] - from[1], dir[2] = to[2] - from[2];
  dir[1] = 0;
  dist = normalize3(dir);
  x00 = 0;
  if (!unordered_or_less(dist, 1.0f)) return;
  dir[0] = fwd[0], dir[1] = fwd[1], dir[2] = fwd[2];
  dir[1] = 0;
  const float len = normalize3(dir);
  if (len == 0 || std::isnan(len)) dir[0] = 0, dir[1] = 0, dir[2] = 1.0f;  // 0x005eda80
  if (unordered_or_less(dist, 0.1f)) dist = 0.1f;
  const float back[3] = {dir[0] * dist, dir[1] * dist, dir[2] * dist};
  from[0] = to[0] - back[0], from[1] = to[1] - back[1], from[2] = to[2] - back[2];
}

// Mech3.exe 1.2 @ 0x004cd430 (the base's and every node's vt+0xa8): the link
// measured, and [0x0060b44c] (0xfffff, set at start-up by 0x004c7950).
float Behaviour::weight(Brain& b, const Behaviour* from, LinkWay* w) {
  (void)from;
  const float* p = position();
  // FIX (bug): a step chain ending in the null step reaches here with no
  // position, which the exe reads (a crash); it weighs nothing.
  if (!p) return 0;
  float fwd[3] = {0, 0, 0};
  brain_hooks().forward(b.controls->vehicle(), &fwd[0], &fwd[2]);
  w->measure(p, fwd);
  return 1048575.0f;
}

// Mech3.exe 1.2 @ 0x004cd470: what follows the step weighs.
float Step::weight(Brain& b, const Behaviour* from, LinkWay* w) { return next->weight(b, from, w); }

// Mech3.exe 1.2 @ 0x004ca210: only while the valve holds the step's value,
// truncated to a whole number (_ftol).
float ValveStep::weight(Brain& b, const Behaviour* from, LinkWay* w) {
  if (!valve || valve->get() != value) return 0;
  return static_cast<float>(crt::msvc_ftol(Step::weight(b, from, w)));
}

// Mech3.exe 1.2 @ 0x004cd580 for jumps and gates (the other value steps
// keep 0x004cd470): only when the pilot may pass.
float ValueStep::weight(Brain& b, const Behaviour* from, LinkWay* w) {
  if (kind != StepKind::Jump) return Step::weight(b, from, w);
  const Pilot p = pilot_of(b);
  return passable(&p, from) ? Step::weight(b, from, w) : 0;
}
float GateStep::weight(Brain& b, const Behaviour* from, LinkWay* w) {
  const Pilot p = pilot_of(b);
  return passable(&p, from) ? Step::weight(b, from, w) : 0;
}

// Mech3.exe 1.2 @ 0x004cdb80: while the union holds (0x004d4600), truncated.
float UnionStep::weight(Brain& b, const Behaviour* from, LinkWay* w) {
  if (!passable(nullptr, from)) return 0;
  return static_cast<float>(crt::msvc_ftol(Step::weight(b, from, w)));
}

// Mech3.exe 1.2 @ 0x004ccf20. The weights go in a scratch list, the exe's
// [0x0060b444] (a vector the node constructor 0x004cf820 reserves through
// 0x004c8f70) or a new one while that is in use; the top order's iterator
// it makes (0x004ce4a0) is never read.
Behaviour* pick_link(Brain& b, Behaviour* at, const float heading[3], std::vector<float>* sums_out) {
  BrainHooks& h = brain_hooks();
  if (Behaviour* n = h.path_next(b, at)) return n;
  const int32_t n = at->link_count();
  const Behaviour* miss = null_step();  // vt+0x10
  LinkWay way;
  const float* p = at->way_point();  // vt+0x1c
  way.from[0] = p[0], way.from[1] = p[1], way.from[2] = p[2];
  Behaviour* picked = nullptr;
  std::vector<float> scratch;
  std::vector<float>& sums = sums_out ? *sums_out : scratch;
  sums.assign(static_cast<size_t>(n > 0 ? n : 0), 0.0f);
  if (n == 1) {
    Behaviour* l = at->link(0);
    if (l && l != miss && l->weight(b, at, &way) > 0) picked = l;
    return picked;
  }
  float total = 0;
  int32_t counted = 0;
  for (int32_t i = 0; i < n; ++i) {
    Behaviour* l = at->link(i);
    float f = 0;
    if (l && l != miss) {
      const float w = l->weight(b, at, &way);
      if (w > 0) {
        const float d = heading[0] * way.dir[0] + heading[2] * way.dir[2];
        // 0x005cb0e4, the doubles 0x005cb160 (-0.495) and 0x005cb168 (0.01),
        // each step rounded once to a float as the x87 does in play
        const float s = crt::x87_mul_f24(d - -1.0f, -0.495);
        f = crt::x87_add_f24(0.01, -static_cast<double>(s)) * w;
      }
    }
    total = total + f;
    sums[static_cast<size_t>(i)] = total;
    if (f != 0 && !std::isnan(f)) ++counted, picked = l;
  }
  if (counted > 1) {
    const float r = static_cast<float>(h.rand()) * total * 3.0518509e-05f;  // 0x005cb170
    size_t first = 0, len = sums.size();
    while (len > 0) {  // the first whose sum is not below r
      const size_t half = len / 2;
      if (r > sums[first + half]) first += half + 1, len -= half + 1;
      else len = half;
    }
    picked = at->link(static_cast<int32_t>(first));
  }
  return picked;
}

// Mech3.exe 1.2 @ 0x004ced40 (with the context 0x004cecf0): the segment set
// from the node to where the pilot last was (brain +0x298), the carrot falling
// back toward the node at the pilot's speed (vt+0x118 on the brain's +0xec
// share), and the stopping distance at that speed (vt+0x120).
NodeState::NodeState(Order* o, Behaviour* node) : BehaviourState(o, node), retry_at(FLT_MAX) {
  Brain& b = *o->owner;
  BrainHooks& h = brain_hooks();
  const zgame::Handle v = b.controls->vehicle();
  float pos[3];
  h.position(v, pos);
  const float speed = h.speed_at(v, b.xec);
  b.steer.set(b.x298, node->position(), speed, v);
  slow = h.stopping_distance(v, speed);
  inv_slow = 1.0f / slow;
}

// Mech3.exe 1.2 @ 0x004c9700
void NodeState::resume() {
  phase = 0;
  order->owner->controls->stop();
}

// Mech3.exe 1.2 @ 0x004cf140: at the node (the carrot fallen back to it and
// the vehicle within its stopping distance) the next link's step takes the
// order, the node's place kept as where the pilot last was; with none the
// pilot stands and asks again every 5 s ([0x005cc560] -5).
void NodeState::run(float dt) {
  (void)dt;
  Brain& b = *order->owner;
  BrainHooks& h = brain_hooks();
  BrainControls& c = *b.controls;
  const zgame::Handle v = c.vehicle();
  float pos[3], fwd[3] = {0, 0, 0};
  h.position(v, pos);
  h.forward(v, &fwd[0], &fwd[2]);
  const Steer& s = b.steer;
  if (phase == 2) return go_on(b, c, 2, true, false);
  if (phase != 0 && phase != 1) return;
  const float away[2] = {std::fabs(s.along), std::fabs(s.side)};
  const float sum = away[0] + away[1];
  if (s.along_at > s.along_min || sum > slow) {
    walk_drive(b, v, pos, fwd, away, sum, slow * 0.5f, &turn, &throttle, inv_slow);
    c.set_turn(turn);
    c.set_throttle(throttle);
    phase = 1;
    return;
  }
  go_on(b, c, 2, false, false);
}

// The three runs' way on (0x004cf1bf, 0x004cfd69, 0x004d0773): the next
// link (given the heading the pilot came by) takes the order, the node's
// place kept as where the pilot last was; with none the pilot stands and
// asks again 5 s later ([0x005cc560] -5), in `wait_phase`. `waiting`: come
// back from that wait; `finder`: (deploy) a way finder (+0x238) ends the
// wait at once.
void NodeState::go_on(Brain& b, BrainControls& c, int32_t wait_phase, bool waiting, bool finder) {
  BrainHooks& h = brain_hooks();
  const Steer& s = b.steer;
  for (;;) {
    if (!waiting) {
      const float heading[3] = {-s.dir[0], 0, -s.dir[1]};
      if (Behaviour* next = pick_link(b, from, heading)) {
        const float* p = from->position();
        b.x298[0] = p[0], b.x298[1] = p[1], b.x298[2] = p[2];
        order->set_behaviour(next);  // 0x004e8120: this state destroyed
        return;
      }
      turn = 0, throttle = 0;
      c.set_turn(0);
      c.set_throttle(throttle);
      retry_at = h.now() - -5.0f;
    }
    waiting = false;
    if (unordered_or_less(h.now(), retry_at) && !(finder && b.path.finder)) {
      phase = wait_phase;
      return;
    }
  }
}

// Mech3.exe 1.2 @ 0x004cfad0. Phase 0 turns the vehicle's back toward the
// aim, 1 backs it up the segment to the node (the walk with its back as its
// front), 2 turns its front toward the aim, then the way on (3 the wait).
// FIX (bug): the exe's aim is a stack vector it never sets in those two
// phases (what the pick's heading left there, or anything); the port aims
// along that heading, the way the pilot came (docs/ai.md 17.4a, fixes.md).
void BackupState::run(float dt) {
  (void)dt;
  Brain& b = *order->owner;
  BrainHooks& h = brain_hooks();
  BrainControls& c = *b.controls;
  const zgame::Handle v = c.vehicle();
  float pos[3], fwd[3] = {0, 0, 0};
  h.position(v, pos);
  h.forward(v, &fwd[0], &fwd[2]);
  const Steer& s = b.steer;
  const float aim[3] = {-s.dir[0], 0, -s.dir[1]};
  const float back[3] = {-fwd[0], -fwd[1], -fwd[2]};
  float dot, cross;
  const float* walk_fwd = back;
  switch (phase) {
    case 0:
      turn = turn_toward(back, aim, &dot, &cross, c.turn_goal(), turn_params());
      if (unordered_or_less(dot, 0.9f)) {  // 0x005cc568
        c.set_turn(turn);
        phase = 0;
        return;
      }
      // The exe's back is made again from the context's forward, which now
      // points at the back: this frame walks with the front.
      walk_fwd = fwd;
      [[fallthrough]];
    case 1: {
      const float away[2] = {std::fabs(s.along), std::fabs(s.side)};
      const float sum = away[0] + away[1];
      if (!(s.along_at > s.along_min) && !(sum > slow)) {
        throttle = 0;
        c.set_throttle(0);
        c.set_turn(turn);
        phase = 2;
        return;
      }
      walk_drive(b, v, pos, walk_fwd, away, sum, slow * 0.5f, &turn, &throttle, inv_slow);
      c.set_turn(turn);
      c.set_throttle(-throttle);
      phase = 1;
      return;
    }
    case 2:
      turn = turn_toward(fwd, aim, &dot, &cross, c.turn_goal(), turn_params());
      if (unordered_or_less(dot, 0.9f)) {
        c.set_turn(turn);
        phase = 2;
        return;
      }
      return go_on(b, c, 3, false, false);
    case 3: return go_on(b, c, 3, true, false);
    default: return;
  }
}

// [0x0060b4d8] (made by the deploy run's first call): a 1 degree dead zone,
// the gain and least turn of turn_params.
const TurnParams& fine_turn_params() {
  static const TurnParams p = [] {
    const double a = 1.0f * 0.01745329251994;  // 0x005cc518, 0x005cc570
    return TurnParams{static_cast<float>(std::cos(a)), static_cast<float>(std::sin(a)), 1.2f, 0.1f};
  }();
  return p;
}

DeployValve& deploy_valve() {
  static DeployValve d;
  return d;
}

// Mech3.exe 1.2 @ 0x004cfe40 (slot +0x08 of the listener at 0x0060b498,
// called at the mission load by 0x004e7b40): "mcbdeploy" found or made and
// set to 0, held while it reads 1. [0x0060b4cc] and [0x0060b4d0], cleared
// too, are not read by the deploy run.
void deploy_mission_load() {
  DeployValve& d = deploy_valve();
  d.valve = valve_get("mcbdeploy");
  d.value = 1;
  if (d.valve) d.valve->set(0);
}

DeployState::DeployState(Order* o, Behaviour* node) : NodeState(o, node) {}

namespace {
// 0x0051e0a0: the flat distance between two points.
float flat_distance(const float a[3], const float b[3]) {
  const float dx = a[0] - b[0], dz = a[2] - b[2];
  return std::sqrt(dz * dz + dx * dx);
}
// A deploy valve's value (its holder's vt+0x04). FIX (bug): a deploy record
// without a valve name has none, which the exe reads (a crash); it reads 0.
int32_t valve_value(const Valve* v) { return v ? v->get() : 0; }
// The yaw that faces from `from` toward `to`, the Mech's set_place angle:
// fpatan(-dx, -dz).
float yaw_toward(const float from[3], const float to[3]) {
  const float vx = to[0] - from[0], vz = to[2] - from[2];
  return static_cast<float>(std::atan2(-static_cast<double>(vx), -static_cast<double>(vz)));
}
}  // namespace

// Mech3.exe 1.2 @ 0x004d00a0. Walked to the node (0, 1), the brain's own
// order held (+0x80 0) and the vehicle brought within 1 m (2); once it has
// settled (its vertical speed under 0.1, 3) the segment is set from the
// deploy target to the node and the vehicle turned to face along it, away
// from the target (4, a 1 degree dead zone first, then 5); then slid onto
// the node at 0.5 m/s facing away from the target (0x29) and set there.
// While "mcbdeploy" reads 1 and its second valve is not at its value it
// holds there facing that way, setting its first valve to its value (5,
// 0x2a); then +0x80 back to 1 and the way on (6 the wait).
void DeployState::run(float dt) {
  Brain& b = *order->owner;
  BrainHooks& h = brain_hooks();
  BrainControls& c = *b.controls;
  const zgame::Handle v = c.vehicle();
  float pos[3], fwd[3] = {0, 0, 0};
  h.position(v, pos);
  h.forward(v, &fwd[0], &fwd[2]);
  Steer& s = b.steer;
  const auto* node = static_cast<const DeployNode*>(from);
  float dot, cross, step = 0;
  switch (phase) {
    case 0:
    case 1: {
      const float away[2] = {std::fabs(s.along), std::fabs(s.side)};
      const float sum = away[0] + away[1];
      if (s.along_at > s.along_min || sum > slow) {
        walk_drive(b, v, pos, fwd, away, sum, slow * 0.5f, &turn, &throttle, inv_slow);
        c.set_turn(turn);
        c.set_throttle(throttle);
        phase = 1;
        return;
      }
      b.params.put<int32_t>(0x80, 0);
      slow = 1.0f;
    }
      [[fallthrough]];
    case 2: {
      const float away[2] = {std::fabs(s.along), std::fabs(s.side)};
      const float sum = away[0] + away[1];
      if (sum > slow) {
        walk_drive(b, v, pos, fwd, away, sum, slow * 0.5f, &turn, &throttle, inv_slow);
        c.set_turn(turn);
        c.set_throttle(throttle);
        phase = 2;
        return;
      }
      throttle = 0, turn = 0;
    }
      [[fallthrough]];
    case 3: {
      if (std::fabs(h.vertical_speed(v)) > 0.1f) {  // 0x005e8bdc
        c.set_throttle(throttle);
        c.set_turn(turn);
        phase = 3;
        return;
      }
      const float speed = h.speed_at(v, b.xec);
      s.set(node->position(), node->target_step->position(), speed, v);
      face[0] = s.dir[0], face[1] = 0, face[2] = s.dir[1];
      turn = turn_toward(fwd, face, &dot, &cross, c.turn_goal(), fine_turn_params());
      goto turned;
    }
    case 4:
      turn = turn_toward(fwd, face, &dot, &cross, c.turn_goal(), turn_params());
    turned:
      if (turn != 0 && !std::isnan(turn)) {
        c.set_throttle(throttle);
        c.set_turn(turn);
        phase = 4;
        return;
      }
      step = dt * 0.5f;
      slow = 0.5f;
      goto slide;
    case 0x29:
      step = slow * dt;
    slide: {
      const float* np = node->position();
      const float dist = flat_distance(np, pos);
      if (dist > step) {
        const float k = step / dist;
        const float* n2 = node->position();
        const float p[3] = {(n2[0] - pos[0]) * k + pos[0], (n2[1] - pos[1]) * k + pos[1], (n2[2] - pos[2]) * k + pos[2]};
        h.place(v, p, yaw_toward(node->target_step->position(), p));
        phase = 0x29;
        return;
      }
      const float* tp = node->target_step->position();
      const float* n3 = node->position();
      h.place(v, n3, yaw_toward(tp, n3));
    }
      [[fallthrough]];
    case 0x2a:
      return hold(b, c, fwd, true);
    case 5:
      return hold(b, c, fwd, false);
    case 6: return go_on(b, c, 6, true, true);
    default: return;
  }
}

// The hold at the node (0x004d0592 / 0x004d05ce, 0x004d06bc): `first` (set
// there, or holding at 0x2a) sets the first valve when it is not at its
// value yet (on to 5); 5 only checks.
void DeployState::hold(Brain& b, BrainControls& c, const float fwd[3], bool first) {
  const auto* node = static_cast<const DeployNode*>(from);
  const DeployValve& d = deploy_valve();
  const bool on = d.valve ? d.valve->get() == d.value : false;  // the exe sets it at every mission load
  const bool open = valve_value(node->valve_b) != static_cast<int32_t>(node->b);
  float dot, cross;
  if (on && open) {
    int32_t next = 5;
    if (first) {
      if (valve_value(node->valve_a) == static_cast<int32_t>(node->a)) next = 0x2a;
      else if (node->valve_a) node->valve_a->set(static_cast<int32_t>(node->a));  // 0x004d3c00
    }
    turn = turn_toward(fwd, face, &dot, &cross, c.turn_goal(), fine_turn_params());
    c.set_turn(turn);
    phase = next;
    return;
  }
  b.params.put<int32_t>(0x80, 1);
  go_on(b, c, 6, false, true);
}

// Mech3.exe 1.2 @ 0x004cee80. [0x0060b4c4] (no backing up) and [0x0060b4c8]
// (no aiming along the segment when near) are never set: both 0.
void walk_drive(Brain& b, zgame::Handle v, const float pos[3], const float fwd[3], const float away[2], float dist,
                float near, float* turn, float* throttle, float inv_slow) {
  BrainHooks& h = brain_hooks();
  const Steer& s = b.steer;
  float to[3];
  s.carrot(to);
  bool reverse = unordered_or_less(s.along, 0);
  bool aimed = false;
  if (!s.near() && h.inout_clamp(v, to)) {  // 0x004c3000: kept inside the mission's boundary
    to[0] = to[0] - pos[0], to[1] = to[1] - pos[1], to[2] = to[2] - pos[2];
    to[1] = 0;
    normalize_flat(to);
    reverse = false;
    aimed = true;
  }
  if (!aimed) {
    if (unordered_or_less(dist, near)) {  // there: face along the segment
      const bool back = unordered_or_less(s.along, 0);
      to[0] = back ? s.dir[0] : -s.dir[0];
      to[1] = 0;
      to[2] = back ? s.dir[1] : -s.dir[1];
    } else {
      to[0] = to[0] - pos[0], to[1] = to[1] - pos[1], to[2] = to[2] - pos[2];
      to[1] = 0;
      normalize_flat(to);
    }
  }
  const float keep = b.controls->turn_goal();  // IControls vt+0x30
  float dot, cross;
  *turn = reverse ? turn_or_back(fwd, to, &dot, &cross, keep, &reverse, turn_params())
                  : turn_toward(fwd, to, &dot, &cross, keep, turn_params());
  float a = away[0] - 0.04f, sd = away[1] - 0.04f;
  if (!(dot > s.x38)) {  // more than 45 degrees off: turn on the spot
    *throttle = 0;
    return;
  }
  if (!(a > 0) && !(sd > 0)) {
    *throttle = 0;
    return;
  }
  if (unordered_or_less(a, 0)) a = 0;
  if (unordered_or_less(sd, 0)) sd = 0;
  const float t = (sd + a * 4.0f) * inv_slow;  // 0x005e8bbc
  *throttle = reverse ? -t : t;
  const float lim = b.xec;
  if (!(lim >= *throttle)) *throttle = lim;
  else if (-lim > *throttle) *throttle = -lim;
}

}  // namespace mech3::ai
