#include "mech3/zai_wire_states_pm.h"

#include "mech3/zai_steps.h"
#include "mech3/zai_wire_attack_pm.h"
#include "mech3/zai_wire_pm.h"
#include "mech3/zai_wire_walk_pm.h"

namespace mech3::ai {

BehaviourState* pm_make_state(Behaviour& b, Order& o) {
  switch (b.kind) {
    case StepKind::VehAttack: return new PmAttackState(&o, &b);
    default: return nullptr;
  }
}

bool pm_state_running(Brain& b) {
  return !b.orders.empty() && dynamic_cast<PmAttackState*>(b.current().state.get()) != nullptr;
}

PmAttackState::PmAttackState(Order* o, Behaviour* b) : BehaviourState(o, b) {}
PmAttackState::~PmAttackState() = default;

// 0x004dedf0 each frame. The brain's fields it keeps (AttackBrainPm) are the record's and the
// params', handed in and back each frame; the walking context and the steering are the record's.
void PmAttackState::run(float dt) {
  (void)dt;
  Brain& b = *order->owner;
  BrainControls& c = *b.controls;
  const zgame::Handle v = c.vehicle();
  PmBrain& r = pm_brain(b);
  WireWalk& walk = pm_walk(b, c);
  if (!hooks_) hooks_ = std::make_unique<WireAttack>(b, walk, r.steer, v);
  if (!started_) {
    started_ = true;
    s_.start(walk, v, static_cast<int32_t>(pm_id(order)));  // +0x38: the order the maker was given
  }
  AttackBrainPm a;
  a.mode = r.tick.mode;
  a.kind = b.params.get<int32_t>(0x40);
  a.delay = b.params.get<float>(0x4c);
  a.x58 = b.params.get<float>(0x58);
  a.x5c = b.params.get<int32_t>(0x5c), a.x60 = b.params.get<int32_t>(0x60);
  a.xb4 = b.params.get<float>(0xb4);
  a.xfc = r.tick.delay_until, a.place = r.tick.home, a.x108 = r.tick.attack_until, a.x1f0 = r.tick.give_up;
  s_.step(*hooks_, a);
  // The step may have popped this order (and so this state): only the record is touched after it.
  // FIX (bug): the exe leaves the brain's +0x104 on the state's own hold (+0x44), which the pop frees
  // while the leash goes on reading it; the record keeps a copy (docs/fixes.md).
  if (a.place) {
    r.home_kept[0] = a.place[0], r.home_kept[1] = a.place[1], r.home_kept[2] = a.place[2];
    a.place = r.home_kept;
  }
  r.tick.mode = a.mode, r.tick.delay_until = a.xfc, r.tick.home = a.place;
  r.tick.attack_until = a.x108, r.tick.give_up = a.x1f0;
}

}  // namespace mech3::ai
