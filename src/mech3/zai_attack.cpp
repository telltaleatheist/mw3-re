#include "mech3/zai_attack.h"

#include <cmath>

#include "gamez/zmath/zmth_vector.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3::ai {

// Mech3.exe 1.2 @ 0x004ce250
void flat_way(const float from[3], const float to[3], FlatWay* w) {
  w->dir[0] = to[0] - from[0], w->dir[1] = to[1] - from[1], w->dir[2] = to[2] - from[2];
  w->dir[1] = 0;
  zmth::Vec3 d{w->dir[0], w->dir[1], w->dir[2]};
  w->dist = zmth::normalize(d);  // 0x004020d0
  w->dir[0] = d.x, w->dir[1] = d.y, w->dir[2] = d.z;
}

// Mech3.exe 1.2 @ 0x004c8d10. A NaN side is a side to the left (-1 behind,
// the left branch ahead); a NaN past the dead zone takes `least`.
float turn_past(float cross, float sin_dead, float gain, float least, float dot) {
  if (!(dot >= 0.0f)) return cross >= 0.0f ? 1.0f : -1.0f;
  if (!(cross >= -sin_dead)) {
    const float v = (cross + sin_dead) * gain;
    if (!(v >= -1.0f)) return -1.0f;
    const float nl = -least;
    return v > nl ? nl : v;
  }
  if (!(cross > sin_dead)) return 0.0f;
  const float v = (cross - sin_dead) * gain;
  if (v > 1.0f) return 1.0f;
  return v >= least ? v : least;
}

// Mech3.exe 1.2 @ 0x004c8df0 (ECX fwd, EDX dir)
float turn_hold(const float fwd[3], const float dir[3], const float* dot, float* cross, float keep,
                const TurnParams& p) {
  *cross = 0;
  if (*dot < -p.cos_dead) return keep == 0.0f || std::isnan(keep) ? 1.0f : keep;  // x87 C3: equal or unordered
  if (*dot >= p.cos_dead) return 0.0f;
  const float c = fwd[2] * dir[0] - dir[2] * fwd[0];
  *cross = c;
  return turn_past(c, p.sin_dead, p.gain, p.least, *dot);
}

// Mech3.exe 1.2 @ 0x004c0810(1): the base reserves one and holds none; the
// stand behaviour is pushed on.
AttackNet::AttackNet() : CompiledNet(znet::alloc_id(300, 300, 400), 1) { elems.push_back(stand_behaviour()); }

// Mech3.exe 1.2 @ 0x004c0a60: the stand behaviour's release (+0xa0) does
// nothing.
AttackNet::~AttackNet() { elems.clear(); }

// Mech3.exe 1.2 @ 0x004c0a70
NetPart* AttackNet::make_part(Order& o) { return new AttackPart(&o, this); }

int32_t& attack_part_unstacked() {
  static int32_t n = 0;
  return n;
}

// Mech3.exe 1.2 @ 0x004c0a90. The vehicle is taken as there (no check).
AttackPart::AttackPart(Order* o, AttackNet* n) : NetPart(o), target(n) {
  Brain& b = *o->owner;
  brain_hooks().position(b.controls->vehicle(), home);  // 0x005a85e0
  b.home_push(home, &prev);
}

// Mech3.exe 1.2 @ 0x004c0b50: the brain's +0xe4 back (FIX (bug): from
// wherever it is in the stack; Brain::homes), the test on the under it had.
AttackPart::~AttackPart() {
  Brain& b = *order->owner;
  const float* under = prev;
  b.home_pop(home);
  if (!under) {
    b.xe8 = 3.4028235e38f;
    b.x230 = 0;
  } else {
    ++attack_part_unstacked();
  }
}

// Mech3.exe 1.2 @ 0x004cac80: the order popped (this state with it), then
// the net freed through its slot 0 (the deleting dtor).
void AttackEndState::resume() {
  AttackNet* n = net;
  order->pop();
  if (n) {
    brain_hooks().on_delete(n);
    delete n;
  }
}

