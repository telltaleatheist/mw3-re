#ifndef MECH3_ZAI_WIRE_STATES_PM_H
#define MECH3_ZAI_WIRE_STATES_PM_H

// Pirate's Moon's states in MW3's orders under PM_AI=1: a behaviour whose PM state is ported makes it
// (in a BehaviourState of ours, so MW3's order stack runs it), the rest make MW3's. docs/pm-tests.md,
// The switch's plan.
#include <cstring>
#include <memory>

#include "mech3/app_mech.h"
#include "mech3/mech.h"
#include "mech3/zai_attack.h"
#include "mech3/zai_attack_pm.h"
#include "mech3/zai_drive_pm.h"
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
  bool may_break_in() const override { return true; }  // vt+0x04, +0x08: 0x004eb000 (MW3's FightState says no to both)
  bool may_fire() const override { return true; }
  void run(float dt) override;
  void resume() override { s_.reset(); }  // vt+0x10: 0x004d9ce0

 private:
  AttackStatePm s_;
  std::unique_ptr<WireAttack> hooks_;
  bool started_ = false;
};

// The node's state (0x004d4010 -> 0x004dcbe0, vtable 0x005eee30; MW3's NodeState, 0x004ced40) and the via
// node's (0x004e1dc0 -> 0x004e1e20, vtable 0x005ef554, whose run 0x004e1e80 is the node's): walking to the
// node on the record's steering, then on to the next behaviour. The backup node's (0x004dda00 -> 0x004dda60,
// vtable 0x005ef0dc) and the terminus's (0x004e1f00 -> 0x004e1f60, vtable 0x005ef634) are the same but for
// their runs, 0x004ddab0 and 0x004e1fd0.
class PmMoveState final : public BehaviourState {
 public:
  enum Run { kNode, kVia, kBackup, kTerminus };
  PmMoveState(Order* o, Behaviour* b, Run run = kNode) : BehaviourState(o, b), run_(run) {}
  // vt+0x04, +0x08: the via node's and the terminus's 0x004d5020, no; the node's and the backup's yes.
  bool may_break_in() const override { return run_ != kVia && run_ != kTerminus; }
  bool may_fire() const override { return run_ != kVia && run_ != kTerminus; }
  Behaviour* behaviour() const override { return from; }
  void resume() override;  // vt+0x10: 0x004d4600
  void run(float dt) override;

 private:
  MoveStatePm s_;
  Run run_;
  bool started_ = false;
};

// The deploy node's state (0x004de010 -> 0x004de070 over the move state's 0x004dcbe0, vtable 0x005ef1d8; MW3's
// DeployState): its run 0x004de0e0 on the record's steering.
class PmDeployState final : public BehaviourState {
 public:
  PmDeployState(Order* o, Behaviour* b) : BehaviourState(o, b) {}
  Behaviour* behaviour() const override { return from; }
  void resume() override;  // vt+0x10: 0x004d4600, the move state's
  void run(float dt) override;

 private:
  DeployStatePm s_;
  bool started_ = false;
};

// The vehicle net's slot states (make_state on each slot's vt+0x8c, the state taking the slot's behaviour and
// the order): PM's follow (slot 0, 0x004d91e0 -> 0x004daaf0, vtable 0x005eecd4), circling (slot 3, 0x004d8c40 ->
// 0x004d8ca0, 0x005eebe4), guarding (slot 6, 0x004dfca0 -> 0x004dfe40, 0x005ef2e8) and behind (slot 8,
// 0x004d8520 -> 0x004d8690, 0x005eebc0). Each starts at its first run, once the brain's controls are known.
class PmSlotState : public BehaviourState {
 public:
  PmSlotState(Order* o, Behaviour* b) : BehaviourState(o, b) {}
  Behaviour* behaviour() const override { return from; }
  void run(float dt) override;

 protected:
  virtual void start(WireWalk& w, SteerPm& s, zgame::Handle v) = 0;
  virtual void step(WireWalk& w, SteerPm& s, zgame::Handle v) = 0;
  bool started_ = false;
};

