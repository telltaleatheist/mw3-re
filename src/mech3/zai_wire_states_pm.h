#ifndef MECH3_ZAI_WIRE_STATES_PM_H
#define MECH3_ZAI_WIRE_STATES_PM_H

// Pirate's Moon's states in MW3's orders under PM_AI=1: a behaviour whose PM state is ported makes it
// (in a BehaviourState of ours, so MW3's order stack runs it), the rest make MW3's. docs/pm-tests.md,
// The switch's plan.
#include <memory>

#include "mech3/zai_attack_pm.h"
#include "mech3/zai_brain.h"

namespace mech3::ai {

class WireAttack;

// The behaviour's vt+0x8c under PM_AI=1: a PM state, or null for MW3's own.
BehaviourState* pm_make_state(Behaviour& b, Order& o);
// Whether the brain's current order runs a PM state: the steering it drives is the record's then.
bool pm_state_running(Brain& b);

// 0x004deb80 -> 0x004decb0: the vehicle's attack (MW3's FightState, 0x004d1ca0).
class PmAttackState final : public BehaviourState {
 public:
  PmAttackState(Order* o, Behaviour* b);
  ~PmAttackState() override;
  bool may_break_in() const override { return false; }  // as MW3's FightState until PM's slots are read
  bool may_fire() const override { return false; }
  void run(float dt) override;
  void resume() override { s_.reset(); }  // vt+0x10: 0x004d9ce0

 private:
  AttackStatePm s_;
  std::unique_ptr<WireAttack> hooks_;
  bool started_ = false;
};

}  // namespace mech3::ai

#endif