// Mech3.exe 1.2 @ 0x004cad30
void AttackRearmState::resume() {
  Brain& b = *order->owner;
  order->pop();
  if (b.params.get<int32_t>(0x60)) {
    b.xdc = 3.4028235e38f;
    b.params.put<int32_t>(0x3c, 0);
  }
  if (b.params.get<int32_t>(0x5c)) {
    const float t = b.params.get<float>(0x58) + brain_hooks().now();
    b.params.put<int32_t>(0x3c, 0);
    b.xdc = t;
  }
}

// Mech3.exe 1.2 @ 0x004cae10. A step without a position: the pilot stops
// and the order ends.
void AttackReturnState::resume() {
  BrainHooks& h = brain_hooks();
  Brain& b = *order->owner;
  float pos[3];
  h.position(b.controls->vehicle(), pos);  // 0x005a85e0
  Behaviour* step = h.nearest(net, pos);   // 0x004d51d0
  // FIX (bug): the exe asks the step for its position with no check (no
  // step nearby on a net without steps: a crash); here as one without.
  const float* p = step ? step->position() : nullptr;  // vt+0x18
  if (p) {
    way[0] = p[0], way[1] = p[1], way[2] = p[2];
    return;
  }
  b.controls->stop();  // IControls vt+0x14
  order->pop();
}

// Mech3.exe 1.2 @ 0x004cae90 (docs/ai.md 17.2). The speed share is the
// pilot's +0x84 (+0xec when 0); kept inside the boundary (0x004c3000), the
// pilot turns toward where it is pulled and the order ends.
void AttackReturnState::run(float) {
  BrainHooks& h = brain_hooks();
  Brain& b = *order->owner;
  BrainControls& c = *b.controls;
  const zgame::Handle v = c.vehicle();
  // The exe pops the order when 0x005a85e0 answers null (a vehicle without
  // a node); the program's always answer.
  float pos[3];
  h.position(v, pos);
  float cap = b.params.get<float>(0x84);
  bool done = false;
  if (cap == 0.0f || std::isnan(cap)) cap = b.xec;
  float fwd[3] = {};
  h.forward(v, &fwd[0], &fwd[2]);  // the vehicle's +0x24: x and z
  float pulled[3] = {};
  float throttle;
  if (h.inout_clamp(v, pulled)) {
    FlatWay w;
    flat_way(pos, pulled, &w);
    const float dot = fwd[0] * w.dir[0] + fwd[2] * w.dir[2];
    float cross;
    turn = turn_hold(fwd, w.dir, &dot, &cross, turn, turn_params());
    throttle = dot >= 0.9f ? cap : 0.0f;  // 0x005cb0d8
    done = true;
  } else {
    FlatWay w;
    flat_way(pos, way, &w);
    const float dot = fwd[0] * w.dir[0] + fwd[2] * w.dir[2];
    const TurnParams& p = turn_params();
    float t;
    if (dot < -p.cos_dead) {  // behind
      t = turn == 0.0f || std::isnan(turn) ? 1.0f : turn;
    } else if (!(dot >= p.cos_dead)) {  // past the dead zone (a NaN dot too)
      const float cross = fwd[2] * w.dir[0] - fwd[0] * w.dir[2];
      t = turn_past(cross, p.sin_dead, p.gain, p.least, dot);
    } else {
      t = 0.0f;
    }
    turn = t;
    if (!(w.dist >= b.params.get<float>(0x28))) {  // within +0x28 (or a NaN)
      throttle = 0;
      done = true;
    } else {
      throttle = h.stop_share(v, w.dist - (b.params.get<float>(0x2c) + b.params.get<float>(0x28)) * 0.5f);
      if (c.throttle_now() > throttle) throttle = 0;  // the smoothed throttle above it: let it fall
      if (throttle > cap) throttle = cap;
    }
  }
  c.set_throttle(throttle);  // IControls vt+0x20
  c.set_turn(turn);          // vt+0x28
  if (done) order->pop();
}

