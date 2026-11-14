#include "mech3/zai_wire_states_pm.h"
#include "mech3/zai_wire_orders_pm.h"

#include <cstring>

#include "mech3/zai_brain_pm.h"
#include "mech3/zai_steps.h"
#include "mech3/zai_valve.h"
#include "mech3/zai_target.h"
#include "mech3/zai_wire_attack_pm.h"
#include "mech3/zai_wire_firing_pm.h"
#include "mech3/zai_wire_nets_pm.h"
#include "mech3/zai_wire_pm.h"

namespace mech3::ai {

namespace {
bool g_making = false;  // inside PM's make_state (0x004f9e70's vt+0x8c)
// The brain's MW3 orders as PM's functions see them, and what their hooks do back on the brain. A new order's parts
// come in pairs (make_net, then make_state), so the net waits for its state, and Brain::push makes the order: it
// suspends the state under it as PM's stop (vt+0x0c) does, so `stop_state` has nothing left to do.
struct OnBrain : OrdersPmHooks {
  explicit OnBrain(Brain& b) : b(b) {
    for (const Order& o : b.orders) {
      OrderPm m;
      m.net = o.net ? pm_id(o.net->net()) : 0;
      m.state = pm_id(o.state.get());
      mirror.orders.push_back(m);  // ready stays 1: MW3's orders have no byte for it
    }
    for (size_t i = pm_pending_count(b); i; --i) mirror.pending.push_back(0);
  }
  bool may_fire(zgame::Handle s) override { return static_cast<BehaviourState*>(pm_ptr(s))->may_fire(); }
  // vt+0x14 on the order's net part: 1 for a compiled net's link (vtable 0x5ecd8c) and a vehicle net's part (0x5ed538),
  // both 0x4eb000; 0 for the built-in net's (0x5ed5b0, 0x4d5020).
  bool net_idle(zgame::Handle n) override { return !dynamic_cast<BuiltinNet*>(static_cast<AiNet*>(pm_ptr(n))); }
  bool state_idle(zgame::Handle s) override {  // vt+0x20 (BehaviourState::idle)
    if (!pm_state_running(b)) pm_hook_missing("assumed: the order's state idle (vt+0x20), MW3's state");
    return static_cast<BehaviourState*>(pm_ptr(s))->idle();
  }
  zgame::Handle as_net(zgame::Handle obj) override { return obj; }
  zgame::Handle make_net(zgame::Handle net) override {
    net_ = static_cast<AiNet*>(pm_ptr(net));
    return net;
  }
  zgame::Handle make_state(zgame::Handle beh) override {
    g_making = true;
    b.push(net_, static_cast<Behaviour*>(pm_ptr(beh)));
    g_making = false;
    return pm_id(b.current().state.get());
  }
  zgame::Handle default_net() override { return pm_id(brain_hooks().builtin_net()); }
  zgame::Handle default_state() override { return pm_id(brain_hooks().default_behaviour()); }
  zgame::Handle stand_state() override { return pm_id(brain_hooks().default_behaviour()); }  // [0x0062faf8]
  void free_part(zgame::Handle part) override {
    if (!popped && part == mirror.orders.back().state) b.pop(), popped = true;
  }
  // Brain::pop resumes the order under it already (MW3's 0x004e8380), as 0x004ea220 then does.
  void resume(zgame::Handle s) override {
    if (!popped) static_cast<BehaviourState*>(pm_ptr(s))->resume();
  }
  void stop_state(zgame::Handle) override {}
  zgame::Handle net_of(zgame::Handle net) override { return net; }
  void run_order(OrderPm&, float dt) override {
    b.x104 = 0;
    b.current().run(dt);
    mirror.again = b.x104;
    sync_pending();
  }
  void run_pending(zgame::Handle) override {
    pm_run_pending(b);
    sync_pending();
  }
  // The exe's loop sees what a state queues at once (one list); the mirror counts the brain's again. Without it a
  // terminus (0x004e1fd0: return_back, then again) looped in one frame, queuing for ever.
  void sync_pending() { mirror.pending.resize(pm_pending_count(b)); }

