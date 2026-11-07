#include "mech3/zai_wire_attack_pm.h"

#include <list>
#include <map>

#include "mech3/zai_steps.h"
#include "mech3/zai_wire_nets_pm.h"
#include "mech3/zai_wire_orders_pm.h"

namespace mech3::ai {

// 0x004dedf0's start: a fresh context each frame, as the exe's on its stack.
bool WireAttack::context(AttackStatePm& s) {
  state_ = &s;
  ctx_ = std::make_unique<WalkAttackPm>(walk_, steer_, v_, s, s.hold_at, pm_brain(b_).tick.mode);
  if (ctx_->no_target && !s.hold_at) {  // why, for the first run: counted, not gaps
    const zgame::Handle t = walk_.order_target();
    pm_hook_missing(!t ? "why no target: no vehicle net order"
                    : !walk_.target_live(t) ? "why no target: not live"
                    : walk_.must_leave(v_) ? "why no target: must leave (mode or checks)"
                                           : "why no target: measured");
  }
  return ctx_->no_target != 0;
}

void WireAttack::fight_step() { static_cast<FightStatePm&>(*state_).step(walk_, *ctx_); }
void WireAttack::fight_enter() { state_->enter(walk_, v_); }

// [0x0062cc60], beside the aim's [0x0062cc64]: taken as MW3's GameIntensity [0x00608e54], as that pair is.
float WireAttack::intensity() { return brain_hooks().intensity(); }

const float* WireAttack::position() { return ctx_->pos; }

zgame::Handle WireAttack::prior_net() {
  if (b_.orders.size() < 2) return 0;
  const Order& o = b_.orders[b_.orders.size() - 2];
  return o.net ? pm_id(o.net->net()) : 0;
}

zgame::Handle WireAttack::nearest(zgame::Handle net, const float* at) {
  return pm_id(brain_hooks().nearest(static_cast<AiNet*>(pm_ptr(net)), at));
}

const float* WireAttack::place_of(zgame::Handle behaviour) {
  return static_cast<Behaviour*>(pm_ptr(behaviour))->way_point();
}

zgame::Handle WireAttack::kind_behaviour(int32_t kind) { return pm_id(pm_kind_behaviour(b_, kind)); }

namespace {
std::map<const Brain*, std::list<PendingPm>>& pending() {
  static std::map<const Brain*, std::list<PendingPm>> m;
  return m;
}
// What a pending item does to the brain's orders when it runs, after the state that left it may be gone.
struct PendingOn : PendingPmHooks {
  explicit PendingOn(Brain& b) : b(b) {}
  void pop_order() override { pm_pop_order(b); }
  void set_behaviour(zgame::Handle beh) override { b.current().set_behaviour(static_cast<Behaviour*>(pm_ptr(beh))); }
  void follow(zgame::Handle target, zgame::Handle beh) override {
    pm_give_order(b, static_cast<AiNet*>(pm_ptr(target)), static_cast<Behaviour*>(pm_ptr(beh)));
  }
  Brain& b;
};
}  // namespace

// 0x004ea0f0 with a 0x005eeca4: at the back.
// The exe's "target" here is the order's net (its +0x0c, vt+0x1c, vt+0x18, vt+0x50), the net the follow goes on.
void WireAttack::queue_after(zgame::Handle, zgame::Handle behaviour) {
  pm_queue_follow(b_, b_.current().net ? b_.current().net->net() : nullptr, static_cast<Behaviour*>(pm_ptr(behaviour)));
}

void pm_queue_follow(Brain& b, const void* net, const void* behaviour) {
  PendingPm p;
  p.kind = PendingPm::kFollow, p.target = pm_id(net), p.behaviour = pm_id(behaviour);
  pending()[&b].push_back(p);
}

// 0x004ea180 with a 0x005ef254, or with none a 0x005ef250: at the front.
void WireAttack::return_to(zgame::Handle behaviour) {
  PendingPm p;
  p.kind = behaviour ? PendingPm::kBackTo : PendingPm::kBack, p.behaviour = behaviour;
  pending()[&b_].push_front(p);
}

// 0x004fb260's first half: one item, before the order runs. The exe asks the last order's ready flag (+0x04, set
// from the start); MW3's orders have none, so here it's always ready.
bool pm_run_pending(Brain& b) {
  auto it = pending().find(&b);
  if (it == pending().end() || it->second.empty()) return false;
  PendingPm p = it->second.front();
  it->second.pop_front();
  PendingOn h(b);
  p.run(h);
  return true;
}

void pm_pending_forget(const Brain& b) { pending().erase(&b); }

size_t pm_pending_count(const Brain& b) {
  const auto it = pending().find(&b);
  return it == pending().end() ? 0 : it->second.size();
}
void WireAttack::pop_order() { pm_pop_order(b_); }
void WireAttack::follow(zgame::Handle target, zgame::Handle b) {
  pm_give_order(b_, static_cast<AiNet*>(pm_ptr(target)), static_cast<Behaviour*>(pm_ptr(b)));
}

}  // namespace mech3::ai