// Mech3.exe 1.2 @ 0x004c8a70
bool drive_to(Brain& b, zgame::Handle v, const float from[3], const float to[3], float* turn, float* throttle,
              bool back) {
  BrainHooks& h = brain_hooks();
  BrainControls& c = *b.controls;
  bool done = false;
  float cap = b.params.get<float>(0x84);
  if (cap == 0.0f || std::isnan(cap)) cap = b.xec;
  float fwd[3] = {};
  h.forward(v, &fwd[0], &fwd[2]);  // the vehicle's +0x24: x and z
  float pulled[3] = {};
  if (h.inout_clamp(v, pulled)) {  // 0x004c3000
    FlatWay w;
    flat_way(from, pulled, &w);
    const float dot = fwd[0] * w.dir[0] + fwd[2] * w.dir[2];
    float cross;
    *turn = turn_hold(fwd, w.dir, &dot, &cross, *turn, turn_params());
    *throttle = dot >= 0.9f ? cap : 0.0f;  // 0x005cb0d8
    done = true;
  } else {
    FlatWay w;
    flat_way(from, to, &w);
    const float dot = fwd[0] * w.dir[0] + fwd[2] * w.dir[2];
    const TurnParams& p = turn_params();
    float t;
    if (dot < -p.cos_dead) {  // behind
      t = *turn == 0.0f || std::isnan(*turn) ? 1.0f : *turn;
    } else if (!(dot >= p.cos_dead)) {  // past the dead zone (a NaN dot too)
      const float cross = fwd[2] * w.dir[0] - w.dir[2] * fwd[0];
      t = turn_past(cross, p.sin_dead, p.gain, p.least, dot);
    } else {
      t = 0.0f;
    }
    *turn = t;
    if (back && !(w.dist >= b.params.get<float>(0x2c))) {  // too close: back off facing it
      *throttle = dot >= 0.9f ? -cap : 0.0f;
    } else if (!(w.dist >= b.params.get<float>(0x28))) {  // arrived
      *throttle = 0;
      done = true;
    } else {
      *throttle = h.stop_share(v, w.dist - (b.params.get<float>(0x2c) + b.params.get<float>(0x28)) * 0.5f);
      if (c.throttle_now() > *throttle) *throttle = 0;
      if (*throttle > cap) *throttle = cap;
    }
  }
  c.set_throttle(*throttle);  // IControls vt+0x20
  c.set_turn(*turn);          // vt+0x28
  return done;
}

// Mech3.exe 1.2 @ 0x004d1d10. The exe takes the order's part as the vehicle
// net's (its vt+0x0c), as it always is: the brain pushes the fight on that
// net; any other part here gives up.
void FightState::run(float) {
  BrainHooks& h = brain_hooks();
  Brain& b = *order->owner;
  const zgame::Handle v = b.controls->vehicle();
  auto* part = dynamic_cast<VehiclePart*>(order->net.get());
  if (!part) {
    order->pop();
    return;
  }
  Behaviour* mode = part->behaviour;
  const zgame::Handle target = part->target->vehicle;
  float to[3], from[3];
  h.position(target, to);  // the net's vt+0x04 (0x004c02e0)
  h.position(v, from);     // 0x005a85e0
  if (!h.entity_active(target) || b.must_break_off(v)) {
    order->pop();
    return;
  }
  float throttle;
  if (drive_to(b, v, from, to, &turn, &throttle, true) && mode) order->set_behaviour(mode);  // 0x004e8120
}

namespace {
// [0x0060b4a4] = cos(10 degrees), from the start-up's 0x004ce5b0.
float cos_ten_degrees() { return static_cast<float>(std::cos(0.17453292519944444)); }
// The pilot's speed for the carrot (+0x84, or +0xec when it is 0: fcomp,
// test ah 0x40, so a NaN too) at the vehicle's vt+0x118.
float follow_speed(Brain& b, zgame::Handle v) {
  float share = b.params.get<float>(0x84);
  if (!(share < 0.0f || share > 0.0f)) share = b.xec;  // [0x005cc51c] 0
  return brain_hooks().speed_at(v, share);
}
bool same_or_nan(float a, float b) { return !(a < b || a > b); }
bool unordered_or_less(float a, float b) { return !(a >= b); }  // x87 C0 after fcomp a, b
}  // namespace

