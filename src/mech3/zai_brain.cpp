#include "mech3/zai_brain.h"

#include "mech3/zai_attack.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "crt/msvc_rand.h"
#include "gamez/znetwork/znet_object.h"
#include "gamez/zsound/zsnd.h"
#include "mech3/sound_extra.h"
#include "mech3/zai_node.h"
#include "mech3/zai_path.h"
#include "mech3/zai_steps.h"
#include "mech3/zai_target.h"
#include "mech3/zai_valve.h"
#include "mech3/zainet_region.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3::ai {

namespace {
uint32_t bits(float f) {
  uint32_t u;
  std::memcpy(&u, &f, 4);
  return u;
}
float fbits(uint32_t u) {
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}
BrainHooks g_default_hooks;
BrainHooks* g_hooks = &g_default_hooks;


// [0x0060b4a0] = cos(pi/4), computed by the start-up code 0x004ce5b0 from the
// double at 0x005cc530.
float cos_quarter_pi() { return static_cast<float>(std::cos(0.78539816339744828)); }
}  // namespace

BrainHooks& brain_hooks() { return *g_hooks; }
void set_brain_hooks(BrainHooks* h) { g_hooks = h ? h : &g_default_hooks; }

znet::Object* BrainHooks::net_find(int32_t id) { return znet::find(id); }
void* BrainHooks::sound_find(const char* name) { return zsnd::find(name); }
Behaviour* BrainHooks::default_behaviour() { return mech3::ai::default_behaviour(); }
Behaviour* BrainHooks::nearest(AiNet* n, const float p[3]) { return nearest_step(*n, p); }
uint8_t BrainHooks::vehicle_flags(zgame::Handle v) {
  AiTarget* t = targetable(v);
  return t ? t->xbc : 0;
}
float BrainHooks::now() { return fbits(unported("[0x0060991c]", {})); }
float BrainHooks::health(zgame::Handle v) { return fbits(unported("vehicle.vt+0x98", {v})); }
void BrainHooks::position(zgame::Handle v, float out[3]) {
  out[0] = out[1] = out[2] = 0;
  unported("0x005a85e0", {v});
}
bool& ai_switch() {
  static bool on = false;
  return on;
}
bool BrainHooks::ai_on() { return ai_switch(); }
int32_t BrainHooks::powered_down(zgame::Handle v) { return static_cast<int32_t>(unported("vehicle.vt+0xa0", {v})); }
Contact* BrainHooks::pick(zgame::Handle v, int32_t side) {
  AiTarget* t = targetable(v);
  return t ? t->first_of(side) : nullptr;
}
zgame::Handle BrainHooks::contact_vehicle(const Contact* c) { return unported("0x004e6fe0", {low(c)}); }
float BrainHooks::contact_range2(zgame::Handle v) {
  AiTarget* t = targetable(v);
  return t ? t->range2 : fbits(unported("vehicle+0xa4", {v}));
}
void BrainHooks::count_awake() { unported("[0x006099c0]++", {}); }
float BrainHooks::intensity() { return fbits(unported("[0x00608e54]", {})); }
int32_t BrainHooks::rand() { return crt::msvc_rand(); }
bool BrainHooks::ammo_value(zgame::Handle v, float* out) {
  *out = 1.0f;
  return unported("0x004b4ed0", {v}) != 0;
}
const std::vector<Contact*>* BrainHooks::contacts(zgame::Handle v) {
  AiTarget* t = targetable(v);
  return t ? &t->contacts : nullptr;
}
void BrainHooks::forward(zgame::Handle e, float* x, float* z) {
  unported("entity+0x24", {e});
  *x = *z = 0;
}
// 0x004e6d50(1.0) on the radio's sound for `event` (Radio::kEvents).
void BrainHooks::radio_line(Brain& b, int32_t event) {
  if (b.radio && event >= 0 && event < 16) sound_say(static_cast<zsnd::Object*>(b.radio->sounds[event]), 1.0f);
}
float BrainHooks::heat(zgame::Handle v) { return fbits(unported("vehicle.vt+0x9c", {v})); }
Behaviour* BrainHooks::path_next(Brain& b, Behaviour* at) { return b.path.next(at); }
float BrainHooks::speed_at(zgame::Handle v, float share) { return fbits(unported("vehicle.vt+0x118", {v, bits(share)})); }
float BrainHooks::stopping_distance(zgame::Handle v, float speed) {
  return fbits(unported("vehicle.vt+0x120", {v, bits(speed)}));
}
float BrainHooks::stop_share(zgame::Handle v, float d) { return fbits(unported("vehicle.vt+0x12c", {v, bits(d)})); }
bool BrainHooks::clear_line(zgame::Handle v, zgame::Handle seen) { return ai::clear_line(v, seen, nullptr); }
bool BrainHooks::clear_line_at(zgame::Handle v, zgame::Handle e) {
  float at[3];
  entity_lock_position(e, at);
  return ai::clear_line(v, e, at);
}
bool BrainHooks::region_inside(Region* r, const float* p) { return r->inside(p); }
void BrainHooks::firing_reset(Brain& b) { b.firing.stand_down(); }    // 0x004e9590
FirePlan* BrainHooks::fire_plan_pick(Brain& b) { return volley_plan_pick(b.firing); }
void BrainHooks::aim_track(Brain& b, Contact* target, zgame::Handle w) { b.firing.aim->track(b, target, w); }
void BrainHooks::aim_lead(Brain& b, float dt, Contact* target, zgame::Handle w, zgame::Handle wm) {
  b.firing.aim->lead(b, dt, target, w, wm);
}
bool BrainHooks::select_target(zgame::Handle v, Contact* c) {
  AiTarget* t = targetable(v);
  return t ? t->set_target(c, false) : false;  // vt+0xc0: 0x004d5e40 (the Mech's 0x0041d890 adds the player's HUD lock)
}
void BrainHooks::firing_run(Brain& b, float dt) { b.firing.run(dt); }  // 0x004e9fb0
AiNet* BrainHooks::vehicle_net(zgame::Handle v) {  // vt+0xd8: the Mech's 0x004d70f0
  AiTarget* t = targetable(v);
  return t ? vehicle_net_of(*t, v) : nullptr;
}
// 0x00603fb8, made at start-up in the exe: here on first ask, and again
// should znet have let it go.
AiNet* BrainHooks::formation_net() {
  static FormationNet* net = nullptr;
  static int32_t id = -1;
  if (!net || znet::find(id) != net) {
    net = new FormationNet;
    id = net->get_id();
  }
  return net;
}

Behaviour* BrainHooks::follow_behaviour(AiNet* n) {  // 0x004c0de0
  auto* vn = dynamic_cast<VehicleNet*>(n);
  return vn ? vn->follow() : nullptr;
}
Behaviour* BrainHooks::attack_behaviour(AiNet* n) {  // 0x004c0e10 (the exe calls it on the vehicle's net)
  auto* vn = dynamic_cast<VehicleNet*>(n);
  return vn ? vn->attack() : nullptr;
}
bool BrainHooks::attack(Brain& b, AiNet* n) { return ai::attack(b, n); }  // 0x004e9ca0
void BrainHooks::own_order_run(Brain& b, float dt) { b.run_own_order(dt); }

Behaviour* BrainHooks::manoeuvre_behaviour(int32_t which) {
  // 0x00410650 / 0x00410680 at start-up; their state is zai_attack's
  // ManoeuvreState.
  static Behaviour evade(StepKind::Manoeuvre2), circle(StepKind::Manoeuvre);
  return which == 0 ? &evade : &circle;
}

// Mech3.exe 1.2 @ 0x004e7ff0: the parts built in place go with their
// order (their slot 0 with 0: no free), the state first.
Order::~Order() {
  state.reset();
  net.reset();
  owner = nullptr;
}

// Mech3.exe 1.2 @ 0x004e8150
void Order::run(float dt) {
  net->run(dt);
  state->run(dt);
}

// Mech3.exe 1.2 @ 0x004e8170; the brain's own order's 0x004e8270: its state
// destroyed, then its part.
void Order::pop() {
  if (!own) {
    owner->pop();
    return;
  }
  state.reset();
  net.reset();
}

// Mech3.exe 1.2 @ 0x004e81a0. The exe asks the net for its own-order
// behaviour (0x004c0e40) as a vehicle's own net, which vt+0xd8 always is.
void Brain::run_own_order(float dt) {
  BrainHooks& h = brain_hooks();
  Order& o = order;
  if (!o.state) {
    const zgame::Handle v = controls ? controls->vehicle() : 0;
    const Contact* c = v ? h.pick(v, kAny) : nullptr;  // 0x004d6b90(3)
    const zgame::Handle cv = c ? h.contact_vehicle(c) : 0;  // 0x004e6fe0
    const zgame::Handle bv = cv ? h.bearer(cv) : 0;          // vt+0x40
    if (bv && !(c->dist2 >= params.get<float>(0x74))) {
      if (AiNet* n = h.vehicle_net(bv)) {  // vt+0xd8
        o.net.reset(n->make_part(o));     // vt+0x5c, in place at +0x30
        if (o.net) {
          auto* vn = dynamic_cast<VehicleNet*>(n);
          Behaviour* own = vn ? vn->own_order() : nullptr;
          o.state.reset(own ? own->make_state(o) : nullptr);  // vt+0x8c, at +0x54
          if (!o.state) o.net.reset();
        }
      }
    }
  }
  if (o.net) o.net->run(dt);
  if (o.state) o.state->run(dt);
}

