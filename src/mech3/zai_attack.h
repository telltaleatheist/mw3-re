#pragma once
#include <cstdint>

#include "mech3/zai_brain.h"
#include "mech3/zai_node.h"
#include "mech3/zai_steps.h"

// The attack's orders (docs/ai.md 17.2): what the brain's attack
// (0x004e9ca0) pushes under the fight, the way home afterwards. Original
// file unknown: the net sits with the nets (0x004c0810..0x004c0b50), the
// behaviours and states with the steps (0x004cac00..0x004cb11b).
namespace mech3::ai {

// 0x004ce250 (through 0x004ce230): the flat way from `from` to `to`, its
// length and unit direction (y 0).
struct FlatWay {
  float dist = 0;      // +0x00
  float dir[3] = {};   // +0x04
};
void flat_way(const float from[3], const float to[3], FlatWay* w);

// 0x004c8d10: the turn for a target `cross` to the side and `dot` ahead:
// behind, full toward its side; else none within the dead zone, past it
// (side - dead) * gain, at least `least`, at most 1.
float turn_past(float cross, float sin_dead, float gain, float least, float dot);
// 0x004c8df0: the same from `fwd` and the way `dir` (*cross written), with
// the dead zone ahead (none) and straight back (`keep`, or 1 for none).
float turn_hold(const float fwd[3], const float dir[3], const float* dot, float* cross, float keep,
                const TurnParams& p);

// 0x004c0810 (0x1c bytes, vtable 0x005cabd0, over 0x004bfbb0(0, 1)): the
// attack's own net, one step: the stand behaviour (0x00604058, not its to
// free). It allows no attack; its part (+0x5c 0x004c0a70) is an AttackPart.
class AttackNet : public CompiledNet {
 public:
  AttackNet();
  ~AttackNet() override;  // 0x004c0a40 -> 0x004c0a60
  NetPart* make_part(Order& o) override;
  bool allows_attack() override { return false; }
};

// 0x004c0a90 (0x1c bytes, vtable 0x005cac30): a home point for the leash,
// the pilot's position, stacked on the brain's +0xe4; its dtor (0x004c0b50)
// takes it off, and with none under it ends the attack (+0xe8 FLT_MAX,
// +0x230 0), else counts [0x0060b328].
class AttackPart : public NetPart {
 public:
  AttackPart(Order* o, AttackNet* n);
  ~AttackPart() override;
  AiNet* net() const override { return target; }  // 0x0040a890

  float home[3] = {};            // +0x08
  const float* prev = nullptr;   // +0x14
  AttackNet* target = nullptr;   // +0x18
};
int32_t& attack_part_unstacked();  // [0x0060b328], written only

// The three behaviours the attack makes on its stack (0x004c8600 with their
// vtables; their states copy what they need).
// 0x005cd920 (state 0x004cac00 -> 0x004cac20): the attack's net to free.
struct AttackEnd : Behaviour {
  explicit AttackEnd(AttackNet* n) : Behaviour(StepKind::AttackEnd), net(n) {}
  AttackNet* net;  // +0x0c
};
// 0x004caca0, 0x005cb948 (state 0x004cacc0 -> 0x004cace0).
struct AttackRearm : Behaviour {
  AttackRearm() : Behaviour(StepKind::AttackRearm) {}
};
// 0x004cad70, 0x005cba20 (state 0x004cad90 -> 0x004cadb0): the net the
// pilot was on.
struct AttackReturn : Behaviour {
  explicit AttackReturn(AiNet* n) : Behaviour(StepKind::AttackReturn), net(n) {}
  AiNet* net;  // +0x0c
};

// 0x004cac20 (0x0c bytes, vtable 0x005cb928): resumed, its order popped and
// the attack's net freed (0x004cac80).
class AttackEndState : public BehaviourState {
 public:
  AttackEndState(Order* o, AttackEnd* b) : BehaviourState(o, b), net(b->net) {}
  void resume() override;
  AttackNet* net;  // +0x08
};
// 0x004cace0 (8 bytes, vtable 0x005cba00): resumed, its order popped and
// the re-engage timer set (0x004cad30: +0x60 off for good, +0x5c a delay of
// +0x58; +0x3c off).
class AttackRearmState : public BehaviourState {
 public:
  using BehaviourState::BehaviourState;
  void resume() override;
};
// 0x004cadb0 (0x1c bytes, vtable 0x005cbad8): the way home. Resumed, the
// old net's step nearest the pilot (0x004d51d0) is its waypoint (0x004cae10);
// run (0x004cae90), the pilot drives there and the order ends within +0x28.
class AttackReturnState : public BehaviourState {
 public:
  AttackReturnState(Order* o, AttackReturn* b) : BehaviourState(o, b), net(b->net) {}
  void resume() override;
  void run(float dt) override;
  AiNet* net;             // +0x08
  float way[3] = {};      // +0x0c
  float turn = 0;         // +0x18
};

// Mech3.exe 1.2 @ 0x004e9ca0 (from the brain's frame, before it pushes the
// fight): the current order remade on `n`, then the attack's net and its
// three orders. False when the net cannot be made.
bool attack(Brain& b, AiNet* n);

}  // namespace mech3::ai