// Mech3.exe 1.2 @ 0x004d1870 (with the context 0x004cecf0)
FollowState::FollowState(Order* o, Behaviour* b) : BehaviourState(o, b), made_by(b) {
  BrainHooks& h = brain_hooks();
  Brain& br = *o->owner;
  const zgame::Handle v = br.controls->vehicle();
  auto* part = dynamic_cast<VehiclePart*>(o->net.get());
  if (!part || !part->target) return;
  float pos[3], to[3];
  h.position(v, pos);
  h.position(part->target->vehicle, to);  // the net's vt+0x04 (0x004c02e0)
  br.steer.set(pos, to, follow_speed(br, v), v);
  br.steer.x38 = cos_ten_degrees();
}

// The run's two resets (0x004d1a20, 0x004d1aea): the segment set again from
// `to`, the carrot where it was from the pilot, the side offset kept, the
// carrot no nearer than `floor`.
void FollowState::aim(Brain& b, zgame::Handle v, const float pos[3], const float to[3], float floor) {
  Steer& s = b.steer;
  const float along = s.along, across = s.across;
  s.set(pos, to, follow_speed(b, v), v);  // 0x004cea30
  s.x38 = cos_ten_degrees();
  s.across = across;
  s.along_at = s.along_at - along;
  s.along_min = floor;
  if (unordered_or_less(s.along_at, s.along_min)) s.along_at = s.along_min;
}

// Mech3.exe 1.2 @ 0x004d1980
void FollowState::run(float dt) {
  BrainHooks& h = brain_hooks();
  Brain& b = *order->owner;
  BrainControls& c = *b.controls;
  const zgame::Handle v = c.vehicle();
  auto* part = dynamic_cast<VehiclePart*>(order->net.get());
  if (!part || !part->target) {
    order->pop();
    return;
  }
  const zgame::Handle target = part->target->vehicle;
  float to[3], pos[3], fwd[3] = {0, 0, 0};
  h.position(v, pos);      // the context's (0x004cecf0)
  h.position(target, to);  // the net's vt+0x04 (0x004c02e0)
  if (!h.entity_active(target)) {  // +0x08
    order->pop();  // order vt+0x08
    return;
  }
  float slack_along = 0, slack_side = 0;
  float at[3] = {to[0], to[1], to[2]};
  if (h.inout_clamp(v, at)) {  // 0x004c3000: the boundary pulls the target's place
    const Steer& s = b.steer;
    if (!(pulled && same_or_nan(at[0], s.start[0]) && same_or_nan(at[2], s.start[2]))) aim(b, v, pos, at, 0.0f);
    pulled = 1;
  } else {
    const float p28 = b.params.get<float>(0x28), p2c = b.params.get<float>(0x2c);
    aim(b, v, pos, to, (p2c + p28) * 0.5f);  // 0x005cc564
    slack_along = (p28 - p2c) * 0.5f;
    slack_side = 2.0f;  // 0x005e8be0
    pulled = 0;
  }
  Steer& s = b.steer;
  s.measure(v, dt);  // 0x004ceb50
  const float away[2] = {std::fabs(s.along), std::fabs(s.side)};  // 0x004cee50
  const float sum = away[0] + away[1];
  h.forward(v, &fwd[0], &fwd[2]);
  float turn = 0, throttle = 0;
  walk_drive(b, v, pos, fwd, away, sum, 2.0f, &turn, &throttle, 0.05f, slack_along, slack_side);  // 0x005e8be4, 0x005e8be8
  c.set_turn(turn);          // IControls vt+0x28
  c.set_throttle(throttle);  // vt+0x20
}

// Mech3.exe 1.2 @ 0x004cc150. The other is the vehicle's bearer (its vt+0x2c
// then vt+0x40: a mech itself).
BehaviourState* make_station_state(Behaviour& b, Order& o) {
  BrainHooks& h = brain_hooks();
  Brain* br = o.owner;
  const zgame::Handle v = br && br->controls ? br->controls->vehicle() : 0;
  if (!v || !o.net) return nullptr;
  auto* vn = dynamic_cast<VehicleNet*>(o.net->net());  // vt+0x08, then the net's vt+0x58
  if (!vn || !vn->vehicle) return nullptr;
  const zgame::Handle other = h.bearer(vn->vehicle);
  if (h.vehicle_xf4(v)) {
    if (other && h.vehicle_xf4(other)) return nullptr;
    return new StationHoldState(&o, &b);  // 0x004cc240
  }
  // FIX (bug): the exe reads a missing other's +0xbc (a crash); here none.
  if (h.vehicle_flags(v) & 0x10) {
    if (!other || (h.vehicle_flags(other) & 0x10) || h.player_vehicle() == other) return nullptr;
  }
  return new StationState(&o, &b);  // 0x004cc290
}