// Mech3.exe 1.2 @ 0x004e80b0: the old state and part destroyed, then the
// net's part (the built-in net's for none) and the behaviour's state (the
// default behaviour's for none).
bool Order::reset(AiNet* n, Behaviour* b) {
  state.reset();
  net.reset();
  if (!b) b = brain_hooks().default_behaviour();
  if (!n) n = brain_hooks().builtin_net();
  net.reset(n->make_part(*this));
  state.reset(b->make_state(*this));
  return true;
}

// Mech3.exe 1.2 @ 0x004e8120
bool Order::set_behaviour(Behaviour* b) {
  state.reset();
  state.reset(b->make_state(*this));
  return true;
}

// Mech3.exe 1.2 @ 0x004c0070 -> 0x004c0010
NetPart* CompiledNet::make_part(Order& o) { return new NetLink(&o, this, false); }
// Mech3.exe 1.2 @ 0x004c0520 -> 0x004c0540
NetPart* BuiltinNet::make_part(Order& o) { return new NetLink(&o, this, true); }

// Mech3.exe 1.2 @ 0x004c0090. The base (0x004bfbb0) reserves seven, the
// list is grown to seven empty slots, then each is made (0x004c8600 and
// its class's vtable).
VehicleNet::VehicleNet(zgame::Handle v)
    : CompiledNet(znet::alloc_id(300, 300, 400), 7), vehicle(v) {
  static const StepKind kinds[7] = {StepKind::VehFollow, StepKind::VehAttack, StepKind::VehMode1,
                                    StepKind::VehMode2,  StepKind::Manoeuvre, StepKind::VehOwn,
                                    StepKind::VehMode4};
  elems.assign(7, nullptr);
  for (int i = 0; i < 7; ++i) {
    elems[size_t(i)] = new Behaviour(kinds[i]);
    brain_hooks().on_new(0xc, elems[size_t(i)]);
  }
}

// Mech3.exe 1.2 @ 0x004c02f0
NetPart* VehicleNet::make_part(Order& o) { return new VehiclePart(&o, this); }

namespace {
Behaviour* vehicle_slot(const VehicleNet& n, size_t i) {
  znet::Element* e = i < n.elems.size() ? n.elems[i] : nullptr;
  return e ? static_cast<AiElem*>(e)->as_behaviour() : nullptr;  // +0x60 then +0x74
}
}  // namespace

// Mech3.exe 1.2 @ 0x004c0de0, 0x004c0e10, 0x004c0e40
Behaviour* VehicleNet::follow() const {
  Behaviour* b = vehicle_slot(*this, 0);
  return b ? b->as_vehicle_step() : nullptr;
}
Behaviour* VehicleNet::attack() const {
  Behaviour* b = vehicle_slot(*this, 1);
  return b ? b->as_attack() : nullptr;
}
Behaviour* VehicleNet::own_order() const {
  Behaviour* b = vehicle_slot(*this, 5);
  return b ? b->as_own_order() : nullptr;
}

Behaviour* Behaviour::as_vehicle_step() {
  switch (kind) {
    case StepKind::VehFollow:
    case StepKind::VehAttack:
    case StepKind::VehMode1:
    case StepKind::VehMode2:
    case StepKind::VehOwn:
    case StepKind::VehMode4: return this;
    default: return nullptr;
  }
}

// Mech3.exe 1.2 @ 0x004d70f0
VehicleNet* vehicle_net_of(AiTarget& t, zgame::Handle vehicle) {
  if (!t.net) {
    t.net = new VehicleNet(vehicle);
    brain_hooks().on_new(0x20, t.net);
  }
  return t.net;
}

// Mech3.exe 1.2 @ 0x004c0ba0. The mode's slot by its +0x90 test: the
// manoeuvre's (mode 3) answers none.
VehiclePart::VehiclePart(Order* o, VehicleNet* n) : NetPart(o), target(n) {
  Brain& b = *o->owner;
  if (!b.controls || !b.controls->vehicle()) pending = true;
  else push_home();
  static const size_t slot[5] = {0, 2, 3, 4, 6};
  const int32_t mode = b.params.get<int32_t>(0x40);
  if (mode >= 1 && mode <= 4) {
    Behaviour* s = vehicle_slot(*n, slot[mode]);
    behaviour = s ? s->as_vehicle_step() : nullptr;
  }
}

void VehiclePart::push_home() {
  Brain& b = *order->owner;
  brain_hooks().position(b.controls->vehicle(), home);  // 0x005a85e0
  b.home_push(home, &prev);
  pushed = true;
}

// Mech3.exe 1.2 @ 0x004c0dc0: the brain's +0xe4 back (FIX (bug): from
// wherever it is in the stack; Brain::homes).
VehiclePart::~VehiclePart() {
  if (pushed) order->owner->home_pop(home);
  else order->owner->xe4 = prev;  // made pending and never run: the exe's +0x14 (0) back
}

void Brain::home_push(const float* home, const float** under) {
  *under = xe4;
  homes.push_back({home, under});
  xe4 = home;
}

void Brain::home_pop(const float* home) {
  for (size_t i = homes.size(); i-- > 0;) {
    if (homes[i].home != home) continue;
    const float* below = *homes[i].under;
    if (i + 1 == homes.size()) xe4 = below;  // on top: as the exe
    else *homes[i + 1].under = below;        // from the middle: the one above takes its under
    homes.erase(homes.begin() + static_cast<std::ptrdiff_t>(i));
    return;
  }
}

// Mech3.exe 1.2 @ 0x004c0d60
void VehiclePart::run(float) {
  if (!pending) return;
  // FIX (bug): the exe takes the vehicle's position without asking whether
  // the pilot has one by now (a crash); here it waits for one.
  Brain& b = *order->owner;
  if (!b.controls || !b.controls->vehicle()) return;
  pending = false;
  push_home();
}

// Mech3.exe 1.2 @ 0x0040a890
AiNet* VehiclePart::net() const { return target; }