  static bool missing(const char* key) {
    pm_hook_missing(key);
    return true;
  }
  Brain& b;
  OrdersPm mirror;
  AiNet* net_ = nullptr;
  bool popped = false;
};
}  // namespace

bool pm_orders_making() { return g_making; }

namespace {
// PM's scan and canleave states run 0x004d5a10: MW3's tests on the brain (their PM twins by name), then the next.
class PmScanState final : public BehaviourState, ScanPmHooks {
 public:
  using BehaviourState::BehaviourState;
  void run(float) override {
    const auto* st = static_cast<const ScanStep*>(from);
    OrdersPm mirror;
    scan_run_pm(*this, mirror, static_cast<int32_t>(st->tests.size()), pm_id(st->next));
  }
  Behaviour* behaviour() const override { return from; }
  void scan_test(int32_t i) override { order->owner->scan_test(static_cast<const ScanStep*>(from)->tests[size_t(i)].first); }
  void set_behaviour(zgame::Handle next) override {
    order->owner->x104 = 1;  // the mirror's `again` is the brain's +0x158, MW3's x104
    order->set_behaviour(static_cast<Behaviour*>(pm_ptr(next)));  // 0x004f9f00: this state destroyed
  }
};
}  // namespace

BehaviourState* pm_make_scan_state(Order& o, Behaviour& b) { return new PmScanState(&o, &b); }
bool pm_is_scan_state(const BehaviourState* s) { return dynamic_cast<const PmScanState*>(s) != nullptr; }

namespace {
float as_float(uint32_t u) {
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}

// PM's scripted steps' states (OrderStepPmHooks) on the brain. The waits' ctors (0x004d4970, 0x004d4ad0) are MW3's: the
// start the game time, the pilot stopped (IControls vt+0x14).
class PmStepState final : public BehaviourState, OrderStepPmHooks {
 public:
  PmStepState(Order* o, Behaviour* b) : BehaviourState(o, b) {
    if (b->kind == StepKind::WaitInterval || b->kind == StepKind::WaitTime) {
      start_ = brain_hooks().now();  // [0x0062da90]
      o->owner->controls->stop();
    }
  }
  Behaviour* behaviour() const override { return from; }  // 0x004df460
  void run(float) override {
    const auto* st = static_cast<const Step*>(from);
    OrdersPm mirror;
    switch (from->kind) {
      case StepKind::Speed: speed_run_pm(*this, mirror, static_cast<const ValueStep*>(from)->value, pm_id(st->next)); break;
      case StepKind::WaitInterval:
      case StepKind::WaitTime:
        wait_run_pm(*this, start_, as_float(static_cast<const ValueStep*>(from)->value), brain_hooks().now(), from->kind == StepKind::WaitTime,
                    pm_id(st->next));
        return;  // no again
      case StepKind::ValveAssign: {
        const auto* a = static_cast<const AssignStep*>(from);
        assign_run_pm(*this, mirror, pm_id(from), pm_id(a->valve), a->value, pm_id(st->next));
        break;
      }
      default: pass_run_pm(*this, mirror, pm_id(from), pm_id(st->next)); break;
    }
  }
  void set_max_throttle(uint32_t bits) override { order->owner->xec = as_float(bits); }
  zgame::Handle path_next(zgame::Handle step) override {
    return pm_id(brain_hooks().path_next(*order->owner, static_cast<Behaviour*>(pm_ptr(step))));
  }
  // FIX (bug), as MW3's AssignState: a record whose valve isn't found has none, which the exe sets (a crash).
  void valve_set(zgame::Handle valve, int32_t value) override {
    if (auto* v = static_cast<Valve*>(pm_ptr(valve))) v->set(value);
  }
  void set_behaviour(zgame::Handle next) override {
    if (from->kind != StepKind::WaitInterval && from->kind != StepKind::WaitTime) order->owner->x104 = 1;  // the mirror's again
    order->set_behaviour(static_cast<Behaviour*>(pm_ptr(next)));  // 0x004f9f00: this state destroyed
  }

