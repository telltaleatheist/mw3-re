#include "mech3/zai_brain.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "crt/msvc_rand.h"
#include "gamez/znetwork/znet_object.h"
#include "gamez/zsound/zsnd.h"
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

// 0x005e8868: the firing state's first 11 words.
constexpr uint32_t kFiringDefaults[11] = {8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// [0x0060b4a0] = cos(pi/4), computed by the start-up code 0x004ce5b0 from the
// double at 0x005cc530.
float cos_quarter_pi() { return static_cast<float>(std::cos(0.78539816339744828)); }
}  // namespace

BrainHooks& brain_hooks() { return *g_hooks; }
void set_brain_hooks(BrainHooks* h) { g_hooks = h ? h : &g_default_hooks; }

znet::Object* BrainHooks::net_find(int32_t id) { return znet::find(id); }
void* BrainHooks::sound_find(const char* name) { return zsnd::find(name); }
Behaviour* BrainHooks::default_behaviour() { return mech3::ai::default_behaviour(); }
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
void BrainHooks::steer(Brain& b, zgame::Handle v, float dt) { unported("0x004cec80", {low(&b), v, bits(dt)}); }
void BrainHooks::radio_line(Brain& b, int32_t event) { unported("0x004e6d50", {low(&b), event}); }
float BrainHooks::heat(zgame::Handle v) { return fbits(unported("vehicle.vt+0x9c", {v})); }
bool BrainHooks::clear_line(zgame::Handle v, zgame::Handle seen) { return unported("0x004d5980", {v, seen, 0u}) & 0xff; }
bool BrainHooks::region_inside(Region* r, const float* p) { return r->inside(p); }
void BrainHooks::firing_reset(Brain& b) { unported("0x004e9590", {low(&b)}); }
void BrainHooks::firing_run(Brain& b, float dt) { unported("0x004e9fb0", {low(&b), bits(dt)}); }
AiNet* BrainHooks::vehicle_net(zgame::Handle v) {
  unported("vehicle.vt+0xd8", {v});
  return nullptr;
}
Behaviour* BrainHooks::attack_behaviour(AiNet* n) {
  unported("0x004c0e10", {low(n)});
  return nullptr;
}
bool BrainHooks::attack(Brain& b, AiNet* n) { return unported("0x004e9ca0", {low(&b), low(n)}) != 0; }
void BrainHooks::own_order_run(Brain& b, float dt) { unported("0x004e81a0", {low(&b), bits(dt)}); }

Behaviour* BrainHooks::manoeuvre_behaviour(int32_t which) {
  // 0x00410650 / 0x00410680 at start-up; their state (0x004caae0, item 11)
  // is not ported: the base one for now.
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

// Mech3.exe 1.2 @ 0x004e8170
void Order::pop() { owner->pop(); }

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

namespace {
// The default behaviour's state (0x004c8770 -> 0x004c8790, vtable
// 0x005cb330). Its run 0x004c87f0 is not ported.
class DefaultState : public BehaviourState {
 public:
  using BehaviourState::BehaviourState;
  bool may_break_in() const override { return false; }  // 0x004d72a0
  bool may_fire() const override { return false; }      // 0x004d7290
  void run(float dt) override { brain_hooks().unported("0x004c87f0", {BrainHooks::low(order), bits(dt)}); }
};

// 0x004cbe10 (vtable 0x005cbe18), made at start-up into 0x0060b460.
struct SharedBehaviour : Behaviour {
  SharedBehaviour() : Behaviour(StepKind::Shared) {}
  BehaviourState* make_state(Order& o) override { return new SharedState(&o); }  // 0x004cbe30
};
}  // namespace

// Mech3.exe 1.2 @ 0x004c8620 (the base, the step base) and 0x004c8770 (the
// default behaviour). The other behaviours' own states come with their ports
// (docs/ai.md 17.9); until then they get the base's.
BehaviourState* Behaviour::make_state(Order& o) {
  if (kind == StepKind::Behaviour) return new DefaultState(&o, this);
  return new BehaviourState(&o, this);
}

// Mech3.exe 1.2 @ 0x004c8690 (not ported: the controls' stop and IControls
// vt+0x10(0)).
void BehaviourState::run(float dt) { brain_hooks().unported("0x004c8690", {BrainHooks::low(order), bits(dt)}); }

Behaviour* shared_behaviour() {
  static SharedBehaviour b;
  return &b;
}

// Mech3.exe 1.2 @ 0x004cbe50
SharedState::SharedState(Order* o) : BehaviourState(o, shared_behaviour()) {}
void SharedState::run(float dt) { brain_hooks().unported("0x004cbeb0", {BrainHooks::low(order), bits(dt)}); }

// Mech3.exe 1.2 @ 0x004b6570, with the fire plan 0x004b6390 and the aim
// 0x004b7060 (`new(0x24)`).
Firing::Firing() {
  for (int i = 0; i < 11; ++i) x00[i] = kFiringDefaults[i];
  aim = new Aim;
  brain_hooks().on_new(0x24, aim);
}

// Mech3.exe 1.2 @ 0x004b65f0. The brain's IControls vt+0x10(0) it calls
// first is 0x004a5bc0 by then (does nothing); +0x14 is freed when it is not
// +0x30, then +0x30, +0x50 and the aim. Nothing here ever sets +0x14, +0x30
// or +0x50 (the firing code, not ported, would), so only the aim goes.
Firing::~Firing() {
  if (aim) {
    brain_hooks().on_delete(aim);
    delete aim;
    aim = nullptr;
  }
}

// Mech3.exe 1.2 @ 0x004ce620
Steer::Steer() : x38(cos_quarter_pi()) {}

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
// at +0x23c and +0x238 (deleted through their slot 0 with 1) are never set
// here.
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
TurretLimits::TurretLimits(float lo, float hi)
    : lo_sin(static_cast<float>(std::sin(static_cast<double>(lo)))),
      lo_cos(static_cast<float>(std::cos(static_cast<double>(lo)))),
      hi_sin(static_cast<float>(std::sin(static_cast<double>(hi)))),
      hi_cos(static_cast<float>(std::cos(static_cast<double>(hi)))) {}

// Mech3.exe 1.2 @ 0x004e71c0 (most derived: the IControls base at +0xcc4,
// owner +0xcd4), over 0x004e7090: the basic vehicle controls 0x004ac750
// (0x004ac740 on +0x764 -> 0x004a4af0: the turret limits of -pi/2 and pi/2),
// the brain on +0xa04, then +0xcb0, +0xcbc and +0x38.
AiVehicleControls::AiVehicleControls(void* o, AiNet* net, const AiParams& rec)
    : turret(-1.57079637f, 1.57079637f), owner(o) {
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
  const zgame::Handle owner = c.vehicle();
  const bool down = h.powered_down(owner) == 1;
  h.steer(*this, owner, dt);
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

}  // namespace mech3::ai