// Mech3.exe 1.2 @ 0x004cc290 (over 0x004c8a30)
StationState::StationState(Order* o, Behaviour* b) : BehaviourState(o, b), made_by(b) { take_params(); }

void StationState::take_params() {
  Brain& br = *order->owner;
  way = br.controls->reversing() ? -1.0f : 1.0f;  // IControls vt+0x18
  close = br.params.get<float>(0x78) * 0.9f;  // 0x005cb0d8
  range = std::sqrt(br.params.get<float>(0x74)) * 1.1f;  // 0x005cb140, 1.1f as a double
}

// Mech3.exe 1.2 @ 0x004cc380
void StationState::resume() { take_params(); }

namespace {
// The turn toward `to` (flat) the walking states' way, `keep` straight back
// (1 for none): the run's three inlined copies (0x004cc80b, 0x004cc8f3,
// 0x004cca33).
float turn_onto(const float fwd[3], const float to_x, const float to_z, float dot, float keep) {
  const TurnParams& p = turn_params();
  if (dot < -p.cos_dead) return keep == 0.0f || std::isnan(keep) ? 1.0f : keep;
  if (dot >= p.cos_dead) return 0.0f;  // a NaN turns
  const float cross = fwd[2] * to_x - fwd[0] * to_z;
  return turn_past(cross, p.sin_dead, p.gain, p.least, dot);
}
}  // namespace