class PmFollowState final : public PmSlotState {
 public:
  using PmSlotState::PmSlotState;
  ~PmFollowState() override { s_.release(); }  // 0x004daba0
  void resume() override { s_.reset(); }       // vt+0x10: 0x004dede0, a jump to 0x004d9ce0

 private:
  void start(WireWalk& w, SteerPm& s, zgame::Handle v) override { follow_start(w, s, v, s_); }
  void step(WireWalk& w, SteerPm& s, zgame::Handle v) override { follow_run(w, s, v, s_); }
  FightStatePm s_;
};

class PmCircleState final : public PmSlotState {
 public:
  using PmSlotState::PmSlotState;
  bool may_break_in() const override { return false; }  // vt+0x04, +0x08: 0x004d5020
  bool may_fire() const override { return false; }
  void resume() override;  // vt+0x10: 0x004d8d30

 private:
  void start(WireWalk& w, SteerPm&, zgame::Handle) override { s_.start(w, static_cast<int32_t>(pm_id(from))); }
  void step(WireWalk& w, SteerPm&, zgame::Handle v) override { s_.run(w, v); }
  CirclePm s_;
};

class PmGuardState final : public PmSlotState {
 public:
  using PmSlotState::PmSlotState;
  void resume() override {}  // vt+0x10: 0x00403ac0, nothing

 private:
  void start(WireWalk& w, SteerPm&, zgame::Handle v) override { s_.start(w, v, static_cast<int32_t>(pm_id(from))); }
  void step(WireWalk& w, SteerPm& s, zgame::Handle v) override { s_.run(w, s, v); }
  GuardStatePm s_;
};

// Slot 2 (0x004e0ff0 -> 0x004e1050, vtable 0x005ef424): MW3's mode 1 circling, PM's.
class PmRoundState final : public PmSlotState {
 public:
  using PmSlotState::PmSlotState;
  bool may_break_in() const override { return false; }  // vt+0x04, +0x08: 0x004d5020
  bool may_fire() const override { return false; }
  void resume() override {}  // vt+0x10: 0x00406cb0, nothing

 private:
  void start(WireWalk& w, SteerPm&, zgame::Handle v) override { s_.start(w, v); }
  void step(WireWalk& w, SteerPm&, zgame::Handle v) override { s_.run(w, v); }
  CircleStatePm s_;
};

class PmBehindState final : public PmSlotState {
 public:
  using PmSlotState::PmSlotState;
  void resume() override {}  // vt+0x10: 0x00403ac0, nothing

 private:
  void start(WireWalk& w, SteerPm&, zgame::Handle v) override { s_.start(w, v); }
  void step(WireWalk& w, SteerPm& s, zgame::Handle v) override { s_.run(w, s, v); }
  BehindPm s_;
};

// The manoeuvre (0x004d54e0 -> 0x004d5530 over 0x004d4e80, vtable 0x005ee188): the vehicle net's slot 4 and the
// one at 0x00627ce8 the tick pushes; the one at 0x00627cf8 (vtable 0x005df8dc, 0x004d55e0) gives it no jets. Its constructor, destructor (0x004d55d0 -> 0x004d4fa0), suspend (0x004d5030,
// the one at 0x0062fae8 taking over) and slots are MW3's ManoeuvreState's; its run is PM's 0x004d5040.
class PmManoeuvreState final : public ManoeuvreState {
 public:
  PmManoeuvreState(Order* o, Behaviour* b, bool no_jets) : ManoeuvreState(o, b, no_jets) {}
  ~PmManoeuvreState() override {
    if (gone_) *gone_ = true;
  }
  void run(float dt) override;

 private:
  bool* gone_ = nullptr;  // set while the run is going: its pop may destroy this state
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
  bool may_break_in() const override { return false; }  // vt+0x04, +0x08 of 0x005eedbc: 0x004d5020
  bool may_fire() const override { return false; }
  Behaviour* behaviour() const override { return from; }
  void run(float dt) override;

 private:
  std::unique_ptr<WireJump> hooks_;
  std::unique_ptr<JumpPm> jump_;
};

}  // namespace mech3::ai

#endif