 private:
  float start_ = 0;  // +0x0c
};
}  // namespace

BehaviourState* pm_make_step_state(Order& o, Behaviour& b) {
  switch (b.kind) {
    case StepKind::Speed:
    case StepKind::WaitInterval:
    case StepKind::WaitTime:
    case StepKind::Valve:
    case StepKind::Shunt:
    case StepKind::ValveUnion:
    case StepKind::ValveAssign: return new PmStepState(&o, &b);
    default: return nullptr;
  }
}
bool pm_is_step_state(const BehaviourState* s) { return dynamic_cast<const PmStepState*>(s) != nullptr; }

void pm_run_orders(Brain& b, float dt) {
  if (b.orders.empty()) return;
  OnBrain h(b);
  run_orders_pm(h, h.mirror, dt);
}

bool pm_may_fire(Brain& b) {
  OnBrain h(b);
  return may_fire_pm(h, h.mirror);
}

bool pm_orders_idle(Brain& b) {
  OnBrain h(b);
  return orders_idle_pm(h, h.mirror);
}

void pm_give_order(Brain& b, AiNet* net, Behaviour* beh) {
  OnBrain h(b);
  give_order_pm(h, h.mirror, pm_id(net), pm_id(beh));
}

void pm_push_behaviour(Brain& b, Behaviour* beh) {
  OnBrain h(b);
  push_behaviour_pm(h, h.mirror, pm_id(beh));
}

void pm_pop_order(Brain& b) {
  OnBrain h(b);
  pop_order_pm(h, h.mirror);
  if (!h.popped) b.pop();  // an order without a state still goes
}

// Pirate's Moon 1.0 @ 0x004fa770 (MW3's 0x004e9100): when any order is on the net, every order goes, each by PM's pop
// (0x004ea220, while the count +0x2c is not 0). FIX (bug), as MW3's: the exe's search for a net no order is on steps
// past a full last block of the deque (31 orders a block here) into its map; here it stops at the last order.
bool pm_clear_if_net(Brain& b, AiNet* n) {
  if (!n) return false;
  bool found = false;
  for (const Order& r : b.orders)
    if (r.net && r.net->net() == n) {
      found = true;
      break;
    }
  if (!found) return false;
  while (!b.orders.empty()) pm_pop_order(b);
  return true;
}

// Pirate's Moon 1.0 @ 0x004ea690 (by 0x004fa190, 0x004ea8a0; MW3's 0x004e8960): an order on the net over what runs,
// with no behaviour of its own (0x004ea5f0, 0x004f9ec0 -> 0x004f9e70: the default state).
bool pm_push_net(Brain& b, AiNet* n) {
  if (!n) return false;
  pm_give_order(b, n, nullptr);
  return true;
}

// Pirate's Moon 1.0 @ 0x004ea3a0 (by 0x004fa110, 0x004ea5c0; MW3's 0x004e8660): the last order goes when it is on
// the net, the one under it taking up again.
bool pm_pop_if_net(Brain& b, AiNet* n) {
  if (!n || b.orders.empty()) return false;
  const Order& top = b.current();
  if (!top.net || top.net->net() != n) return false;
  pm_pop_order(b);
  return true;
}

// Pirate's Moon 1.0 @ 0x004fa320: the pilot's +0xee bit 1 (+0xce here; on unless its aiv clears it), and the
// current order's net part and state both answering yes: the part's vt+0x10 (no on the built-in net's, as its
// vt+0x14) and the state's vt+0x1c (no for a jump).
bool pm_order_leavable(Brain& b) {
  if (!(b.params.raw[0xce] & 2)) return false;
  const Order& o = b.current();
  AiNet* n = o.net ? o.net->net() : nullptr;
  if (n && dynamic_cast<BuiltinNet*>(n)) return false;
  return !o.state || o.state->may_leave();
}

namespace {
// PM's lance orders on the brain: the contact is the order's own (one per call), the orders the brain's MW3 deque.
struct OnLance : LancePmHooks {
  OnLance(Brain& b, Contact* c) : b(b), c(c), h(brain_hooks()) {}
  zgame::Handle own_vehicle() override { return b.controls ? b.controls->vehicle() : 0; }
  zgame::Handle contact_vehicle(zgame::Handle) override { return c ? h.contact_entity(c) : 0; }
  bool leavable() override { return pm_order_leavable(b); }
  zgame::Handle vehicle_net(zgame::Handle v) override {  // PM's ten slots
    return pm_id(static_cast<AiNet*>(pm_vehicle_net(v)));
  }
  zgame::Handle follow_behaviour(zgame::Handle net) override {  // 0x004c7920: slot 0
    auto* vn = dynamic_cast<VehicleNet*>(static_cast<AiNet*>(pm_ptr(net)));
    return pm_id(vn ? vn->follow() : nullptr);
  }
  void give(zgame::Handle net, zgame::Handle beh) override {
    pm_give_order(b, static_cast<AiNet*>(pm_ptr(net)), static_cast<Behaviour*>(pm_ptr(beh)));
  }
  zgame::Handle first_order() override { return b.orders.empty() ? 0 : 1; }
  void pop_to(zgame::Handle) override {
    while (b.orders.size() > 1) pm_pop_order(b);
  }
  int32_t give_net(zgame::Handle net) override {
    pm_give_order(b, static_cast<AiNet*>(pm_ptr(net)), nullptr);
    return 1;
  }
  zgame::Handle formation() override { return pm_id(static_cast<AiNet*>(h.formation_net())); }
  uint32_t team(zgame::Handle v) override { return h.targetable(v) ? h.targetable(v)->team : 0; }
  uint32_t allies(zgame::Handle own) override { return h.targetable(own) ? h.targetable(own)->allies : 0; }
  uint32_t enemies(zgame::Handle own) override { return h.targetable(own) ? h.targetable(own)->enemies : 0; }
  void keep(zgame::Handle target) override {
    if (const zgame::Handle v = own_vehicle()) pm_firing(b, v).pick.prefer = target;
  }
  int32_t refit() override { return h.refit(b); }
  bool has_radio() override { return b.radio != nullptr; }
  void radio_line(int32_t line) override { h.radio_line(b, line); }
  Brain& b;
  Contact* c;
  BrainHooks& h;
};
}  // namespace

int32_t pm_lance_order(Brain& b, int32_t order, Contact* c) {
  OnLance h(b, c);
  return lance_order_pm(h, order, 1);
}

}  // namespace mech3::ai
