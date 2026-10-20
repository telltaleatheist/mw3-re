#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "mech3/zainet.h"

// The pieces a compiled AI net is made of (docs/ai.md, The compiled net):
// the behaviour base every order and step derives from, one step per
// property a path node has, and one per node. Only what building and walking
// a net uses is here: the data each one holds, its links, and the
// path finder's visited mark. What a step does when a pilot reaches it is
// the behaviours' (ai.md, porting order step 6). Original file unknown: the
// classes sit between zainetmechinout.cpp and zaitarget.cpp
// (0x004c8600..0x004d0100); the two shared instances at 0x00603fa0 and
// 0x00604068 are made by the start-up code (0x004105d0, 0x004c8750).
namespace mech3::ai {

class CompiledNet;
class BehaviourState;
struct Order;

// What a step asks of the pilot finding its way (in the exe its brain: the
// vehicle at brain +0xf0 through the virtual base at +0xe0).
struct Pilot {
  const char* name = nullptr;  // the vehicle's +0x04, for gates
  int32_t jump_jets = 0;       // the mech's +0x3d8 (vehicle slot +0x50), for jump steps
  const float* pos = nullptr;  // 0x005a85e0: the vehicle's node's position
};

// Which class an element is, by the exe's vtable (the tests compare these).
enum class StepKind : uint32_t {
  Behaviour = 0x5cb278,     // 0x004c8750, the default behaviour (0x00604068)
  Shared = 0x5cbe18,        // 0x004cbe10, the shared behaviour (0x0060b460)
  Null = 0x5bc8d0,          // 0x004105d0, the null step (0x00603fa0)
  Speed = 0x5cb488,         // 0x004c9eb0
  WaitInterval = 0x5cb560,  // 0x004c9f80
  WaitTime = 0x5cb638,      // 0x004ca0a0
  Valve = 0x5cb710,         // 0x004ca1c0
  Scan = 0x5cbbd0,          // 0x004cb950
  CanLeave = 0x5cbca8,      // 0x004cbcf0: canleave ( 1 n ), n != 0
  CanLeaveNot = 0x5cbd60,   // 0x004cbd40: canleave ( 1 0 )
  Jump = 0x5cbaf8,          // 0x004cb2a0
  Sound = 0x5cc008,         // 0x004cce30
  Gate = 0x5cc0e0,          // 0x004cd4a0
  Shunt = 0x5cc1b8,         // 0x004cd6e0
  ValveUnion = 0x5cc368,    // 0x004cda50
  ValveAssign = 0x5cc440,   // 0x004cdc20
  Node = 0x5cc6f0,          // 0x004cf820
  BackupNode = 0x5cc7a8,    // 0x004cfa00
  McbWaitNode = 0x5cbf30,   // 0x004ccc30
  DeployNode = 0x5cc898,    // 0x004cff10
};

// What a scan or canleave entry runs: the exe's function addresses (all in
// 0x004d9c20..0x004d9ca0; what they test is the behaviours').
enum class ScanTest : uint32_t {
  AimOn = 0x4d9c20, AimOff = 0x4d9c30,   // scan_aim ( 1 n ): n != 0 / 0
  HitOn = 0x4d9c40, HitOff = 0x4d9c50,   // scan_hit
  TimeOn = 0x4d9c60, TimeOff = 0x4d9c70, // scan_time
  CanLeave = 0x4d9c80, CanLeaveNot = 0x4d9ca0,
};

// 0x004c8600: 0x0c bytes, vtable 0x005cb180.
struct Behaviour : AiElem {
  explicit Behaviour(StepKind k = StepKind::Behaviour) : kind(k) {}
  ~Behaviour() override = default;
  Behaviour* as_behaviour() override { return this; }  // +0x60 then +0x74
  znet::Element* miss() override;                     // +0x10 0x004106c0: the null step
  virtual int32_t link_count() const { return 0; }    // +0x04
  virtual Behaviour* link(int32_t i) const;           // +0x08: the null step here
  virtual const float* position() const { return nullptr; }  // +0x18
  virtual void attach(CompiledNet* net) { (void)net; }        // +0x6c
  // +0x8c: its state in an order (zai_brain.h; the caller owns it).
  virtual BehaviourState* make_state(Order& o);
  // +0xac 0x004cd490: whether `p` may go on here from `from`; yes but for
  // valves, jumps, gates and valve unions.
  virtual bool passable(const Pilot* p, const Behaviour* from) const {
    (void)p, (void)from;
    return true;
  }
  bool visited() const { return flags & 1; }                   // +0xb4 0x004cd6b0
  bool set_visited(bool v) {                                   // +0xb0 0x004cd6c0
    flags = uint8_t((flags & ~1u) | (v ? 1u : 0u));
    return v;
  }

