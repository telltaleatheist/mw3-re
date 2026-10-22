#pragma once
#include <cstdint>
#include <vector>

#include "mech3/zai_brain.h"

// The node state (docs/ai.md 17.4a): what a pilot runs while its order's
// behaviour is a node of its net. It walks the brain's steering segment back
// to the node under the pilot's own controls, then picks the node's next link
// (the way finder's first, else a weighted random one, straight on preferred)
// and gives the order that step. Original file unknown: the state sits with
// the steering (0x004cdd40..0x004cf330), the link weights with the steps
// (0x004c7960..0x004c7ae0, 0x004cd430..0x004cdbc0).
namespace mech3::ai {

// [0x0060b478] (0x004c7900 at start-up): how the walking states turn: a dead
// zone of 5 degrees (its cosine and sine), the gain past it, the least turn.
struct TurnParams {
  float cos_dead, sin_dead, gain, least;
};
const TurnParams& turn_params();

// 0x004cdd40: the turn toward `to` (flat, x and z) for a vehicle facing
// `fwd`: 0 within the dead zone, else (the side past it) * gain clamped to
// [least, 1]; full one way or the other when `to` is behind (dot < 0), and
// `keep` (or 1 for none) when it is more than 5 degrees off straight back.
// `dot` and `cross` are what it measured.
float turn_toward(const float fwd[3], const float to[3], float* dot, float* cross, float keep, const TurnParams& p);
// 0x004cdee0: the same for a vehicle that may back up: past 135 degrees off
// it faces away (`reverse` set) and turns its back toward `to`.
float turn_or_back(const float fwd[3], const float to[3], float* dot, float* cross, float keep, bool* reverse,
                   const TurnParams& p);

// 0x004c7ae0 (0x20 bytes, on the stack of the link pick 0x004ccf20): where
// the links are measured from, and the last one's flat direction.
struct LinkWay {
  int32_t x00 = -1;      // +0x00  0 once measured
  float dist = 0;        // +0x04  the flat distance to the last link's node
  float from[3] = {};    // +0x08  the node picking, moved back for a near link
  float dir[3] = {};     // +0x14  the flat unit way to it
  void measure(const float to[3], const float fwd[3]);  // 0x004c79f0 (with 0x004c7960)
};

// 0x004ccf20: the step after node `at` for a pilot heading `heading`: the
// brain's way finder's (0x004c7cb0) when it has one; else among the links
// that weigh more than nothing (vt+0xa8), the only one, or one at random by
// weight, a link straight on weighing a hundred times one back the way the
// pilot came. Null for none. `sums`, when given, gets the running sums of
// the counted weights (what the exe leaves in its scratch vector).
Behaviour* pick_link(Brain& b, Behaviour* at, const float heading[3], std::vector<float>* sums = nullptr);

// A wait step's state (0x004c9fa0 -> 0x004c9fc0, vtable 0x005cb618;
// 0x004ca0c0 -> 0x004ca0e0, vtable 0x005cb6f0): made with the game time
// (+0x0c) and the pilot stopped (IControls vt+0x14); the next step once
// the step's seconds have passed since then (wait_interval) or the game
// time has passed them (wait_time).
class WaitState : public BehaviourState {
 public:
  WaitState(Order* o, Behaviour* b, bool absolute);            // 0x004c9fc0 / 0x004ca0e0
  void run(float dt) override;                                  // 0x004ca070 / 0x004ca190
  Behaviour* behaviour() const override { return from; }        // 0x004c0030
  bool absolute;
  float start;  // +0x0c
};


// An mcbwait node's state (0x004ccce0 -> 0x004ccd00, vtable 0x005cbfe8):
// the pilot stopped there (IControls vt+0x14) until the game time is past
// the node's (+0x34: its link 0 then), or its valve (+0x2c) holds its value
// (+0x30: link 1).
class McbWaitState : public BehaviourState {
 public:
  McbWaitState(Order* o, Behaviour* node);                      // 0x004ccd00
  void run(float dt) override;                                  // 0x004ccda0
  Behaviour* behaviour() const override { return from; }        // 0x004c0030
};

// A sound step's state (0x004cce50 -> 0x004cce70, vtable 0x005cc0c0): the
// step's sound at the pilot's place, then the next step.
class SoundState : public BehaviourState {
 public:
  using BehaviourState::BehaviourState;
  void run(float dt) override;                                  // 0x004ccec0
  Behaviour* behaviour() const override { return from; }        // 0x004c0030
};

// A gate step's state (0x004cd560 -> 0x004cd640, vtable 0x005cc198): the
// next step (the gate's test is its passable, 0x004cd5d0).
class GateState : public BehaviourState {
 public:
  using BehaviourState::BehaviourState;
  void run(float dt) override;                                  // 0x004cd690
  Behaviour* behaviour() const override { return from; }        // 0x004c0030
};

// The node state (0x004ced40, 0x28 bytes, vtable 0x005cc5c0; made by the
// node's vt+0x8c 0x004c9150).
class NodeState : public BehaviourState {
 public:
  NodeState(Order* o, Behaviour* node);   // 0x004ced40
  void resume() override;                 // 0x004c9700: walking again from the start
  void run(float dt) override;            // 0x004cf140
  Behaviour* behaviour() const override { return from; }  // 0x004c0030

