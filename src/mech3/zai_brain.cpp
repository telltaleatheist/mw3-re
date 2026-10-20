#include "mech3/zai_brain.h"

#include <cmath>
#include <cstdio>

#include "gamez/znetwork/znet_object.h"
#include "gamez/zsound/zsnd.h"
#include "mech3/zai_steps.h"

#pragma STDC FP_CONTRACT OFF

namespace mech3::ai {

namespace {
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

// Mech3.exe 1.2 @ 0x004e7ff0: the parts built in place go with their
// order (their slot 0 with 0: no free).
Order::~Order() {
  state = nullptr;
  net = nullptr;
  owner = nullptr;
}

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
  // Not empty: the new last order's state vt+0x10 (resume, likely); the
  // shared behaviour's does nothing (0x0049bfd0) and no other order is made
  // here.
}

// Mech3.exe 1.2 @ 0x004e9380 (IControls vt+0x04 of the AI's controls).
bool Brain::set_net(AiNet* net) {
  AiNet* compiled = net ? (net == brain_hooks().builtin_net() ? net : net->as_compiled()) : nullptr;
  if (!compiled) return false;
  while (!orders.empty()) pop_order();
  push_order();
  Order& o = orders.back();
  // The net's vt+0x5c (0x004c0070 / 0x004c0520) into +0x30, the shared
  // behaviour's vt+0x8c (0x004cbe30) into +0x54.
  o.net_part.builtin = compiled == brain_hooks().builtin_net();
  o.net_part.order = &o;
  o.net_part.net = compiled;
  o.net = &o.net_part;
  o.own_net = true;
  o.state_part.order = &o;
  o.state = &o.state_part;
  o.own_state = true;
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

}  // namespace mech3::ai