  StepKind kind;
  uint8_t flags = 0;  // +0x04  bit 0: the path finder has been here
  int32_t x08 = 0;    // +0x08
};
Behaviour* null_step();          // 0x00603fa0
Behaviour* default_behaviour();  // 0x00604068

// 0x004c9d00 (vtable 0x005cb3d0): a step with one way on (+0x0c). Built with
// the slot of what follows, which attach() makes a pointer (0x004c9e60).
struct Step : Behaviour {
  explicit Step(StepKind k) : Behaviour(k) {}
  int32_t link_count() const override { return next != null_step() ? 1 : 0; }  // 0x004c9e40
  Behaviour* link(int32_t i) const override { return i == 0 ? next : null_step(); }  // 0x004c9e00
  void attach(CompiledNet* net) override;

  Behaviour* next = null_step();  // +0x0c
  // While compiling the exe keeps a slot number in the same word; one that
  // names no step stays a number (FIX (bug): the null step here, the number
  // kept for the tests in next_slot with `unresolved`).
  int32_t next_slot = 0;
  bool slot_set = false;
  bool unresolved = false;
};

struct ValueStep : Step {  // speed, the waits, jump: one word at +0x10
  ValueStep(StepKind k, uint32_t v) : Step(k), value(v) {}
  bool passable(const Pilot* p, const Behaviour* from) const override;  // jump: 0x004cb2c0
  uint32_t value;  // +0x10  the record's word as it was (a float)
};

struct SoundStep : Step {  // 0x004cce30
  explicit SoundStep(void* s) : Step(StepKind::Sound), sound(s) {}
  void* sound;  // +0x10  zSound's object
};

struct ValveStep : Step {  // 0x004ca1c0, 0x18 bytes
  ValveStep(Valve* v, int32_t val) : Step(StepKind::Valve), valve(v), value(val) {}
  bool passable(const Pilot* p, const Behaviour* from) const override;  // 0x004ca260
  Valve* valve;   // +0x10
  int32_t value;  // +0x14
};

struct ScanStep : Step {  // 0x004cb950 (+0x004cbcf0, 0x004cbd40), 0x20 bytes
  ScanStep(StepKind k, int32_t reserve) : Step(k), count(uint8_t(reserve)) { tests.reserve(size_t(reserve)); }
  uint8_t count;                                     // +0x10  the ctor's count, kept
  std::vector<std::pair<ScanTest, int32_t>> tests;  // +0x14  (function, 0)
};

struct GateStep : Step {  // 0x004cd4a0
  explicit GateStep(const char* g) : Step(StepKind::Gate), gate(g ? g : ""), has_gate(g != nullptr) {}
  bool passable(const Pilot* p, const Behaviour* from) const override;  // 0x004cd5d0
  std::string gate;  // +0x10  strdup'd
  bool has_gate;
};

struct UnionStep : Step {  // 0x004cda50: the record's valve union copied
  explicit UnionStep(const ValveUnion& u) : Step(StepKind::ValveUnion), tag(u.tag), valves(u.valves) {}
  bool passable(const Pilot* p, const Behaviour* from) const override;  // 0x004cdbc0 -> 0x004d4600
  uint8_t tag;                     // +0x10
  std::vector<ValveCount> valves;  // +0x14
};

struct AssignStep : Step {  // 0x004cdc20
  explicit AssignStep(const ValveAssign& a) : Step(StepKind::ValveAssign), valve(a.valve), value(a.value) {}
  Valve* valve;   // +0x10
  int32_t value;  // +0x14
};

// 0x004cf820 (0x2c bytes, vtable 0x005cc6f0): a place on the path, with a
// way on per link (+0x20), built as slots (the trailing ones naming nothing
// dropped) and made pointers by attach() (0x004c94a0), which drops the ones
// that name no step.
struct NodeStep : Behaviour {
  NodeStep(StepKind k, const float p[3], const int32_t* slots, int32_t n);
  ~NodeStep() override = default;
  int32_t link_count() const override { return int32_t(attached ? links.size() : slots.size()); }  // 0x004c9460
  Behaviour* link(int32_t i) const override;   // 0x004c93d0
  const float* position() const override { return pos; }  // 0x004c9170
  void attach(CompiledNet* net) override;
  void set_name(const char* n) { name = n ? n : "", has_name = n != nullptr; }  // 0x004c9100

