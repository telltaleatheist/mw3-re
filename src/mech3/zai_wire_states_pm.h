#ifndef MECH3_ZAI_WIRE_STATES_PM_H
#define MECH3_ZAI_WIRE_STATES_PM_H

// Pirate's Moon's states in MW3's orders under PM_AI=1: a behaviour whose PM state is ported makes it
// (in a BehaviourState of ours, so MW3's order stack runs it), the rest make MW3's. docs/pm-tests.md,
// The switch's plan.
#include <cstring>
#include <memory>

#include "mech3/app_mech.h"
#include "mech3/mech.h"
#include "mech3/zai_attack_pm.h"
#include "mech3/zai_jump_pm.h"
#include "mech3/zai_wire_walk_pm.h"
#include "mech3/zai_brain.h"
#include "mech3/zai_steps.h"
#include "mech3/zai_wire_pm.h"

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

// JumpPmHooks: the walk's answers, and the jump's own as MW3's JumpState asks them.
class WireJump final : public WireWalkT<JumpPmHooks> {
 public:
  WireJump(Brain& b, BrainControls& c, Order& o, const Step& step) : WireWalkT<JumpPmHooks>(b, c), brain_(b), controls_(c), o_(o), step_(step) {}
  zgame::Handle mech(zgame::Handle v) override { return brain_hooks().vehicle_mech(v); }
  bool has_jets(zgame::Handle) override { return brain_.path_pilot.jump_jets > 0; }  // +0x3e8, MW3's +0x3d8
  // PM's +0x588 is the jet fuel (MW3's +0x564): the jump fills it from [0x0060bb70], 1.0, never written.
  void set_x588(zgame::Handle m) override {
    void* o = brain_hooks().object_of(m);
    if (o && entity_is_mech(o)) static_cast<Mech*>(o)->jet_fuel = 1.0f;
  }
  bool in_air(zgame::Handle m) override { return brain_hooks().airborne(m); }
  float climb_speed(zgame::Handle m) override { return brain_hooks().vertical_speed(m); }
  float along_speed(zgame::Handle m) override { return brain_hooks().forward_speed(m); }
  const float* way_point() override { return step_.next->way_point(); }
  float margin() override {  // the step's +0x10, a float in a word
    const uint32_t w = static_cast<const ValueStep&>(step_).value;
    float f;
    std::memcpy(&f, &w, sizeof f);
    return f;
  }
  // [0x00629314]: 0x00417e80 sets it once at start-up to [0x005dfa18] + [0x005dfa1c], 30 + 8.
  float jet_climb() override { return 30.0f + 8.0f; }
  float gravity() override { return brain_hooks().gravity(); }  // [0x00607ec0], MW3's [0x005e5d34]
  void set_jump(float v) override { controls_.set_jump(v); }
  void go_next() override { o_.set_behaviour(step_.next); }  // 0x004f9f00: this state destroyed

 private:
  Brain& brain_;
  BrainControls& controls_;
  Order& o_;
  const Step& step_;
};

// 0x004daf40 / 0x004db0e0: the jump step's state (MW3's JumpState, 0x004cb330).
class PmJumpState final : public BehaviourState {
 public:
  PmJumpState(Order* o, Behaviour* b);
  Behaviour* behaviour() const override { return from; }
  void run(float dt) override;

 private:
  std::unique_ptr<WireJump> hooks_;
  std::unique_ptr<JumpPm> jump_;
};

}  // namespace mech3::ai

#endif
