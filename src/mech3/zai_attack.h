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

// 0x004c8a70 (EDX the brain): drive the vehicle from `from` toward `to`
// (*turn kept between calls, *throttle out) and set the controls' throttle
// and turn: within +0x28 it has arrived (true); closer than +0x2c, with
// `back`, it backs off at the speed share facing within 0.9, else holds;
// pulled by the boundary it turns toward the pull and is done. The way home
// (0x004cae90) is the same without backing off.
bool drive_to(Brain& b, zgame::Handle v, const float from[3], const float to[3], float* turn, float* throttle,
              bool back);

// The fight (0x004d1ca0 -> 0x004d1cc0, vtable 0x005ccb60, over the waiting
// state 0x004c8a30): what the target vehicle net's attack behaviour (slot 1)
// makes in the order the brain pushes on that net. Run (0x004d1d10): the
// pilot closes on the target vehicle (the net's +0x1c) to within +0x28,
// backing off inside +0x2c, then the order takes the behaviour for its
// mode (the net part's +0x1c); it gives up when either has no position,
// the target is not active (+0x08) or the brain must break off (0x004e8dd0).
// It lets no order break in and the pilot not fire (0x004d72a0, 0x004d7290).
class FightState : public BehaviourState {
 public:
  FightState(Order* o, Behaviour* b) : BehaviourState(o, b), made_by(b) {}
  bool may_break_in() const override { return false; }
  bool may_fire() const override { return false; }
  void run(float dt) override;
  uint32_t x08 = 0;          // +0x08  the waiting state's
  float turn = 0;            // +0x0c
  uint8_t x10 = 0;           // +0x10
  Behaviour* made_by;        // +0x18
};

// The brain's own order's state (docs/ai.md 17.2): keeping station on the
// vehicle whose own net it is on. 0x004cc290 (0x30 bytes, vtable 0x005cbf10,
// over the waiting state 0x004c8a30): +0x20 the throttle's way (-1 when
// the pilot is reversing, IControls vt+0x18), +0x24 the pilot's +0x78 x
// 0.9, +0x28 sqrt(+0x74) x 1.1 (made again when resumed, 0x004cc380), +0x2c
// a count and +0x2e a phase its run (0x004cc3e0, item 10d3) keeps. Lets no
// order break in, the pilot not fire; its behaviour() is its +0x18.
class StationState : public BehaviourState {
 public:
  StationState(Order* o, Behaviour* b);
  bool may_break_in() const override { return false; }
  bool may_fire() const override { return false; }
  void resume() override;  // 0x004cc380
  void run(float dt) override;
  Behaviour* behaviour() const override { return made_by; }  // 0x0040a890
  uint32_t x08 = 0;        // +0x08  the waiting state's
  float turn = 0;          // +0x0c
  uint8_t x10 = 0;         // +0x10
  Behaviour* made_by;      // +0x18
  float way = 1.0f;        // +0x20
  float near_speed = 0;    // +0x24
  float range = 0;         // +0x28
  int16_t count = 0;       // +0x2c
  int16_t phase = 0;       // +0x2e

 protected:
  void take_params();      // the ctor's and the resume's
};
// 0x004cc240 (vtable 0x005cbef0): the same for a pilot whose vehicle's
// vt+0xf4 says so (none of the mechs): its run (0x004cc020) only holds while
// the other vehicle is within +0x28 (phase 1; the stop under [0x0060b48c],
// which nothing sets), else the order ends.
class StationHoldState : public StationState {
 public:
  using StationState::StationState;
  void run(float dt) override;
};
// Mech3.exe 1.2 @ 0x004cc150 (the own-order behaviour's vt+0x8c): none
// without a vehicle, a vehicle net part or its vehicle; for a vt+0xf4
// pilot the hold unless the other is one too; else the station keeping,
// but none for a lancemate (+0xbc bit 4) by another or by the player.
BehaviourState* make_station_state(Behaviour& b, Order& o);

// Mech3.exe 1.2 @ 0x004e9ca0 (from the brain's frame, before it pushes the
// fight): the current order remade on `n`, then the attack's net and its
// three orders. False when the net cannot be made.
bool attack(Brain& b, AiNet* n);

}  // namespace mech3::ai