  std::string name;   // +0x0c  strdup'd: the record's auxname
  bool has_name = false;
  float pos[3];       // +0x10
  uint8_t x1c;        // +0x1c  the links pointer's low byte (the vector's allocator byte), never read
  std::vector<int32_t> slots;     // +0x20  while compiling
  std::vector<Behaviour*> links;  // +0x20  after attach
  bool attached = false;
};

struct McbWaitNode : NodeStep {  // 0x004ccc30, 0x38 bytes
  McbWaitNode(const float p[3], const int32_t* s, int32_t n, const char* nm, Valve* v, uint8_t b, float t)
      : NodeStep(StepKind::McbWaitNode, p, s, n), valve(v), b(b), time(t) {
    set_name(nm);
  }
  Valve* valve;  // +0x2c
  uint32_t b;    // +0x30
  float time;    // +0x34
};

struct DeployNode : NodeStep {  // 0x004cff10, 0x40 bytes
  DeployNode(const float p[3], const int32_t* s, int32_t n, const char* nm, Valve* va, uint8_t a, Valve* vb,
             uint8_t b, int32_t target)
      : NodeStep(StepKind::DeployNode, p, s, n), valve_a(va), a(a), valve_b(vb), b(b), target(target) {
    if (nm) set_name(nm);
  }
  void attach(CompiledNet* net) override;  // 0x004cffd0: the target's slot made its step too
  Valve* valve_a;  // +0x2c
  uint32_t a;      // +0x30
  Valve* valve_b;  // +0x34
  uint32_t b;      // +0x38
  int32_t target;  // +0x3c  the slot of deploy's node (its index read as a short)...
  Behaviour* target_step = nullptr;  // +0x3c  ...and after attach its step
};

// 0x004bf910 (0x1c bytes, vtable 0x005ca6d8): a net_NN compiled. Its
// elements are the steps above, in slots; the null step fills any slot
// nothing was built for. It owns its steps (0x004bff20).
class CompiledNet : public AiNet {
 public:
  CompiledNet(int32_t id, int32_t slots);  // 0x004bf910 with no starting list
  ~CompiledNet() override;
  AiNet* as_compiled() override { return this; }
  NetPart* make_part(Order& o) override;  // +0x5c 0x004c0070 (zai_brain.cpp)
  bool mark(bool on);  // 0x004c0e70: the path finder's mark (+0x18 bit 0)
  Behaviour* step(int32_t i) const {
    znet::Element* e = elem(i);
    return e ? static_cast<AiElem*>(e)->as_behaviour() : nullptr;
  }

  uint8_t x18 = 0;  // +0x18  bit 0 cleared by the ctor
};

}  // namespace mech3::ai
