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
  bool net_idle(zgame::Handle) override { return missing("assumed: the order's net idle (vt+0x14)"); }
  bool state_idle(zgame::Handle) override { return missing("assumed: the order's state idle (vt+0x20)"); }
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
  }
  void run_pending(zgame::Handle) override { pm_run_pending(b); }

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