namespace {
// The pilot's controls stopped and the fire let go (the base run 0x004c8690).
void stop_and_hold(Brain& b) {
  b.controls->stop();        // IControls vt+0x14
  b.controls->fire(false);   // IControls vt+0x10(0)
}

// The default behaviour's state (0x004c8770 -> 0x004c8790, vtable 0x005cb330).
class DefaultState : public BehaviourState {
 public:
  using BehaviourState::BehaviourState;
  bool may_break_in() const override { return false; }  // 0x004d72a0
  bool may_fire() const override { return false; }      // 0x004d7290
  void run(float dt) override;                          // 0x004c87f0
  Behaviour* behaviour() const override { return default_behaviour(); }  // 0x004c87e0: 0x00604068
};

// Mech3.exe 1.2 @ 0x004c87f0: stopped; the net's step nearest the vehicle
// becomes this order's behaviour (this state goes), and the order runs again.
void DefaultState::run(float dt) {
  (void)dt;
  Order* o = order;
  Brain& b = *o->owner;
  BrainHooks& h = brain_hooks();
  const zgame::Handle v = b.controls->vehicle();
  stop_and_hold(b);
  if (!v || !o->net) return;
  AiNet* n = o->net->net();
  if (!n) return;
  float p[3];
  h.position(v, p);
  Behaviour* step = h.nearest(n, p);
  if (!step) return;
  o->set_behaviour(step);  // 0x004e8120: this state destroyed
  b.x104 = 1;
}

// 0x004c8700 (vtable 0x005cb258): the null step's.
class NullStepState : public BehaviourState {
 public:
  using BehaviourState::BehaviourState;
  Behaviour* behaviour() const override { return null_step(); }  // 0x004106c0
};

// 0x004c8940 (vtable 0x005cb370) for 0x00604058: the base's run.
class StandState : public BehaviourState {
 public:
  using BehaviourState::BehaviourState;
  void run(float dt) override { BehaviourState::run(dt); }  // 0x004c89a0
  Behaviour* behaviour() const override { return stand_behaviour(); }  // 0x004c8960
};

// 0x004c88c0 (vtable 0x005cb350) for 0x00604028: nothing a frame.
class HoldState : public BehaviourState {
 public:
  using BehaviourState::BehaviourState;
  void run(float dt) override { (void)dt; }  // 0x004bf620
  Behaviour* behaviour() const override { return hold_behaviour(); }  // 0x004c88e0
};

// A speed step's state (0x004c9ed0 -> 0x004c9ef0, vtable 0x005cb540): the
// pilot's speed share (brain +0xec) set to the step's, then the next step.
class SpeedState : public BehaviourState {
 public:
  using BehaviourState::BehaviourState;
  void run(float dt) override;                                  // 0x004c9f40
  Behaviour* behaviour() const override { return from; }        // 0x004c0030
};

// Mech3.exe 1.2 @ 0x004c9f40: the order run again (+0x104) on what follows.
void SpeedState::run(float dt) {
  (void)dt;
  const auto* st = static_cast<const ValueStep*>(from);
  Brain& b = *order->owner;
  b.xec = fbits(st->value);
  b.x104 = 1;
  order->set_behaviour(st->next);  // 0x004e8120: this state destroyed
}

// A valve step's state (0x004ca1f0 -> 0x004ca290, vtable 0x005cb7c8; shunt
// and valve union states run the same 0x004ca2e0): the way finder's next
// step, else the one that follows.
class PassState : public BehaviourState {
 public:
  using BehaviourState::BehaviourState;
  void run(float dt) override;                                  // 0x004ca2e0
  Behaviour* behaviour() const override { return from; }        // 0x004c0030
};

// A valve_assign step's state (0x004cdc80 -> 0x004cdca0, vtable
// 0x005cc4f8): its valve set to its value, then as the valve step's.
class AssignState : public PassState {
 public:
  using PassState::PassState;
  void run(float dt) override;                                  // 0x004cdcf0
};

// Mech3.exe 1.2 @ 0x004cdcf0
void AssignState::run(float dt) {
  const auto* st = static_cast<const AssignStep*>(from);
  // FIX (bug): a record whose valve is not found has none, which the exe
  // sets (a crash); nothing is set.
  if (st->valve) st->valve->set(st->value);  // 0x004d3c00: its reactions fire
  PassState::run(dt);
}

// Mech3.exe 1.2 @ 0x004ca2e0
void PassState::run(float dt) {
  (void)dt;
  Brain& b = *order->owner;
  Behaviour* next = brain_hooks().path_next(b, from);  // +0x238, 0x004c7cb0
  if (!next) next = static_cast<const Step*>(from)->next;
  b.x104 = 1;
  order->set_behaviour(next);  // 0x004e8120: this state destroyed
}

// A scan or canleave step's state (0x004cbc30 -> 0x004cbc50, vtable
// 0x005cbc88): each of the step's tests run on the brain, then the next.
class ScanState : public BehaviourState {
 public:
  using BehaviourState::BehaviourState;
  void run(float dt) override;                                  // 0x004cbca0
  Behaviour* behaviour() const override { return from; }        // 0x004c0030
};

// Mech3.exe 1.2 @ 0x004cbca0. Each test is called on the brain plus the
// entry's second word (0 in every step the net builds).
void ScanState::run(float dt) {
  (void)dt;
  const auto* st = static_cast<const ScanStep*>(from);
  Brain& b = *order->owner;
  for (const auto& t : st->tests) b.scan_test(t.first);
  b.x104 = 1;
  order->set_behaviour(st->next);  // 0x004e8120: this state destroyed
}

// 0x004cbe10 (vtable 0x005cbe18), made at start-up into 0x0060b460.
struct SharedBehaviour : Behaviour {
  SharedBehaviour() : Behaviour(StepKind::Shared) {}
  BehaviourState* make_state(Order& o) override { return new SharedState(&o); }  // 0x004cbe30
};
}  // namespace

// Mech3.exe 1.2 @ 0x004c8620 (the base, the step base), 0x004c8770 (the
// default behaviour), 0x004c86e0 (the null step), 0x004c8920 / 0x004c88a0
// (0x00604058 / 0x00604028), 0x004c9150 (a node), 0x004cfa60 (a backup
// node), 0x004d0020 (a deploy node), 0x004c9ed0 (a speed step),
// 0x004ca1f0 (a valve step), 0x004cbc30 (scan and canleave steps). The
// other behaviours' own
// states come with their ports (docs/ai.md 17.9); until then they get the
// base's.
// Mech3.exe 1.2 @ 0x004c9fc0 / 0x004ca0e0
WaitState::WaitState(Order* o, Behaviour* b, bool absolute) : BehaviourState(o, b), absolute(absolute) {
  start = brain_hooks().now();  // [0x0060991c]
  o->owner->controls->stop();   // IControls vt+0x14
}

// Mech3.exe 1.2 @ 0x004ca070 (wait_interval: on when the end is not at or
// past now, a NaN included) and 0x004ca190 (wait_time: only once now is
// past it)
void WaitState::run(float dt) {
  (void)dt;
  const auto* st = static_cast<const ValueStep*>(from);
  const float now = brain_hooks().now();
  const bool on = absolute ? now > fbits(st->value) : !(start + fbits(st->value) >= now);
  if (on) order->set_behaviour(st->next);  // 0x004e8120: this state destroyed
}