 protected:
  void go_on(Brain& b, BrainControls& c, int32_t wait_phase, bool waiting, bool finder);
 public:

  int32_t phase = 0;    // +0x0c  0 set out, 1 walking, 2 at the node with nowhere to go
  float turn = 0;       // +0x10  the last turn and throttle given the controls
  float x14 = 0;        // +0x14
  float throttle = 0;   // +0x18
  float slow = 0;       // +0x1c  the vehicle's stopping distance at its speed (vt+0x120)
  float inv_slow = 0;   // +0x20
  float retry_at;       // +0x24  FLT_MAX; when a node with nowhere to go asks again
};

// A backup node's state (0x004cfa60 -> 0x004cfa80, vtable 0x005cc860): the
// node state backing up to the node, then turning round.
class BackupState : public NodeState {
 public:
  using NodeState::NodeState;
  void run(float dt) override;  // 0x004cfad0
};

// "mcbdeploy" (0x004cfe40): [0x0060b4a8] and the value it holds deploys at,
// [0x0060b4ac].
struct DeployValve {
  Valve* valve = nullptr;
  int32_t value = 0;
};
DeployValve& deploy_valve();
void deploy_mission_load();  // 0x004cfe40, from the mission load's 0x004e7b40
const TurnParams& fine_turn_params();  // [0x0060b4d8]

// A deploy node's state (0x004d0020 -> 0x004d0040, 0x38 bytes, vtable
// 0x005cc950): walked there, placed on the node facing away from its target,
// held while "mcbdeploy" says, then on (docs/ai.md 17.4a).
class DeployState : public NodeState {
 public:
  DeployState(Order* o, Behaviour* node);  // 0x004d0040: the node again at +0x28
  void run(float dt) override;             // 0x004d00a0

  float face[3] = {};  // +0x2c  the way to face, from the target to the node

 private:
  void hold(Brain& b, BrainControls& c, const float fwd[3], bool first);
};

// 0x004cee80: one frame of walking toward the carrot: the turn and the
// throttle for the controls. `away` is the vehicle's along and side from
// the carrot (absolute), `dist` their sum, `near` how close counts as there
// (half the stopping distance), `inv_slow` its inverse; the throttle comes
// from how far along and aside it is past `slack_along` / `slack_side`
// (the nodes' 0.04 each; the follow state's its own).
void walk_drive(Brain& b, zgame::Handle v, const float pos[3], const float fwd[3], const float away[2], float dist,
                float near, float* turn, float* throttle, float inv_slow, float slack_along = 0.04f,
                float slack_side = 0.04f);

}  // namespace mech3::ai
