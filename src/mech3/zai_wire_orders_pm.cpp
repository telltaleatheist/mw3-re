#include "mech3/zai_wire_states_pm.h"
#include "mech3/zai_wire_orders_pm.h"

#include "mech3/zai_brain_pm.h"
#include "mech3/zai_steps.h"
#include "mech3/zai_wire_attack_pm.h"
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

}  // namespace mech3::ai
