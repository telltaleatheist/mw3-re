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
  prev = b.xe4;
  brain_hooks().position(b.controls->vehicle(), home);  // 0x005a85e0
  b.xe4 = home;
}

// Mech3.exe 1.2 @ 0x004c0b50
AttackPart::~AttackPart() {
  Brain& b = *order->owner;
  b.xe4 = prev;
  if (!prev) {
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
