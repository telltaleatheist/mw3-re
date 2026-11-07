#include "mech3/zai_wire_attack_pm.h"

namespace mech3::ai {

// 0x004dedf0's start: a fresh context each frame, as the exe's on its stack.
bool WireAttack::context(AttackStatePm& s) {
  state_ = &s;
  ctx_ = std::make_unique<WalkAttackPm>(walk_, steer_, v_, s, s.hold_at, pm_brain(b_).tick.mode);
  return ctx_->no_target != 0;
}

void WireAttack::fight_step() { static_cast<FightStatePm&>(*state_).step(walk_, *ctx_); }
void WireAttack::fight_enter() { state_->enter(walk_, v_); }

float WireAttack::intensity() {
  pm_hook_missing("[0x0062cc60]");
  return 1.0f;
}

const float* WireAttack::position() { return ctx_->pos; }

zgame::Handle WireAttack::prior_net() {
  if (b_.orders.size() < 2) return 0;
  const Order& o = b_.orders[b_.orders.size() - 2];
  return o.net ? pm_id(o.net->net()) : 0;
}

zgame::Handle WireAttack::nearest(zgame::Handle, const float*) {
  pm_hook_missing("0x004e5580");
  return 0;
}

const float* WireAttack::place_of(zgame::Handle) {
  pm_hook_missing("behaviour.vt+0x1c");
  return nullptr;
}

zgame::Handle WireAttack::kind_behaviour(int32_t) {
  pm_hook_missing("0x004c7800");
  return 0;
}

void WireAttack::queue_after(zgame::Handle, zgame::Handle) { pm_hook_missing("0x004ea0f0"); }
void WireAttack::return_to(zgame::Handle) { pm_hook_missing("0x004ea180"); }
void WireAttack::pop_order() { b_.pop(); }
void WireAttack::follow(zgame::Handle target, zgame::Handle b) {
  b_.push(static_cast<AiNet*>(pm_ptr(target)), static_cast<Behaviour*>(pm_ptr(b)));
}

}  // namespace mech3::ai