// Mech3.exe 1.2 @ 0x004cc3e0 (docs/ai.md 17.2). Phases 1..3 fall back to 0
// in the same run when they no longer hold.
void StationState::run(float) {
  BrainHooks& h = brain_hooks();
  Brain& br = *order->owner;
  BrainControls& c = *br.controls;
  const zgame::Handle v = c.vehicle();
  auto* vn = order->net ? dynamic_cast<VehicleNet*>(order->net->net()) : nullptr;  // vt+0x0c, vt+0x08, vt+0x58
  const zgame::Handle target = vn ? vn->vehicle : 0;
  bool done = false, idle = false;
  if (!target || !h.entity_active(target)) {  // +0x08
    order->pop();
    return;
  }
  float tp[3], vp[3];
  h.position(target, tp);  // 0x005a85e0
  h.position(v, vp);
  FlatWay w;
  flat_way(vp, tp, &w);   // 0x004ce230
  float fwd[3] = {};
  h.forward(v, &fwd[0], &fwd[2]);  // +0x24
  const zgame::Handle other = h.bearer(target);  // vt+0x2c, vt+0x40
  float ov[3], tv[3];
  h.velocity(v, ov);  // vt+0x08
  (void)std::fabs(h.move_speed(v));  // vt+0x114: read, not used
  if (other) {
    h.velocity(other, tv);
    (void)std::fabs(h.move_speed(other));
  } else {
    tv[0] = tv[1] = tv[2] = 0;
  }
  zmth::Vec3 rel{ov[0] - tv[0], ov[1] - tv[1], ov[2] - tv[2]};
  const float rel_speed = zmth::normalize(rel);  // 0x004020d0
  const float* d = w.dir;
  const float dist = w.dist;
  const float closing = (rel.x * d[0] + rel.y * d[1]) + rel.z * d[2];
  const float side = d[0] * rel.z - d[2] * rel.x;
  const float ahead = (fwd[0] * d[0] + fwd[1] * d[1]) + fwd[2] * d[2];
  const float p78 = br.params.get<float>(0x78);
  bool again;
  int zeros = 0;
  do {
    again = false;
    switch (phase) {
      case 0:
        // FIX (bug): phase 0 again in one run decides as before, so the exe
        // loops for ever (a hang) when +0x78 is below the +0x24 it took
        // (0.9 of it: a negative +0x78, none shipped); here the run stops.
        if (++zeros > 1) break;
        if (dist > range) {  // past +0x28: the order ends
          done = true;
          count = 0;
        } else if (!(dist >= close)) {  // too close (a NaN too)
          if (ahead > 0.5f) phase = 1, way = -1.0f;        // 0x005e8b94
          else if (!(ahead >= -0.5f)) phase = 2, way = 1.0f;   // 0x005e8b98 (a NaN too)
          else phase = 3, way = !(way >= 0.0f) ? -1.0f : 1.0f;
          again = true;
          count = 0;
        } else if (rel_speed > 0.2f && closing > 0.0f && !(closing * dist > p78)) {  // 0x005cb138
          turn = 0;  // closing in: swerve, 8 frames
          if (side < -0.17f) turn = 1.0f;       // -[0x005e8b9c], 0x005e8ba0
          else if (side > 0.17f) turn = -1.0f;
          way = c.reversing() ? -1.0f : 1.0f;   // IControls vt+0x18
          count = 8;
        } else if (count != 0) {
          turn = 0;
          way = c.reversing() ? -1.0f : 1.0f;
          --count;
        } else {
          idle = true;  // the controls left alone
        }
        break;
      case 1:  // the other ahead: back off facing it
        if (!(ahead >= 0.0f) || dist > p78) {
          phase = 0, again = true;
          break;
        }
        turn = turn_onto(fwd, d[0], d[2], fwd[0] * d[0] + fwd[2] * d[2], turn);
        break;
      case 2: {  // behind: on, its back to it
        if (ahead > 0.0f || dist > p78) {
          phase = 0, again = true;
          break;
        }
        const float back[3] = {-fwd[0], -fwd[1], -fwd[2]};
        turn = turn_onto(back, d[0], d[2], back[0] * d[0] + back[2] * d[2], turn);
        break;
      }
      case 3: {  // beside: alongside, across the way to it
        if (!(ahead >= -0.8) || ahead > 0.8 || dist > p78) {  // 0x005cb150, 0x005cb158: doubles
          phase = 0, again = true;
          break;
        }
        float px = d[2], pz = -d[0];
        const float g = fwd[0] * d[0] + fwd[2] * d[2];
        float g2 = g;
        if (!(g >= 0.0f)) px = -px, pz = -pz, g2 = -g;
        turn = turn_onto(fwd, px, pz, g2, turn);
        break;
      }
      default: break;
    }
  } while (again);
  if (done) {
    order->pop();  // 0x004e8270 (the own order's)
    return;
  }
  if (idle) return;
  c.set_turn(turn);   // IControls vt+0x28
  c.set_throttle(way);  // vt+0x20
}

// Mech3.exe 1.2 @ 0x004cc020. The other vehicle through the order's part
// (the vehicle net's, as always here).
void StationHoldState::run(float) {
  BrainHooks& h = brain_hooks();
  Brain& br = *order->owner;
  auto* vn = order->net ? dynamic_cast<VehicleNet*>(order->net->net()) : nullptr;
  const zgame::Handle other = vn ? vn->vehicle : 0;
  if (other) {
    float a[3], t[3];
    h.position(br.controls->vehicle(), a);  // 0x005a85e0
    h.position(other, t);
    const float dx = t[0] - a[0], dy = t[1] - a[1], dz = t[2] - a[2];
    const float d = std::sqrt((dx * dx + dy * dy) + dz * dz);
    if (!(d >= range)) {  // within (a NaN too)
      // [0x0060b48c] (never set): the pilot stopped.
      phase = 1;
      return;
    }
  }
  order->pop();
}

// Mech3.exe 1.2 @ 0x004e9ca0
bool attack(Brain& b, AiNet* n) {
  b.remake(n);  // 0x004e82e0
  auto* an = new AttackNet();
  brain_hooks().on_new(0x1c, an);
  AttackEnd end(an);
  AttackRearm rearm;
  AttackReturn home(n);
  b.push(an, &end);   // 0x004e87c0
  b.push(an, &rearm);
  b.push(an, &home);
  return true;
}

}  // namespace mech3::ai
