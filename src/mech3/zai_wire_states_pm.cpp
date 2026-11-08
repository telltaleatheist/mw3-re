#include "mech3/zai_wire_states_pm.h"

#include "mech3/zai_wire_orders_pm.h"

#include "mech3/zai_brain_pm.h"
#include "mech3/zai_steps.h"
#include "mech3/zai_wire_attack_pm.h"
#include "mech3/zai_wire_pm.h"
#include "mech3/zai_wire_walk_pm.h"

namespace mech3::ai {

BehaviourState* pm_make_state(Behaviour& b, Order& o) {
  switch (b.kind) {
    case StepKind::VehAttack: return new PmAttackState(&o, &b);
    case StepKind::Jump: return new PmJumpState(&o, &b);
    case StepKind::Node:
    case StepKind::ViaNode: return new PmMoveState(&o, &b);  // 0x005eef64's and 0x005ef498's vt+0x8c
    case StepKind::BackupNode: return new PmMoveState(&o, &b, PmMoveState::kBackup);  // 0x005ef020's
    case StepKind::TerminusNode: return new PmMoveState(&o, &b, PmMoveState::kTerminus);  // 0x005ef578's
    case StepKind::VehFollow:  // for the first runs: whose order made it
      pm_hook_missing(pm_orders_making() ? "follow made: PM's orders" : "follow made: MW3's push");
      return new PmFollowState(&o, &b);
    case StepKind::Scan:
    case StepKind::CanLeave:
    case StepKind::CanLeaveNot: return pm_make_scan_state(o, b);
    case StepKind::Manoeuvre: return new PmManoeuvreState(&o, &b, false);
    case StepKind::Manoeuvre2: return new PmManoeuvreState(&o, &b, true);
    default: break;
  }
  const uint32_t k = static_cast<uint32_t>(b.kind);
  if (k == kVehicleSlotsPm[3]) return new PmCircleState(&o, &b);
  if (k == kVehicleSlotsPm[4]) return new PmManoeuvreState(&o, &b, false);
  if (k == kVehicleSlotsPm[6]) return new PmGuardState(&o, &b);
  if (k == kVehicleSlotsPm[8]) return new PmBehindState(&o, &b);
  return nullptr;
}

bool pm_state_running(Brain& b) {
  if (b.orders.empty()) return false;
  BehaviourState* s = b.current().state.get();
  return dynamic_cast<PmAttackState*>(s) || dynamic_cast<PmJumpState*>(s) || dynamic_cast<PmMoveState*>(s) ||
         dynamic_cast<PmSlotState*>(s) || dynamic_cast<PmManoeuvreState*>(s);
}

PmAttackState::PmAttackState(Order* o, Behaviour* b) : BehaviourState(o, b) {}
PmAttackState::~PmAttackState() { s_.release(); }  // 0x004ded60

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

namespace mech3::ai {

PmJumpState::PmJumpState(Order* o, Behaviour* b) : BehaviourState(o, b) {}

void PmJumpState::run(float dt) {
  Brain& b = *order->owner;
  BrainControls& c = *b.controls;
  if (!jump_) {  // 0x004daf40 once the brain's controls are known
    hooks_ = std::make_unique<WireJump>(b, c, *order, *static_cast<const Step*>(from));
    jump_ = std::make_unique<JumpPm>(*hooks_, pm_brain(b).steer, c.vehicle());
  }
  jump_->run(dt);  // may end in go_next, which destroys this state
}

}  // namespace mech3::ai

namespace mech3::ai {

// Pirate's Moon 1.0 @ 0x004d4600: from the start again, the vehicle stopped (its controls' vt+0x14).
void PmMoveState::resume() {
  s_.step = 0;
  order->owner->controls->stop();
}

void PmMoveState::run(float dt) {
  (void)dt;
  Brain& b = *order->owner;
  BrainControls& c = *b.controls;
  const zgame::Handle v = c.vehicle();
  PmBrain& r = pm_brain(b);
  WireWalk& walk = pm_walk(b, c);
  if (!started_) {  // 0x004dcbe0 once the brain's controls are known
    started_ = true;
    s_.start(walk, r.steer, v, from->position());
  }
  if (run_ == kBackup) s_.run_backup(walk, r.steer, v, from->position());
  else if (run_ == kTerminus) s_.run_terminus(walk, r.steer, v, from->position());
  else s_.run(walk, r.steer, v, from->position());  // either may end in set_behaviour, which destroys this state
}

}  // namespace mech3::ai

namespace mech3::ai {

void PmSlotState::run(float dt) {
  (void)dt;
  Brain& b = *order->owner;
  BrainControls& c = *b.controls;
  const zgame::Handle v = c.vehicle();
  PmBrain& r = pm_brain(b);
  WireWalk& walk = pm_walk(b, c);
  if (!started_) {
    started_ = true;
    start(walk, r.steer, v);
  }
  step(walk, r.steer, v);  // may pop the order or hand it on, either destroying this state
}

// Pirate's Moon 1.0 @ 0x004d8d30, given the time
void PmCircleState::resume() { s_.change_side(brain_hooks().now()); }

}  // namespace mech3::ai

namespace mech3::ai {

// 0x004d5040 each frame on the state's own fields (laid out as MW3's).
void PmManoeuvreState::run(float dt) {
  (void)dt;
  Brain& b = *order->owner;
  BrainControls& c = *b.controls;
  ManoeuvrePm m;
  m.no_jets = no_jets, m.jump = jump, m.phase = phase, m.turn = turn, m.until = until, m.target = target;
  bool gone = false;
  gone_ = &gone;
  m.run(pm_walk(b, c), c.vehicle());
  if (gone) return;  // the run popped the order, and with it this state
  gone_ = nullptr;
  no_jets = m.no_jets, jump = m.jump, phase = m.phase, turn = m.turn, until = m.until, target = m.target;
}

}  // namespace mech3::ai
