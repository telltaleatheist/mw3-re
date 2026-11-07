#ifndef MECH3_ZAI_WIRE_ATTACK_PM_H
#define MECH3_ZAI_WIRE_ATTACK_PM_H

// The attack state's hooks on the program, one per state: the walking context and the fight are real, on the
// brain's WireWalk and steering. The order's side waits on the orders' wiring, and is counted till then.
#include <memory>

#include "mech3/zai_attack_pm.h"
#include "mech3/zai_wire_walk_pm.h"

namespace mech3::ai {

class WireAttack : public AttackPmHooks, public PendingPmHooks {
 public:
  WireAttack(Brain& b, WireWalk& walk, SteerPm& steer, zgame::Handle vehicle)
      : b_(b), walk_(walk), steer_(steer), v_(vehicle) {}

  bool context(AttackStatePm& s) override;
  void fight_step() override;
  void fight_enter() override;
  double speed() override { return brain_hooks().health(v_); }
  float now() override { return brain_hooks().now(); }
  float intensity() override;
  const float* position() override;
  uint32_t orders() override { return static_cast<uint32_t>(b_.orders.size()); }
  zgame::Handle prior_net() override;
  zgame::Handle nearest(zgame::Handle net, const float* at) override;
  const float* place_of(zgame::Handle behaviour) override;
  zgame::Handle order_target() override { return walk_.order_target(); }
  zgame::Handle kind_behaviour(int32_t kind) override;
  void queue_after(zgame::Handle target, zgame::Handle behaviour) override;
  void return_to(zgame::Handle behaviour) override;
  void pop_order() override;
  void set_behaviour(zgame::Handle b) override { walk_.set_behaviour(b); }
  void follow(zgame::Handle target, zgame::Handle b) override;

 private:
  Brain& b_;
  WireWalk& walk_;
  SteerPm& steer_;
  zgame::Handle v_;
  AttackStatePm* state_ = nullptr;
  std::unique_ptr<WalkAttackPm> ctx_;
};

// The attack's pending items (0x004ea0f0, 0x004ea180), run one a frame by the tick's run_orders before the order.
bool pm_run_pending(Brain& b);
void pm_pending_forget(const Brain& b);

}  // namespace mech3::ai

#endif