// Mech3.exe 1.2 @ 0x004cb120 with 0x004cb250 (x87 at 24 bits: floats; the
// flat distance through the CRT's _hypot, here the root of the squares)
JumpPlan jump_plan(const float from[3], const float to[3], float up, float down, float margin, float speed) {
  JumpPlan p;
  const float d[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
  const float hyp = std::sqrt(d[0] * d[0] + d[2] * d[2]);  // the CRT's _hypot, at 24 bits
  p.dist = hyp;
  p.rise = d[1];
  if (d[1] < 0.0f || std::isnan(d[1])) {  // C0: below, or unordered
    p.rise = -d[1];
    p.climb = margin;
    p.fall = margin + p.rise;
  } else {
    p.climb = d[1] + margin;
    p.fall = margin;
  }
  p.ratio = up / down;
  p.t[2] = 0;
  p.k = 2.0f / ((down - up) * p.ratio);
  p.t[0] = std::sqrt(p.climb * p.k);
  p.t[1] = -(p.t[0] * p.ratio);
  p.t[4] = std::sqrt(p.fall * p.k);
  p.t[3] = -(p.ratio * p.t[4]);
  p.total = ((p.t[3] + p.t[4]) + p.t[1]) + p.t[0];
  p.need = p.total > 0.0f ? hyp / p.total : 3.4028234663852886e38f;  // 0x7f7fffff
  // 0x004cb250: no faster than needed; no glide without a speed.
  if (speed > p.need) {
    p.t[2] = 0, p.speed = p.need;
  } else if (!(speed > 0.0f)) {
    p.t[2] = 0, p.speed = speed;
  } else {
    p.t[2] = p.dist / speed - p.total;
    p.speed = speed;
  }
  return p;
}

// Mech3.exe 1.2 @ 0x004cb330. The jets' climb is 0x0041d490(1) (8 a unit
// of thrust, plus gravity) less gravity; the glide's jets 0x0041d4b0(gravity).
JumpState::JumpState(Order* o, Behaviour* step) : BehaviourState(o, step) {
  BrainHooks& h = brain_hooks();
  Brain& b = *o->owner;
  const zgame::Handle v = b.controls->vehicle();
  const float g = h.gravity();
  const Behaviour* next = static_cast<const Step*>(step)->next;
  const float top = h.speed_at(v, b.xec);  // vt+0x118
  const float up = (1.0f * 8.0f + g) - g;  // 0x005e1f8c
  const float* to = next->way_point();     // vt+0x1c
  float at[3];
  h.position(v, at);  // 0x005a85e0
  const JumpPlan p = jump_plan(at, to, up, -g, fbits(static_cast<const ValueStep*>(step)->value), top);
  jet = (g - g) / 8.0f;
  const float now = h.now();
  float t = now;
  for (int i = 0; i < 5; ++i) until[i] = t = t + p.t[i];
  speed = p.speed;
  phase = 0;
}

// Mech3.exe 1.2 @ 0x004cb4b0. [0x0060b488] (the vehicle's +0x564 set to 1)
// is never set: left out. Each phase ends on its time, or (but the first)
// when the vehicle is down.
void JumpState::run(float dt) {
  (void)dt;
  BrainHooks& h = brain_hooks();
  Brain& b = *order->owner;
  BrainControls& c = *b.controls;
  const zgame::Handle v = c.vehicle();
  Behaviour* next = static_cast<const Step*>(from)->next;
  // FIX (bug): a jump whose next step has no place (vt+0x18 null: not a node)
  // has the exe steer toward a null pointer (a crash); it does not turn.
  const float* to = next ? next->position() : nullptr;
  if (steering && to) {
    float at[3];
    h.position(v, at);  // 0x005a85e0
    FlatWay w;
    flat_way(at, to, &w);  // 0x004ce230, the next step's vt+0x18
    float fwd[3] = {0, 0, 0};
    h.forward(v, &fwd[0], &fwd[2]);  // +0x24
    const float dot = fwd[0] * w.dir[0] + fwd[2] * w.dir[2];
    float cross;
    turn = turn_hold(fwd, w.dir, &dot, &cross, turn, turn_params());  // 0x004c8df0 with [0x0060b478]
  } else {
    turn = 0;
  }
  c.set_turn(turn);
  // The forward thrust toward the speed (0x0060b430..0x0060b43c: 1, 1, 0.2, 1).
  const float diff = h.forward_speed(v) - speed;
  float k = (std::fabs(diff) - 1.0f) * 1.0f + 0.2f;
  if (k > 1.0f) k = 1.0f;
  if (diff > 0.0f) k = -k;
  c.set_jet_forward(k);
  auto before = [&](int i) { return !(h.now() >= until[i]); };  // C0: before, or unordered
  for (;;) {
    switch (phase) {
      case 0:
        h.set_local_velocity(v, 0, 0, 0);
        phase = 10;
        [[fallthrough]];
      case 10:
        if (before(0)) return c.set_jump(1.0f);
        phase = 1;
        continue;
      case 1:
        if (before(1) && h.airborne(v)) return c.set_jump(0.0f);
        phase = 2;
        continue;
      case 2:
        h.set_local_velocity(v, 0, 0, 0);
        phase = 11;
        [[fallthrough]];
      case 11:
        if (before(2) && h.airborne(v)) return c.set_jump(jet);
        phase = 3;
        continue;
      case 3:
        if (before(3) && h.airborne(v)) return c.set_jump(0.0f);
        phase = 4;
        continue;
      case 4:
        if (before(4) && h.airborne(v)) return c.set_jump(1.0f);
        phase = 5;
        steering = 0;
        continue;
      case 5:
        h.set_local_velocity(v, 0, -2.0f, 0);
        phase = 6;
        continue;
      case 6:
        if (h.airborne(v)) return c.set_jump(0.0f);
        c.set_jump(0.0f);
        order->set_behaviour(next);  // 0x004e8120: this state destroyed
        return;
      default:
        return;
    }
  }
}

// Mech3.exe 1.2 @ 0x004ccd00
McbWaitState::McbWaitState(Order* o, Behaviour* node) : BehaviourState(o, node) {
  o->owner->controls->stop();  // IControls vt+0x14
}

// Mech3.exe 1.2 @ 0x004ccda0. A link that is no behaviour (+0x60, +0x74)
// leaves the pilot waiting.
void McbWaitState::run(float dt) {
  (void)dt;
  const auto* n = static_cast<const McbWaitNode*>(from);
  Behaviour* l = nullptr;
  if (brain_hooks().now() > n->time) {  // past it ([0x0060991c]; a NaN waits on the valve)
    l = n->link(0);
  } else if ((n->valve ? n->valve->get() : 0) == static_cast<int32_t>(n->b)) {
    // FIX (bug): a record without a valve name has none, which the exe reads
    // (a crash); it reads 0, as the deploy node's.
    l = n->link(1);
  } else {
    return;
  }
  if (Behaviour* next = l ? l->as_behaviour() : nullptr) order->set_behaviour(next);  // 0x004e8120
}

// Mech3.exe 1.2 @ 0x004ccec0
void SoundState::run(float dt) {
  (void)dt;
  Brain& b = *order->owner;
  float at[3];
  brain_hooks().position(b.controls->vehicle(), at);  // 0x005a85e0
  brain_hooks().sound_at(static_cast<const SoundStep*>(from)->sound, at, 1.0f);
  b.x104 = 1;
  order->set_behaviour(static_cast<const Step*>(from)->next);  // 0x004e8120: this state destroyed
}

// Mech3.exe 1.2 @ 0x004cd690
void GateState::run(float dt) {
  (void)dt;
  order->owner->x104 = 1;
  order->set_behaviour(static_cast<const Step*>(from)->next);  // 0x004e8120: this state destroyed
}

BehaviourState* Behaviour::make_state(Order& o) {
  switch (kind) {
    case StepKind::Behaviour: return new DefaultState(&o, this);
    case StepKind::Null: return new NullStepState(&o, this);
    case StepKind::Stand: return new StandState(&o, this);
    case StepKind::Hold: return new HoldState(&o, this);
    case StepKind::Node: return new NodeState(&o, this);  // 0x004c9150
    case StepKind::BackupNode: return new BackupState(&o, this);  // 0x004cfa60
    case StepKind::DeployNode: return new DeployState(&o, this);  // 0x004d0020
    case StepKind::Speed: return new SpeedState(&o, this);  // 0x004c9ed0
    case StepKind::WaitInterval: return new WaitState(&o, this, false);  // 0x004c9fa0
    case StepKind::WaitTime: return new WaitState(&o, this, true);       // 0x004ca0c0
    case StepKind::McbWaitNode: return new McbWaitState(&o, this);      // 0x004ccce0
    case StepKind::Jump: return new JumpState(&o, this);                // 0x004cb310
    case StepKind::Sound: return new SoundState(&o, this);              // 0x004cce50
    case StepKind::Gate: return new GateState(&o, this);                // 0x004cd560
    case StepKind::Valve: return new PassState(&o, this);   // 0x004ca1f0
    case StepKind::Shunt: return new PassState(&o, this);   // 0x004cd700 -> 0x004cd720 (vtable 0x005cc270)
    case StepKind::ValveUnion: return new PassState(&o, this);  // 0x004cdb60 -> 0x004cdbd0 (vtable 0x005cc420)
    case StepKind::ValveAssign: return new AssignState(&o, this);  // 0x004cdc80
    case StepKind::Scan:
    case StepKind::CanLeave:
    case StepKind::CanLeaveNot: return new ScanState(&o, this);  // 0x004cbc30
    case StepKind::VehAttack: return new FightState(&o, this);                                    // 0x004d1ca0
    case StepKind::VehOwn: return make_station_state(*this, o);  // 0x004cc150
    case StepKind::VehFollow: return new FollowState(&o, this);  // 0x004c89f0 -> 0x004d1870
    case StepKind::VehMode1: return new CircleState(&o, this);  // 0x004c9730
    case StepKind::VehMode2: return new OrbitState(&o, this);    // 0x004d1300
    case StepKind::VehMode4: return new VeerState(&o, this);     // 0x004d1630
    case StepKind::Manoeuvre: return new ManoeuvreState(&o, this, false);   // 0x004caac0
    case StepKind::Manoeuvre2: return new ManoeuvreState(&o, this, true);   // 0x004cabc0
    case StepKind::AfterManoeuvre: return new AfterManoeuvreState(&o, this);  // 0x004ca390
    case StepKind::AttackEnd: return new AttackEndState(&o, static_cast<AttackEnd*>(this));        // 0x004cac00
    case StepKind::AttackRearm: return new AttackRearmState(&o, this);                            // 0x004cacc0
    case StepKind::AttackReturn: return new AttackReturnState(&o, static_cast<AttackReturn*>(this));  // 0x004cad90
    default: return new BehaviourState(&o, this);
  }
}

// Mech3.exe 1.2 @ 0x004c8690
void BehaviourState::run(float dt) {
  (void)dt;
  stop_and_hold(*order->owner);
}

Behaviour* shared_behaviour() {
  static SharedBehaviour b;
  return &b;
}
Behaviour* stand_behaviour() {
  static Behaviour b(StepKind::Stand);
  return &b;
}
Behaviour* hold_behaviour() {
  static Behaviour b(StepKind::Hold);
  return &b;
}

// Mech3.exe 1.2 @ 0x004cbe50
SharedState::SharedState(Order* o) : BehaviourState(o, shared_behaviour()) {}
Behaviour* SharedState::behaviour() const { return shared_behaviour(); }

// Mech3.exe 1.2 @ 0x004cbeb0: stopped; a lancemate the player commands
// (+0xbc bit 4) forms up on the formation net when the player's lance state
// is 4, else follows the player's vehicle's own net; any other pilot goes on
// to the step of its net nearest it, the place kept (brain +0x298, the
// order's +0x24).
void SharedState::run(float dt) {
  (void)dt;
  Order* o = order;
  Brain& b = *o->owner;
  BrainHooks& h = brain_hooks();
  const zgame::Handle v = b.controls->vehicle();
  stop_and_hold(b);
  if (!v) return;
  if (h.vehicle_flags(v) & 0x10) {
    if (h.player_lance_state() == 4) {
      if (AiNet* f = h.formation_net()) b.push_net(f);  // 0x004e86a0; the port has no 0x00603fb8 yet
      return;
    }
    const zgame::Handle pv = h.player_vehicle();
    if (!pv) return;
    AiNet* vn = h.vehicle_net(pv);
    if (!vn) return;
    b.push(vn, h.follow_behaviour(vn));  // 0x004e87c0
    return;
  }
  if (!o->net) return;
  AiNet* n = o->net->net();
  if (!n) return;
  float p[3];
  h.position(v, p);
  Behaviour* step = h.nearest(n, p);
  for (int i = 0; i < 3; ++i) b.x298[i] = p[i], o->x24[i] = p[i];
  b.push_behaviour(step);  // 0x004e88c0
}

// Mech3.exe 1.2 @ 0x004ce620
Steer::Steer() : x38(cos_quarter_pi()) {}
static_assert(sizeof(Steer) == 0x48, "the brain's +0x250 block");

namespace {
// [0x0060b494] = cos(3 pi / 4), from the start-up's 0x004ce610 (the double at
// 0x005cc548); [0x0060b4c0] beside it is never written: 0.
float cos_three_quarter_pi() { return static_cast<float>(std::cos(2.3561944901919)); }
constexpr float kKeepNear = 30.0f;  // 0x005e8bd0 / 0x005e8bc8
constexpr float kBand = 10.0f;      // 0x005e8bd4
}  // namespace

// Mech3.exe 1.2 @ 0x004ceb00: p from the carrot, along the segment and to
// its side.
void Steer::project(const float p[3], float out[2]) const {
  const float dx = p[0] - start[0], dz = p[2] - start[2];
  out[0] = dx * dir[0] + dz * dir[1];
  out[1] = dx * dir[1] - dz * dir[0];
  out[0] = out[0] - along_at;
  out[1] = out[1] - across;
}

// Mech3.exe 1.2 @ 0x004ce6f0
void Steer::carrot(float out[3]) const {
  out[0] = dir[0] * along_at + start[0];
  out[1] = 0 + start[1];
  out[2] = dir[1] * along_at + start[2];
  if (across < 0 || across > 0) {  // the x87's C3: 0 or a NaN adds nothing
    out[0] = across * dir[1] + out[0];
    out[2] = out[2] - across * dir[0];
  }
}

// The distance from the segment 0x004ce6a0 and 0x004ceb50 measure: to the
// side, plus how far before its start or past its end.
float Steer::distance_off() const {
  float m = std::fabs(side);
  if (!(along >= 0)) m = m - along;  // the x87's unordered compares as written
  else if (along > length) m = (along - length) + m;
  return m;
}

// Mech3.exe 1.2 @ 0x004ce6a0
bool Steer::near() const { return !(distance_off() >= near_dist); }

// Mech3.exe 1.2 @ 0x004ce780 / 0x004ce7d0 / 0x004ce9f0 / 0x004ce860 over the
// vehicle's contacts, nearest first, while within (30 + along)^2 + 10^2: a
// contact ahead of the vehicle, behind the carrot and within 10 of its line
// pushes the carrot ahead by the room left, and the more head-on it faces
// (its forward against the segment's back, cos 135 at full), the more of
// that to the side away from it.
void Steer::push_by_contacts(zgame::Handle owner, float dt) {
  (void)dt;  // the context keeps it; nothing reads it
  BrainHooks& h = brain_hooks();
  const std::vector<Contact*>* cs = h.contacts(owner);
  if (!cs) return;
  const float a = along > 0 ? kKeepNear + along : kKeepNear;
  const float limit = kBand * kBand + a * a;
  for (const Contact* c : *cs) {
    if (c->dist2 > limit) break;  // 0x004ce7d0 (a NaN distance is taken)
    const zgame::Handle e = h.contact_entity(c);
    if (!e) continue;
    float p[3], l[2];
    h.position(e, p);
    project(p, l);
    if (l[0] >= along || -kKeepNear >= l[0]) continue;
    const float d = l[1] - side;
    if (std::fabs(d) >= kBand) continue;
    const float room = (kKeepNear + l[0]) - std::fabs(d);
    if (!(room > 0)) continue;
    float da = room, ds = 0;
    float fx, fz;
    h.forward(e, &fx, &fz);
    const float dot = -dir[0] * fx + -dir[1] * fz;
    float f = (dot - 0.0f) / (cos_three_quarter_pi() - 0.0f);
    if (!(0 >= f)) {
      if (f > 1.0f) f = 1.0f;
      const float w = f * room;
      da = da - w;
      ds = ds - static_cast<float>(std::copysign(static_cast<double>(w), static_cast<double>(d)));
    }
    shift(da, ds);  // 0x004ce820
  }
}

// Mech3.exe 1.2 @ 0x004ceb50: the vehicle measured; the carrot kept within
// 30 of it (less its distance to the side); near the segment, the contacts.
void Steer::measure(zgame::Handle owner, float dt) {
  BrainHooks& h = brain_hooks();
  float p[3];
  h.position(owner, p);
  float l[2];
  project(p, l);
  along = l[0], side = l[1];
  float room = kKeepNear - std::fabs(side);
  if (!(room >= 0)) room = 0;
  const float over = std::fabs(along) - room;
  if (over > 0) advance(static_cast<float>(std::copysign(static_cast<double>(over), static_cast<double>(along))));
  if (near()) push_by_contacts(owner, dt);
}

// Mech3.exe 1.2 @ 0x004cec80: the side offset eases to its target at 2 a
// second (snapped within 0.05), the carrot falls back at its rate down to
// along_min, then the measure.
void Steer::update(float dt, zgame::Handle owner) {
  const float to = across_to;
  const float n = (to - across) * 2.0f * dt + across;
  across = n;
  if (!(std::fabs(to - n) >= 0.05)) across = to;
  const float a = along_at - rate * dt;
  along_at = a;
  if (!(a >= along_min)) along_at = along_min;
  measure(owner, dt);
}

// Mech3.exe 1.2 @ 0x004cea30 (with 0x004ce530 for the direction).
void Steer::set(const float to[3], const float from[3], float r, zgame::Handle owner) {
  dir[0] = to[0] - from[0];
  dir[1] = to[2] - from[2];
  const float len = std::sqrt(dir[1] * dir[1] + dir[0] * dir[0]);
  if (len != 0) {
    dir[0] = (1.0f / len) * dir[0];
    dir[1] = (1.0f / len) * dir[1];
  } else {
    dir[0] = dir[0] * 0.0f;
    dir[1] = dir[1] * 0.0f;
  }
  along_at = length = len;
  along_min = 0, across = 0, across_to = 0;
  rate = r;
  start[0] = from[0], start[1] = from[1], start[2] = from[2];
  BrainHooks& h = brain_hooks();
  float p[3], l[2];
  h.position(owner, p);
  project(p, l);
  along = l[0], side = l[1];
  push_by_contacts(owner, 0.0f);
}

// Mech3.exe 1.2 @ 0x004e7c40, each sound through 0x004e7dc0.
const char* const Radio::kEvents[16] = {
    "call_help", "last_shot", "confirm_order", "confirm_refit", "disregard_order", "disengages_order",
    "ejecting",  "engaging",  "friendly_fire", "low_ammo",      "out_of_ammo",     "nuke",
    "follow",    "stop",      "repairs",       "objective"};

void Radio::load(const char* voice) {
  for (int i = 0; i < 16; ++i) {
    // 0x004e7dc0: sprintf into 32 bytes. FIX (bug): cut to fit (the exe's
    // overflows its frame for a voice of 15 or more characters; the shipped
    // ones are 3 to 5).
    char name[32];
    std::snprintf(name, sizeof name, "%s_%s", voice, kEvents[i]);
    sounds[i] = brain_hooks().sound_find(name);
  }
}

// Mech3.exe 1.2 @ 0x004c9d00 over 0x004c8600, then 0x004cd770.
BrainStep::BrainStep() : next(null_step()) {}

// Mech3.exe 1.2 @ 0x004cf330 (most derived 0: the AI controls' base). The
// net given is the vehicle's (player.cpp passes it); without one the brain
// asks znet for the record's first item's net, else takes the built-in one.
Brain::Brain(AiNet* net, const AiParams& rec) : params(rec) {
  if (!net) net = brain_net_for(rec);
  // +0x190: the order stack's owner (the brain).
  set_net(net);
  order.set_owner(this);  // 0x004e8090
  order.own = true;       // its vtable 0x005cd8e8
  firing.brain = this;    // 0x004b6660 (its IControls vt+0x10(0): nothing)
  make_radio();
}

AiNet* brain_net_for(const AiParams& rec) {
  BrainHooks& h = brain_hooks();
  AiNet* found = nullptr;
  if (znet::Object* o = h.net_find(rec.kind()))
    if (AiNet* n = dynamic_cast<AiNet*>(o)) found = n->as_net() ? n->as_net()->as_compiled() : nullptr;
  return found ? found : h.builtin_net();
}

// Mech3.exe 1.2 @ 0x004cf630 (from the controls' destructors). The objects
// at +0x23c and +0x238 (the way finder) go through their slot 0 with 1.
Brain::~Brain() {
  BrainHooks& h = brain_hooks();
  if (radio) {  // 0x004e7df0 (0x004e7d80 clears it)
    h.on_delete(radio);
    delete radio;
    radio = nullptr;
  }
  // The orders, first to last (each in place), the block, then the map.
  while (!orders.empty()) {
    orders.pop_front();
    if (orders.empty() && order_block) {
      h.on_delete(order_block);
      h.on_delete(order_map);
      order_block = order_map = nullptr;
    }
  }
}

// Mech3.exe 1.2 @ 0x004e9d90 (from 0x004e9d80): a new last order, a default
// one (0x004e8070) with the template's (0x0060b898, or the last order's)
// +0x14..+0x2f copied and the brain its owner. An empty deque takes a block
// (`new(0xf70)`) and a map of two (`new(8)`): the order goes in the
// block's middle.
void Brain::push_order() {
  BrainHooks& h = brain_hooks();
  const bool empty = orders.empty();
  Order from_template;  // 0x0060b898 (0x004e8030): zeros
  const Order& src = empty ? from_template : orders.back();
  int32_t x14 = src.x14;
  float x18[3] = {src.x18[0], src.x18[1], src.x18[2]};
  float x24[3] = {src.x24[0], src.x24[1], src.x24[2]};
  if (empty) {
    h.on_new(0xf70, &orders);
    h.on_new(8, &order_block);
    order_block = &orders;
    order_map = &order_block;
  }
  Order& o = orders.emplace_back();
  o.set_owner(this);
  o.x14 = x14;
  for (int i = 0; i < 3; ++i) o.x18[i] = x18[i], o.x24[i] = x24[i];
}

// The pop_back in 0x004e9380: the order destroyed in place; when the deque
// empties its block and map go (0x004ea7f0).
void Brain::pop_order() {
  BrainHooks& h = brain_hooks();
  orders.pop_back();
  if (orders.empty() && order_block) {
    h.on_delete(order_block);
    h.on_delete(order_map);
    order_block = order_map = nullptr;
  }
}

// The pop of 0x004e8380, 0x004e84a0, 0x004e8990 and 0x004e9100: then the
// new current order's state resumes. Emptied, the exe still resumes
// "the current state" through a null block (a crash) in all but 0x004e9100;
// no caller gets there (ai.md 17.2), and here nothing is resumed.
void Brain::pop_resume() {
  pop_order();
  if (!orders.empty()) current().state->resume();
}

// Mech3.exe 1.2 @ 0x004e9380 (IControls vt+0x04 of the AI's controls).
bool Brain::set_net(AiNet* net) {
  AiNet* compiled = net ? (net == brain_hooks().builtin_net() ? net : net->as_compiled()) : nullptr;
  if (!compiled) return false;
  while (!orders.empty()) pop_order();
  push_order();
  // The net's part into +0x30, the shared behaviour's state into +0x54.
  Order& o = current();
  o.net.reset(compiled->make_part(o));
  o.state.reset(shared_behaviour()->make_state(o));
  return true;
}

// Mech3.exe 1.2 @ 0x004e82e0
bool Brain::remake(AiNet* n) { return current().reset(n, nullptr); }

// Mech3.exe 1.2 @ 0x004e86a0: the current state suspended, a new order (the
// last one's +0x14..+0x2f copied) on the net with the default behaviour.
bool Brain::push_net(AiNet* n) {
  current().state->suspend();
  push_order();
  return current().reset(n, nullptr);
}

// Mech3.exe 1.2 @ 0x004e87c0: the same with a behaviour; the new order has
// no parts to destroy.
bool Brain::push(AiNet* n, Behaviour* b) {
  current().state->suspend();
  push_order();
  Order& o = current();
  if (!b) b = brain_hooks().default_behaviour();
  if (!n) n = brain_hooks().builtin_net();
  o.net.reset(n->make_part(o));
  o.state.reset(b->make_state(o));
  return true;
}

// Mech3.exe 1.2 @ 0x004e88c0
bool Brain::push_behaviour(Behaviour* b) { return push(current().net->net(), b); }

// Mech3.exe 1.2 @ 0x004e8380
void Brain::pop() { pop_resume(); }

// Mech3.exe 1.2 @ 0x004e84a0
bool Brain::pop_if(AiNet* n) {
  if (!n || current().net->net() != n) return false;
  pop_resume();
  return true;
}

// Mech3.exe 1.2 @ 0x004e8c60: from the current order down, the first not
// marked. All marked, the exe answers the slot before the first (no order);
// here none, which 0x004e8990 treats the same.
Order* Brain::top_unmarked() {
  for (auto it = orders.rbegin(); it != orders.rend(); ++it)
    if (!it->x04) return &*it;
  return nullptr;
}

// Mech3.exe 1.2 @ 0x004e8990: pops down to `o` and it, but never the last
// order left (o the bottom one: all above it go). No order (none matches)
// pops down to one; the exe given 0 with one order left pops it too (a
// crash, see pop_resume), which no caller does.
void Brain::unwind_to(Order* o) {
  const Order* seen = nullptr;
  while (orders.size() > 1) {
    seen = &current();
    if (seen == o) break;
    pop_resume();
  }
  if (seen && seen == o) pop_resume();
}

// Mech3.exe 1.2 @ 0x004ea4e0 (vt+0x40: the player's stop, likely). Every
// order marked, 0x004e8c60 answers the slot before the first, which matches
// none.
bool Brain::stop() {
  unwind_to(top_unmarked());
  return true;
}

// Mech3.exe 1.2 @ 0x004ea450 (and 0x004ea4a0): the vehicle the contact sees
// (0x004e6fe0), its own net (vt+0xd8): stopped (vt+0x40), its follow
// behaviour pushed on it (0x004c0de0, 0x004e87c0).
bool Brain::attack_target(Contact* c) {
  BrainHooks& h = brain_hooks();
  const zgame::Handle e = c ? h.contact_entity(c) : 0;
  if (!e) return false;
  AiNet* net = h.vehicle_net(e);
  if (!net) return false;
  stop();
  push(net, h.follow_behaviour(net));
  return true;
}

// Mech3.exe 1.2 @ 0x004ea4b0
bool Brain::form_on_me() {
  stop();
  AiNet* f = brain_hooks().formation_net();
  return f && push_net(f);  // 0x004e86a0 == 1
}

namespace {
// 0x004ea390 / 0x004ea3f0's test: the contact's vehicle (0x004e6fe0, vt+0x28)
// among the pilot's allies. FIX (bug): a contact that sees nothing is read
// through null in the exe; here neither order takes it.
int32_t ally_of(Brain& b, Contact* c) {
  BrainHooks& h = brain_hooks();
  const zgame::Handle e = c ? h.contact_entity(c) : 0;
  const AiTarget* mine = h.targetable(b.controls->vehicle());
  const AiTarget* theirs = e ? h.targetable(e) : nullptr;
  if (!mine || !theirs) return -1;
  return (mine->allies & theirs->team) ? 1 : 0;
}
}  // namespace

// Mech3.exe 1.2 @ 0x004ea390
bool Brain::enemy_only(Contact* c) { return ally_of(*this, c) == 0 && attack_target(c); }

// Mech3.exe 1.2 @ 0x004ea3f0
bool Brain::ally_only(Contact* c) { return ally_of(*this, c) == 1 && attack_target(c); }

// Mech3.exe 1.2 @ 0x004ea000..0x004ea230 with 0x004e9fd0 (the lines the
// callbacks 0x004ea030 / 0x004ea0a0 / 0x004ea0e0 / 0x004ea1b0 / 0x004ea1f0
// play: radio +0x30, +0x34, +0x08, +0x0c, +0x14).
int32_t Brain::lance_order(int32_t order, Contact* c) {
  int32_t r = 0, line = 2;  // confirm_order
  switch (order) {
    case 0: r = attack_target(c), line = 12; break;   // 0x004ea000 vt+0x20, follow
    case 1: r = form_on_me(), line = 13; break;       // 0x004ea070 vt+0x28, stop
    case 3: r = enemy_only(c); break;                 // 0x004ea0b0 vt+0x2c
    case 4: r = ally_only(c); break;                  // 0x004ea0f0 vt+0x30
    case 5: r = attack_target(c); break;              // 0x004ea120 vt+0x34
    case 6: r = 0; break;                             // 0x004ea150 vt+0x38 (0x0058b380)
    case 7:
    case 12: r = brain_hooks().refit(*this), line = 3; break;  // 0x004ea180 vt+0x3c, confirm_refit
    case 8: r = attack_target(c), line = 12; break;   // 0x004ea040 vt+0x24, follow
    case 9: r = stop(), line = 5; break;              // 0x004ea1c0 vt+0x40, disengages_order
    case 10: r = enemy_only(c); break;                // 0x004ea200 vt+0x44
    case 11: r = ally_only(c); break;                 // 0x004ea230 vt+0x48
    default: return 0;
  }
  if (radio) {  // 0x004e9fd0
    if (r == 0) brain_hooks().radio_line(*this, 4);  // disregard_order (+0x10)
    else if (r == 1) brain_hooks().radio_line(*this, line);
  }
  return r;
}

// Mech3.exe 1.2 @ 0x004e9070
bool Brain::may_fire() { return current().state->may_fire(); }

namespace {
AiNet* cast_net(znet::Object* o) {  // vt+0x34
  AiNet* n = o ? dynamic_cast<AiNet*>(o) : nullptr;
  return n ? n->as_compiled() : nullptr;
}
}  // namespace

// Mech3.exe 1.2 @ 0x004e8660 (IControls side).
bool Brain::pop_if_object(znet::Object* o) { return pop_if(cast_net(o)); }

// Mech3.exe 1.2 @ 0x004e8960 (IControls side).
bool Brain::push_object(znet::Object* o) {
  AiNet* n = cast_net(o);
  return n ? push_net(n) : false;
}

// Mech3.exe 1.2 @ 0x004e9100 (IControls side): when any order, first to
// last, is on the net, every order goes (each pop resumes the next current
// one while any is left). FIX (bug): the exe's search for a net no order is
// on steps past a full last block into the deque's map (a crash); here it
// stops at the last order (docs/fixes.md).
bool Brain::clear_if_object(znet::Object* o) {
  AiNet* n = cast_net(o);
  if (!n) return false;
  bool found = false;
  for (const Order& r : orders)
    if (r.net->net() == n) {
      found = true;
      break;
    }
  if (!found) return false;
  while (!orders.empty()) pop_resume();
  return true;
}

// Mech3.exe 1.2 @ 0x004e7e20: a radio (`new(0x40)`) for a pilot with a
// voice (aiv.zrd's sound_tag) on a team the player's vehicle counts as
// allies, or on any team when there is no player's vehicle yet.
void Brain::make_radio() {
  BrainHooks& h = brain_hooks();
  uint32_t team = static_cast<uint32_t>(params.team());
  uint32_t mask = 1u << (team & 0x1f);
  while ((team >>= 5) != 0) mask |= 1u << (team & 0x1f);
  if (radio) {
    h.on_delete(radio);
    delete radio;
  }
  radio = nullptr;
  const char* voice = params.sound_tag;
  if (!voice) return;
  uint32_t allies = 0;
  if (h.player_allies(&allies) && !(allies & mask)) return;
  radio = new Radio;
  h.on_new(0x40, radio);
  radio->load(voice);
}

// Mech3.exe 1.2 @ 0x004a3fc0 twice (from 0x004a4000).
// Mech3.exe 1.2 @ 0x004e71c0 (most derived: the IControls base at +0xcc4,
// owner +0xcd4), over 0x004e7090: the basic vehicle controls 0x004ac750
// (0x004ac740 on +0x764 -> 0x004a4af0: the turret limits of -pi/2 and pi/2),
// the brain on +0xa04, then +0xcb0, +0xcbc and +0x38.
AiVehicleControls::AiVehicleControls(void* o, AiNet* net, const AiParams& rec) {
  owner = o;
  brain = std::make_unique<Brain>(net, rec);
}

// Mech3.exe 1.2 @ 0x004e7870: the brain's destructor, then the basic
// controls' (0x004ac800), which asks the owner. FIX (bug): without an owner
// (a vehicle that failed to build, mission-start.md) the exe reads through
// null; here nothing is asked.
AiVehicleControls::~AiVehicleControls() {
  brain.reset();
  if (owner) brain_hooks().vehicle_controls_gone(owner);
}

// --- the gates (docs/ai.md 17.3) ---------------------------------------------

namespace {
// The checks 0x004e8f10 and 0x004e8dd0 share after their first tests: the
// hurt flag decided once a frame (+0xcc bit 2), then a leash from the home
// point (bit 0), the hurt flag, the attack's time (bit 1) and the health it
// gives up at (bit 3). The x87's unordered compares pass as written.
bool engage_checks(Brain& b, zgame::Handle owner) {
  BrainHooks& h = brain_hooks();
  const uint8_t flags = b.params.raw[0xcc];
  if (b.x22c == 2) b.x22c = (flags & 4) && !(h.health(owner) >= b.params.get<float>(0x50));
  if ((flags & 1) && b.xe4) {
    float p[3];
    h.position(owner, p);
    const float dx = p[0] - b.xe4[0], dy = p[1] - b.xe4[1], dz = p[2] - b.xe4[2];
    const float d2 = dx * dx + dy * dy + dz * dz;
    if (d2 > b.params.get<float>(0x24)) return false;
  }
  if (b.x22c) return false;
  if ((flags & 2) && h.now() >= b.xe8) return false;
  if ((flags & 8) && !(h.health(owner) >= b.x230)) return false;
  return true;
}
}  // namespace

// Mech3.exe 1.2 @ 0x004e8f10: engaging on (+0x3c), the net and the state
// letting the pilot break in, and the checks.
bool Brain::may_engage(zgame::Handle owner, AiNet* net, BehaviourState* st) {
  if (!params.get<int32_t>(0x3c)) return false;
  if (net && !net->allows_attack()) return false;
  if (st && !st->may_break_in()) return false;
  return engage_checks(*this, owner);
}

// Mech3.exe 1.2 @ 0x004e8dd0: the reverse, without the net and state: yes
// when engaging is off or a check fails (the states' break-off test).
bool Brain::must_break_off(zgame::Handle owner) {
  if (!params.get<int32_t>(0x3c)) return true;
  return !engage_checks(*this, owner);
}

// Mech3.exe 1.2 @ 0x004aec10. The x87's below: a NaN time starts a run.
void HitRecord::note(zgame::Handle attacker, float now) {
  if (by != attacker || !(last - -3.0f >= now)) run_start = now;  // 0x005c8ea8
  last = now;
  by = attacker;
}

// Mech3.exe 1.2 @ 0x004aec40: now at most last + secs (a NaN holds).
bool HitRecord::within(float secs, float now) const { return !(now > last + secs); }

// Mech3.exe 1.2 @ 0x004aec90
void controls_hit(HitRecord& r, zgame::Handle owner, zgame::Handle attacker) {
  BrainHooks& h = brain_hooks();
  r.note(attacker, h.now());
  if (owner == attacker || !attacker) return;
  const AiTarget* mine = h.targetable(owner);       // vt+0x28
  const AiTarget* theirs = h.targetable(attacker);
  if (mine && theirs && (theirs->allies & mine->team)) h.ally_hit(attacker, owner);
}

// Mech3.exe 1.2 @ 0x004ea520
void Brain::on_hit(HitRecord& r, zgame::Handle owner, zgame::Handle attacker) {
  BrainHooks& h = brain_hooks();
  controls_hit(r, owner, attacker);
  if (owner == attacker || !attacker) return;
  const AiTarget* mine = h.targetable(owner);
  const AiTarget* theirs = h.targetable(attacker);
  if (!mine || !theirs) return;
  if (!(mine->allies & theirs->team)) {
    const Contact* p = mine->player_contact;  // +0x98
    if (p && p->dist2 > 500.0f && radio) h.radio_line(*this, 0);  // 0x005e8e8c; call_help (radio +0x00)
    return;
  }
  if (h.vehicle_mech(attacker) != h.player_vehicle()) return;
  if (radio) h.radio_line(*this, 8);  // friendly_fire (radio +0x20)
  if (r.run() > 3.0f) h.allies_turn();  // 0x005cd918
}

// Mech3.exe 1.2 @ 0x004ea630 (the AI's IControls vt+0xb4, from the kill
// credit 0x0041b420): a kill of what is not an ally, last_shot.
void Brain::killed(zgame::Handle owner, zgame::Handle victim) {
  BrainHooks& h = brain_hooks();
  if (!radio) return;
  const AiTarget* mine = h.targetable(owner);
  const AiTarget* theirs = h.targetable(victim);  // vt+0x28
  if (mine && theirs && (mine->allies & theirs->team)) return;
  h.radio_line(*this, 1);  // last_shot (radio +0x04)
}

// Mech3.exe 1.2 @ 0x004e9640
bool Brain::p6_clear() const { return params.get<int32_t>(0x54) == 0; }

// Mech3.exe 1.2 @ 0x004e9650: always with +0x88 at 1, else whether the
// contact on the player (+0x8c set: owner +0x98) or on the nearest of his
// lance (owner +0x80) is within the sight range (+0x1c, squared).
bool Brain::sees_target(zgame::Handle owner) {
  if (params.get<int32_t>(0x88) == 1) return true;
  const AiTarget* t = brain_hooks().targetable(owner);
  const Contact* c = t ? (params.get<int32_t>(0x8c) ? t->player_contact : t->nearest_lance) : nullptr;
  return c && !(c->dist2 >= params.get<float>(0x1c));
}

// Mech3.exe 1.2 @ 0x004e96b0: +0xa4 picks the manoeuvre (0 the one at
// 0x00604098, 1 at 0x00604088, else none), pushed on the current net.
void Brain::manoeuvre() {
  const int32_t which = params.get<int32_t>(0xa4);
  if (which != 0 && which != 1) return;
  push_behaviour(brain_hooks().manoeuvre_behaviour(which));
  xf4 = 3.4028235e38f;
}

// --- the frame (docs/ai.md 17.1) ---------------------------------------------

namespace {
constexpr float kFltMax = 3.4028235e38f;
}  // namespace

// Mech3.exe 1.2 @ 0x004e9710
bool Brain::frame(BrainControls& c, float dt) {
  BrainHooks& h = brain_hooks();
  if (!h.ai_on()) {
    c.stop();
    c.smooth(dt);
    return true;
  }
  controls = &c;
  const zgame::Handle owner = c.vehicle();
  const bool down = h.powered_down(owner) == 1;
  steer.update(dt, owner);  // 0x004cec80
  if (!x2a4 && down) {
    c.stop();
    c.smooth(dt);
    return true;
  }
  bool answer = true;
  bool picked = false;
  Contact* enemy = nullptr;  // the nearest enemy, picked once
  const uint8_t flags = params.raw[0xcc];
  x22c = (flags >> 1) & 2;
  if (params.get<int32_t>(0x5c) && h.now() > xdc) {  // the delay before engaging (+0x5c)
    params.put<int32_t>(0x3c, 1);
    xdc = kFltMax;
  }
  AiTarget* self = h.targetable(owner);
  bool awake = true;
  if (params.get<int32_t>(0x88) == 0) {
    const Contact* t;
    if (params.get<int32_t>(0x8c)) {
      t = self ? self->player_contact : nullptr;
    } else {
      t = enemy = h.pick(owner, kEnemy);
      picked = true;
    }
    if (t) awake = !(t->dist2 >= params.get<float>(0x1c));  // the sight range; a NaN sees
  }
  if (!awake) {
    // Asleep: still steered while its attacker is within that one's range.
    const Contact* a = self ? self->player_contact : nullptr;
    const zgame::Handle v = a ? h.contact_vehicle(a) : 0;
    if (v && !(a->dist2 >= h.contact_range2(v))) {
      c.stop();
      c.smooth(dt);
    } else {
      answer = false;
    }
  } else {
    h.count_awake();
    float ammo;
    if (radio && h.ammo_value(owner, &ammo)) {
      if (!(ammo >= 0.1f)) h.radio_line(*this, 10);        // out_of_ammo (radio +0x28)
      else if (!(ammo >= 0.25f)) h.radio_line(*this, 9);   // low_ammo (+0x24)
    }
    vlist(c, owner, &picked, &enemy);
    // Hit lately: the firing starts again and the pilot may manoeuvre (+0x9c).
    if (xf0 == 0 && params.get<int32_t>(0x9c) && h.now() > x100 && c.hit_lately() && may_fire()) {
      x100 = params.get<float>(0xb0) + h.now();
      h.firing_reset(*this);
      manoeuvre();
    }
    bool attacking = false;
    AiNet* net = current().net->net();
    if (may_engage(owner, net, current().state.get())) {
      // Without a selected target the nearest enemy, but only when none was
      // picked yet this frame: one the sight check picked is not used
      // (as written; docs/fixes.md candidates).
      const Contact* t = self ? self->target : nullptr;
      if (!t && !picked) {
        picked = true;
        t = enemy = h.pick(owner, kEnemy);
      }
      const zgame::Handle v = t ? h.contact_vehicle(t) : 0;
      const float d2 = v ? t->dist2 : kFltMax;
      // FIX (bug): no vehicle with a range past FLT_MAX (a NaN) calls through
      // null in the exe; here no attack.
      if (!(d2 >= params.get<float>(0x20)) && v) {
        attacking = true;
        if (AiNet* vn = h.vehicle_net(v))
          if (Behaviour* beh = h.attack_behaviour(vn))
            if (h.attack(*this, net)) {
              x230 = h.health(owner) - params.get<float>(0xb4);
              xe8 = params.get<float>(0x4c) * h.intensity() + h.now();
              push(vn, beh);
            }
      }
    }
    bool due = false;
    if (!attacking && xf0 == 0 && params.get<int32_t>(0xa0) && may_fire()) {
      if (xf4 == kFltMax) {  // a random wait between +0x64 and +0x68
        const float lo = params.get<float>(0x64), hi = params.get<float>(0x68), now = h.now();
        const float r = static_cast<float>(h.rand());
        xf4 = (hi - lo) * 3.051851e-05f * r + lo + now;
      } else if (h.now() > xf4) {
        due = true;
        manoeuvre();
      }
    }
    if (attacking || due) xf4 = kFltMax;
    do {
      x104 = 0;
      current().run(dt);
    } while (x104);
    if (!down && params.get<int32_t>(0x80) == 1) h.own_order_run(*this, dt);  // [0x005e9d40] is 1
    c.smooth(dt);
    if (!down) h.firing_run(*this, dt);
  }
  if (x22c != 2) params.raw[0xcc] = static_cast<uint8_t>((params.raw[0xcc] & ~4u) | (x22c ? 4u : 0u));
  return answer;
}

// --- the vlist triggers (docs/ai.md 17.5) ------------------------------------

namespace {
// Mech3.exe 1.2 @ 0x004d8e70: the conditions in order, the first that holds
// wins (flags +0x18: bit 3 hit lately, 5 the heat at +0x08 or more, 4 the health
// at most +0x04, 2 the ammo's value at most +0x00; then the nearest enemy,
// picked once: bit 7 nearer than sqrt(+0x10), bit 6 nearer than sqrt(+0x0c)
// with a clear line; failing those, the region +0x14 by +0x19: bit 0 inside,
// bit 1 outside, both either). The x87's unordered compares hold.
bool vlist_holds(const VListEntry& e, BrainControls& c, zgame::Handle owner, bool* picked, Contact** target) {
  BrainHooks& h = brain_hooks();
  const uint8_t f = e.flags;
  if ((f & 8) && c.hit_lately()) return true;
  if ((f & 0x20) && h.heat(owner) >= e.f08) return true;
  if ((f & 0x10) && !(h.health(owner) > e.f04)) return true;
  if (f & 4) {
    float a;
    if (!h.ammo_value(owner, &a)) a = 1.0f;  // IControls vt+0x9c (0x004aed10): 1 without a manager
    if (!(a > e.f00)) return true;
  }
  if (!*picked) {
    *picked = true;
    *target = h.pick(owner, kEnemy);
  }
  const Contact* t = *target;
  if (t) {
    if ((f & 0x80) && !(t->dist2 >= e.f10)) return true;
    if ((f & 0x40) && !(t->dist2 >= e.f0c) && h.clear_line(owner, t->seen)) return true;
  }
  const uint8_t f2 = e.flags2;
  if (!e.region || !(f2 & 3)) return false;
  if ((f2 & 3) == 3) return true;
  float p[3];
  h.position(owner, p);
  return h.region_inside(e.region, p) ? (f2 & 1) : (f2 >> 1 & 1);
}

// Mech3.exe 1.2 @ 0x004d8e70: a trigger whose valve is not yet at its value
// (+0x18 bit 1) sets it there when its conditions hold.
void vlist_entry(const VListEntry& e, BrainControls& c, zgame::Handle owner, bool* picked, Contact** target) {
  if (!e.valve) return;
  const int32_t want = e.flags >> 1 & 1;
  if ((e.valve->get() != 0) == (want != 0)) return;
  if (vlist_holds(e, c, owner, picked, target)) e.valve->set(want);
}
}  // namespace

// Mech3.exe 1.2 @ 0x004d9290
void vlist_run(const AiParams& p, BrainControls& c, zgame::Handle owner, bool* picked, Contact** target) {
  if (!p.vlist) return;
  for (const VListEntry& e : *p.vlist) vlist_entry(e, c, owner, picked, target);
}

// Mech3.exe 1.2 @ 0x004e9550
void Brain::vlist(BrainControls& c, zgame::Handle owner, bool* picked, Contact** target) {
  vlist_run(params, c, owner, picked, target);
}

// Mech3.exe 1.2 @ 0x004e94f0
void Brain::pilot_tick(BrainControls& c) {
  bool picked = false;
  Contact* target = nullptr;
  vlist(c, c.vehicle(), &picked, &target);
}

// Mech3.exe 1.2 @ 0x004d9c20..0x004d9ca0: scan_aim (+0x98), scan_hit
// (+0x9c), scan_time (+0xa0) on or off; canleave (+0x3c, may engage) on or
// off with the engage delay's end (+0xdc) cleared.
void Brain::scan_test(ScanTest t) {
  switch (t) {
    case ScanTest::AimOn: return params.put<int32_t>(0x98, 1);
    case ScanTest::AimOff: return params.put<int32_t>(0x98, 0);
    case ScanTest::HitOn: return params.put<int32_t>(0x9c, 1);
    case ScanTest::HitOff: return params.put<int32_t>(0x9c, 0);
    case ScanTest::TimeOn: return params.put<int32_t>(0xa0, 1);
    case ScanTest::TimeOff: return params.put<int32_t>(0xa0, 0);
    case ScanTest::CanLeave:
    case ScanTest::CanLeaveNot:
      params.put<int32_t>(0x3c, t == ScanTest::CanLeave ? 1 : 0);
      xdc = 3.4028235e38f;
      return;
  }
}

// Mech3.exe 1.2 @ 0x004d8930 (through the IControls thunk 0x004d8cf0)
bool Brain::scan_aim() {
  const bool attacking = xf0 != 0;
  if (attacking || !params.get<int32_t>(0x98)) return attacking;
  const float now = brain_hooks().now();
  if (!(now > xfc)) return false;
  xfc = params.get<float>(0xa8) + now;
  manoeuvre();  // 0x004e96b0
  return true;
}

}  // namespace mech3::ai
